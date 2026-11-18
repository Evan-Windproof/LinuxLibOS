# Linux Compatibility LibOS

Phase 1 在不变更 Liberums v1.0.0 的情况下，用独立 Ring 3 Library OS 运行重新编译的终端应用。Liberums ABI 1.2 保护 CPU、内存和设备资源。用户态 LibOS 实现堆、文件描述符、RAM 文件系统和终端策略。项目只依赖公开 Kernel SDK；Aelthuvor 是另一个独立 LibOS。

支持的软件为 [Kilo](ports/kilo/README.md) 的小型纯文本移植版。支持部分 POSIX 风格接口，以及从真实上游源码重新编译运行。未实现未修改的 Linux ELF32 装载与 Linux 系统调用兼容。各项能力与证据见 [兼容性矩阵](docs/compatibility-matrix.md)。

## 构建与运行

需要 GNU Make、GCC 的 freestanding i686 支持、GNU Binutils、Python 3、GNU Patch 和 Git。集成验收需要 QEMU i386 与支持 Python 的 GDB。本地交互还需要 QEMU GTK 后端和桌面会话。

```sh
make                  # 构建 build/hello.elf 与 build/editor.elf
make test             # 宿主 RAM FS、SDK/上游文件校验、ELF 布局
make integration      # 32 MiB / 1 CPU 的 QEMU TCG，含 QMP 模拟 PS/2 和 GDB
make run              # 本地 GTK，以可信启动参数授权 VGA 和键盘
make clean            # 删除生成文件与默认下载的内核 checkout
```

`make integration` 和 `make run` 默认下载并检查固定内核提交。可用 `LIBERUMS=/absolute/path` 指定已有的干净 checkout。设置 `QEMU=/path/qemu-system-i386 GDB=/path/gdb` 可以选择调试工具。

Kilo 默认打开 `/hello.txt`。方向键移动，Ctrl-S 保存，Ctrl-F 搜索，Ctrl-Q 退出。退出后，LibOS 在同一域内重新打开并读取 RAM 文件，再归还资源。域退出后 QEMU 保持运行，关闭窗口或结束 QEMU 即可。RAM 文件仅在该域的运行期间有效。

`make debug DISPLAY_BACKEND=none` 以暂停状态启动 QEMU。另一个终端运行 `gdb build/liberums/build/liberums.elf`，然后执行 `target remote :1234` 和 `add-symbol-file build/editor.elf`。在 `libos_observe` 设置硬件断点可检查 ABI、用户栈与退出前的回读结果。自动验收及限制见 [验证报告](docs/validation.md)。

## 接口和限制

文件接口为 `open/close/read/write/lseek/fstat/ftruncate`，终端接口为 `isatty/ioctl/tcgetattr/tcsetattr`。定长 RAM FS、16 KiB 动态堆、80×25 VGA、US ASCII 键盘；详细语义见 [POSIX 子集](docs/posix-subset.md) 和 [终端](docs/terminal.md)。

项目代码、Liberums SDK 和链接脚本等采用 GPL-3.0-or-later，Kilo 上游文件保留 BSD-2-Clause 许可。源码来源、固定提交和修改摘要见 [Kilo 移植说明](ports/kilo/README.md)。内核 ABI 可行性审查及 Linux i386 二进制研究见 [Phase 1 可行性审查](docs/feasibility.md) 和 [Linux ELF32 研究](docs/linux-binary-compatibility-feasibility.md)。
