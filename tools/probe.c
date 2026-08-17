/*
 * probe: raw syscall survival tester.
 *
 * Calls syscall(nr, 0,0,0,0,0,0) directly. On vendor ROMs with strict
 * seccomp this either returns normally or the process is killed with
 * SIGSYS ("Bad system call", shell exit code 159).
 *
 * Build:  clang -O1 -o probe probe.c
 * Usage:  ./probe 434
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: probe SYSCALL_NR\n");
        return 2;
    }
    long nr = atol(argv[1]);
    long r = syscall(nr, 0, 0, 0, 0, 0, 0);
    printf("nr=%ld ret=%ld errno=%d(%s)\n", nr, r, errno,
           errno == ENOSYS ? "ENOSYS" : "other");
    return 0;
}
