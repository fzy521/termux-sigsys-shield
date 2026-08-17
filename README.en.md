# termux-sigsys-shield

English | [中文](README.md)

Keep modern programs alive in Termux on old-kernel Android devices, when the
vendor seccomp policy would otherwise kill them.

If your program dies at startup with `Bad system call` (SIGSYS) — typically
Go CLI/TUI tools (opencode, k8s/docker clients, and friends) — this project
is for you.

```
$ opencode
Bad system call            ← before

$ oc                       ← after installing this
 █▀▀█ █▀▀█ █▀▀█ ...        ← OpenCode TUI fully rendered
```

## The problem

Some vendor ROMs (TV boxes, budget devices, heavily customized systems)
combine two traits:

1. **An old kernel** (e.g. Linux 4.14) that predates the new generation of
   syscalls: `pidfd_open` (434), `clone3` (435), `close_range` (436),
   `openat2` (437), `rseq` (383), and even `statx` (291)
2. **A narrow vendor seccomp whitelist**: any number outside it returns
   `SECCOMP_RET_TRAP` → SIGSYS → default disposition kills the process

The Go runtime happily uses these newer syscalls (it officially supports old
kernels and carries ENOSYS fallbacks for all of them), but its default
handling of SIGSYS is **fatal** — so the program never gets to start.

## The solution

`sigsys-shield` acts as a ptrace-based **signal-level bodyguard**:

```
target ──calls pidfd_open──► vendor seccomp traps ──SIGSYS──► shield intercepts
                                                                │ rewrite x0 = -ENOSYS
                                                                │ swallow the signal
target ◄──── syscall returns ENOSYS → old-kernel fallback path ─┘
```

Key design points:

- **No interception on the syscall path** (on arm64 4.14 the seccomp check
  runs *before* the ptrace syscall stop, so rewriting the number can never
  work) — it only intervenes at signal-delivery stops → near-zero overhead
- All descendants via fork/clone/exec are **automatically protected**
- The vendor behavior is untouched; the shield merely translates "death
  sentence" into "not supported"

## Quick start

Inside Termux:

```bash
pkg install -y git
git clone https://github.com/fzy521/termux-sigsys-shield.git
cd termux-sigsys-shield
bash install.sh
```

Usage:

```bash
sigsys-shield <any-command> [args...]  # generic wrapper
oc                                     # opencode shortcut (created by the installer)
```

## Diagnostic toolbox

[tools/](tools/) contains the instruments used to hunt this class of bugs:

| Tool | Purpose |
|---|---|
| [probe.c](tools/probe.c) | Test syscall numbers one by one — alive or SIGSYS-killed — to map the vendor whitelist |
| [probe_sigsys.c](tools/probe_sigsys.c) | Check whether SIGSYS is catchable (distinguishes `SECCOMP_RET_TRAP` from `RET_KILL`, which decides feasibility) |
| [scan_svc.py](tools/scan_svc.py) | Scan a binary for inline syscall sites (`movz w8,#N; svc`) and list every number |
| [patch_syscall.py](tools/patch_syscall.py) | Statically patch given syscall numbers in a binary to return -ENOSYS (wrapper-free alternative) |

## Verified on

- Linux 4.14.116 (aarch64, vendor ROM)
- opencode 1.17.9 (Go, bionic-linked) — TUI runs fully
- Termux + `clang`

In principle this works on any Android device whose vendor seccomp uses
`SECCOMP_RET_TRAP`.

## Limitations

- If the vendor uses `SECCOMP_RET_KILL` (uncatchable SIGSYS), this approach
  cannot work — test with `tools/probe_sigsys.c` first
- The target program must fall back on ENOSYS (true for most Go programs;
  check your own code)
- Shielded processes cannot be attached by strace/gdb (ptrace is taken)
- Old kernel + ptrace: rare `waitpid` races after suspend/resume — file an
  issue if you hit one

## License

[MIT](LICENSE)
