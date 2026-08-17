# termux-sigsys-shield

[English](README.en.md) | 中文

让被厂商 seccomp 杀死的现代程序,在老内核安卓手机的 Termux 里活下来。

如果你的程序启动即崩,报 `Bad system call`(SIGSYS)—— 比如 Go 编写的 CLI/TUI 工具(opencode、各种 k8s/docker 客户端等)—— 这个工具就是为你准备的。

```
$ opencode
Bad system call            ← 之前

$ oc                       ← 安装本工具后
 █▀▀█ █▀▀█ █▀▀█ ...        ← OpenCode TUI 完整启动
```

## 问题是什么

一些厂商 ROM(电视盒子、低端机、魔改系统的安卓设备)同时具备两个特性:

1. **内核很老**(如 Linux 4.14),不认识新一代系统调用:`pidfd_open`(434)、`clone3`(435)、`close_range`(436)、`openat2`(437)、`rseq`(383),甚至 `statx`(291)
2. **厂商 seccomp 白名单极窄**,调用白名单之外的调用号直接返回 `SECCOMP_RET_TRAP` → 进程收到 SIGSYS → 默认行为是死亡

Go 运行时会主动使用这些新系统调用(官方支持老内核,都带 ENOSYS 回退路径),但它对 SIGSYS 的默认处理是**致命崩溃** —— 于是程序连启动都过不去。

## 怎么解决

`sigsys-shield` 用 ptrace 作为**信号级保镖**:

```
目标程序 ──调用 pidfd_open──► 厂商 seccomp 拦截 ──SIGSYS──► sigsys-shield 截获
                                                              │ 改写寄存器 x0 = -ENOSYS
                                                              │ 吞掉信号,不投递
目标程序 ◄──── syscall 返回 ENOSYS,走老内核回退路径 ────────────┘
```

关键设计:

- **不在系统调用路径上拦截**(在 arm64 4.14 上 seccomp 检查先于 ptrace 停点,调用号改写从原理上无效),只在**信号投递停点**介入 → 几乎零开销
- fork/clone/exec 产生的**所有子孙进程自动受保护**
- 不修改厂商行为,只是把"死刑"翻译成"不支持"

## 快速开始

在 Termux 里:

```bash
pkg install -y git
git clone https://github.com/fzy521/termux-sigsys-shield.git
cd termux-sigsys-shield
bash install.sh
```

使用:

```bash
sigsys-shield <任意命令> [参数...]   # 通用:给任何命令套壳
oc                                   # 若装了 opencode,安装器会创建这个快捷方式
```

## 诊断工具箱

[tools/](tools/) 里有排查这类问题用的三板斧:

| 工具 | 用途 |
|---|---|
| [probe.c](tools/probe.c) | 逐个测试系统调用号是存活还是被 SIGSYS 杀,绘制厂商白名单 |
| [probe_sigsys.c](tools/probe_sigsys.c) | 验证 SIGSYS 是否可捕获(区分 `SECCOMP_RET_TRAP` 与 `RET_KILL`,决定方案可行性) |
| [scan_svc.py](tools/scan_svc.py) | 扫描二进制里的内联系统调用点(`movz w8,#N; svc`),列出全部调用号 |
| [patch_syscall.py](tools/patch_syscall.py) | 把二进制里指定调用号静态补丁为返回 -ENOSYS(免壳方案) |

## 已验证环境

- Linux 4.14.116(aarch64,厂商 ROM)
- opencode 1.17.9(Go,bionic 链接)—— TUI 完整运行
- Termux + `clang`

原理上适用于任何 `SECCOMP_RET_TRAP` 模式的老内核安卓设备。

## 限制与注意

- 若厂商是 `SECCOMP_RET_KILL` 模式(SIGSYS 不可捕获),本方案无效 —— 用 `tools/probe_sigsys.c` 先测
- 目标程序必须对 ENOSYS 有回退(Go 程序普遍满足;自家程序请自查)
- 被保护进程无法再被 strace/gdb 附着(已被 ptrace 占用)
- 老内核 + ptrace:休眠唤醒等极端场景偶发 `waitpid` 竞态,遇到请提 issue

## License

[MIT](LICENSE)
