#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>

static void handler(int sig, siginfo_t *si, void *ctx)
{
    char buf[256];
    int n = snprintf(buf, sizeof buf,
                     "  [handler] SIGSYS caught: si_code=%d si_syscall=%d si_call_addr=%p\n",
                     si->si_code, si->si_syscall, si->si_call_addr);
    write(1, buf, n);
    /* plain return: kernel gives the syscall -ENOSYS return (SECCOMP_RET_TRAP) */
}

int main(int argc, char **argv)
{
    long nr = atol(argv[1]);
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGSYS, &sa, NULL) != 0) { perror("sigaction"); return 2; }

    long r = syscall(nr, 0, 0, 0, 0, 0, 0);
    printf("nr=%ld ret=%ld errno=%d (%s)\n", nr, r, errno, strerror(errno));
    printf("SURVIVED\n");
    return 0;
}
