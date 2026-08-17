/*
 * sigsys-shield: survive vendor seccomp SECCOMP_RET_TRAP SIGSYS kills.
 *
 * This ROM's vendor seccomp filter returns SECCOMP_RET_TRAP for a wide set
 * of syscall numbers (statx 291, rseq 383, pidfd_open 434, clone3 435,
 * close_range 436, ...), delivering a catchable SIGSYS. The Go runtime
 * installs its own SIGSYS handler (raw rt_sigaction syscall -- LD_PRELOAD
 * cannot interpose) that treats it as a fatal "bad system call".
 *
 * Attach as tracer WITHOUT PTRACE_SYSCALL: we only intervene at
 * signal-delivery stops. When SIGSYS (si_code == SYS_SECCOMP) stops the
 * tracee, we set x0 = -ENOSYS and suppress the signal: the aborted syscall
 * reports ENOSYS and every caller falls back to the old-kernel path.
 * All other signals are forwarded untouched. Overhead is one ptrace stop
 * per real signal -- negligible for a TUI app.
 */
#define _GNU_SOURCE
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#ifndef PTRACE_GETREGSET
#define PTRACE_GETREGSET 0x4204
#define PTRACE_SETREGSET 0x4205
#endif
#ifndef NT_PRSTATUS
#define NT_PRSTATUS 1
#endif

struct user_pt_regs_x { unsigned long regs[31], sp, pc, pstate; };

#define OPTS (PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK | \
             PTRACE_O_TRACEVFORK | PTRACE_O_TRACEEXEC | PTRACE_O_EXITKILL)

static void fake_enosys(pid_t pid)
{
    struct user_pt_regs_x r;
    struct iovec iov = { &r, sizeof r };
    if (ptrace(PTRACE_GETREGSET, pid, (void *)NT_PRSTATUS, &iov) == 0) {
        r.regs[0] = (unsigned long)-38;   /* x0 = -ENOSYS */
        ptrace(PTRACE_SETREGSET, pid, (void *)NT_PRSTATUS, &iov);
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: sigsys-shield prog [args...]\n");
        return 2;
    }

    pid_t child = fork();
    if (child == 0) {
        ptrace(PTRACE_TRACEME, 0, 0, 0);
        raise(SIGSTOP);
        execv(argv[1], argv + 1);
        perror("execv");
        _exit(127);
    }

    int st;
    waitpid(child, &st, 0);                        /* initial SIGSTOP */
    ptrace(PTRACE_SETOPTIONS, child, 0, (void *)OPTS);
    ptrace(PTRACE_CONT, child, 0, 0);

    for (;;) {
        pid_t pid = waitpid(-1, &st, __WALL);
        if (pid < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (WIFEXITED(st) || WIFSIGNALED(st)) {
            if (pid == child)
                return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
            continue;
        }
        if (!WIFSTOPPED(st)) continue;

        unsigned event = (unsigned)st >> 16;
        int sig = WSTOPSIG(st);

        if (event) {                               /* fork/clone/exec/exit events */
            ptrace(PTRACE_CONT, pid, 0, 0);
            continue;
        }

        if (sig == SIGTRAP || sig == SIGSTOP) {    /* attach/group stops */
            ptrace(PTRACE_CONT, pid, 0, 0);
            continue;
        }

        siginfo_t si;
        if (ptrace(PTRACE_GETSIGINFO, pid, 0, &si) == 0) {
            if (sig == SIGSYS) {
                fake_enosys(pid);                  /* swallow, fake -ENOSYS */
                ptrace(PTRACE_CONT, pid, 0, 0);
            } else {
                ptrace(PTRACE_CONT, pid, 0, (void *)(long)sig); /* forward */
            }
        } else {
            ptrace(PTRACE_CONT, pid, 0, 0);        /* group-stop: swallow */
        }
    }
    return 0;
}
