# sbase cat 移植

sbase 是 suckless 的 Unix 可移植工具集。选用其中的 `cat`，因为正文与 I/O 辅助函数可以独立编译，依赖集中于 LinuxLibOS 已提供的文件描述符、读写和错误处理。源码采用 MIT 许可，兼容项目的 GPL-3.0-or-later 组合分发。

| 项目 | 记录 |
|---|---|
| 上游 | [sbase](https://git.suckless.org/sbase/) · [用于固定源码的 GitHub 镜像](https://github.com/michaelforney/sbase/tree/b30fb56804bfed69b45ef0e944d2e029e4d26258) |
| 固定提交 | `b30fb56804bfed69b45ef0e944d2e029e4d26258` |
| 原始程序、辅助函数及手册 | [cat.c](upstream/cat.c) · [concat.c](upstream/libutil/concat.c) · [writeall.c](upstream/libutil/writeall.c) · [cat.1](upstream/cat.1) |
| 许可和完整 SHA256 | [MIT LICENSE](upstream/LICENSE) · [SHA256SUMS](upstream/SHA256SUMS) |
| 移植补丁 | [0001-liberums-port.patch](patches/0001-liberums-port.patch) |

构建先校验原件，再将补丁应用到独立生成目录。补丁保留上游逐个打开文件、分块读到 EOF、循环写出和累积错误状态的流程。兼容层使用现有 `open/read/write/close`、`errno` 和 `strerror`，以标准错误输出替代宿主 stdio 诊断。写调用返回零时报告 `EIO`，写入失败时关闭已打开的输入描述符再返回。

`sbase_cat(argc, argv)` 从 Launcher 接收参数，完成后以成功 0 或失败 1 返回。程序接受一个或多个 RAM 文件名，支持 `-u` 和 `--`。无参数时报告用法，`-` 会报告当前终端缺少 stdin EOF。应用将字节原样写入 stdout，VGA TTY 按当前终端控制规则显示。

复制使用 512 字节栈缓冲区，且不分配堆。GCC 14 的 freestanding i686 构建中，`concat` 的含调用参数栈帧静态上界为 544 字节。应用与 Kilo 复用同一用户域中的 RAM FS、设备授权和终端，Launcher 通过 POSIX 文件路径传递待读文件。
