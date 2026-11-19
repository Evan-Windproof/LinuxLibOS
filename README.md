# LinuxLibOS

LinuxLibOS 是基于 Liberums v1.0.0 公共 Kernel SDK 1.2 的独立 i686 Ring 3 Library OS。Phase 1 建立堆、RAM 文件系统、文件描述符和 VGA/PS/2 终端，并移植 Kilo。Phase 2 增加同一用户域内的静态应用选择器和真实上游 sbase `cat`，使 Kilo 保存的 RAM 文件可以由另一个应用读出。

## 构建与使用

需要 GCC 的 freestanding i686 支持、GNU Make 4.3+、GNU Binutils、GNU Patch、Git 和 Python 3。集成测试需要 QEMU i386 和带 Python 的 GDB。GTK 运行入口需要桌面会话及 QEMU 的 GTK 后端。实测版本见 [Phase 2 验证](docs/phase2-validation.md)。

```sh
make                  # 构建 hello、独立 Kilo 和 Launcher ELF
make run               # 保留原入口，直接启动 Kilo
make run-launcher      # GTK 应用选择器
make test              # 宿主 RAM FS 测试、SDK/上游哈希和 ELF 布局
make integration       # QEMU TCG + GDB + QMP 模拟 PS/2
make debug DISPLAY_BACKEND=none
make clean
```

内核运行目标会下载并验证固定版本。可设置 `LIBERUMS=/absolute/path` 复用干净的指定 checkout，也可使用 `QEMU=/path/qemu-system-i386 GDB=/path/gdb`。调试时用 GDB 加载内核 ELF，执行 `target remote :1234`，再用 `add-symbol-file build/editor.elf` 加载用户程序符号。

Kilo 默认编辑 `/hello.txt`。Ctrl-S 保存，Ctrl-F 搜索，Ctrl-Q 退出。有未保存修改时，首次 Ctrl-Q 提示再按三次。Launcher 输入 `1` 至 `4` 并按 Enter，依次选择 Kilo、`cat`、文件信息或退出。Kilo 退出后返回菜单。文件查看结束后按 Enter 返回。直接运行时，Kilo 退出后显示会话和 RAM 回读结果。域退出后 QEMU 窗口保持打开。

## 接口与范围

支持 `open/close/read/write/lseek/fstat/ftruncate/stat/unlink/access/rename` 与 `isatty/ioctl/tcgetattr/tcsetattr`。RAM FS 有四个文件对象槽，每文件最多 4096 字节，内容随用户域结束而消失。应用切换共享文件、TTY 和域资源，使用静态入口函数。尚无 Linux 进程创建、原生 Linux ELF32 运行或持久文件系统。 [POSIX 子集](docs/posix-subset.md) · [兼容性矩阵](docs/compatibility-matrix.md) · [已知限制](docs/known-limitations.md)

项目代码与 SDK 为 GPL-3.0-or-later。Kilo 上游为 BSD-2-Clause，sbase `cat` 上游为 MIT。 [Kilo 源码与补丁](ports/kilo/README.md) · [sbase 源码与补丁](ports/sbase-cat/README.md)
