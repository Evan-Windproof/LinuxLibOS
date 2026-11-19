# Vim Tiny 可行性研究

本研究固定使用 Vim 标签 `v9.1.1230`，提交 `8a63529a2f84bb5069b5f5e38f1704a541d067ad`。上游 Tiny 支持在 Linux 宿主终端编辑和保存文本。将该配置移植到 Liberums 还需要缩小代码映像、重新规划工作内存，并实现 Vim 的平台适配。本阶段未构建 i686 freestanding Vim，也未在 Liberums 上运行 Vim。 [上游标签](https://github.com/vim/vim/tree/8a63529a2f84bb5069b5f5e38f1704a541d067ad)

## 源码构建与宿主实测

在 Debian 13 x86-64 上以 GCC 14.2 构建了固定提交。编译启用 `-Os -g0 -ffunction-sections -fdata-sections`，链接启用 `--gc-sections`。构建需要 Linux C 开发环境和 Tinfo 开发头。此次链接显式选择宿主的动态 `libtinfo.so.6`。最小功能选择仍保留完整编辑主循环、行存储、撤销、终端代码和 Unix 平台层。

```sh
CFLAGS='-Os -g0 -ffunction-sections -fdata-sections' \
LDFLAGS='-Wl,--gc-sections' \
./configure --with-features=tiny --enable-gui=no --without-x \
  --disable-nls --disable-acl --disable-selinux --enable-gpm=no \
  --disable-sysmouse --disable-terminal --disable-channel \
  --disable-netbeans --with-tlib=tinfo
make -j3
strip -o /tmp/vim-tiny.stripped src/vim
size /tmp/vim-tiny.stripped
readelf -lW /tmp/vim-tiny.stripped
readelf -dW /tmp/vim-tiny.stripped
```

| 观测项 | 主机实测 |
|---|---|
| 去符号可执行文件 | 1,303,640 字节，x86-64 ELF PIE |
| `size` 分类 | text 1,147,578，data 149,512，BSS 25,888 字节 |
| `readelf` | PT_INTERP 指向 Linux 动态链接器，链接 libc、libm、libtinfo |
| 交互检查 | 80×25 PTY，`TERM=builtin_ansi` 和 Latin-1 编码；打开 20 字节单行文件，追加 ` Phase 2` 后执行 `:wq!`，退出 0，复核文件 28 字节 |
| 分配观测 | 对同一组目标文件用链接器 `--wrap` 拦截 `malloc/calloc/realloc/strdup/free`，该交互路径的直接分配请求活动峰值 199,793 字节，单次最大请求 65,537 字节 |

测试以 `-Nu NONE -i NONE -n --noplugin` 禁用配置、viminfo、交换和插件，并关闭备份。堆数据是 Tiny 目标文件直接分配请求的观测值，不含共享库内部申请、分配器开销、栈或 RSS，也无法用来预报 i686 瘦身移植后的峰值。该宿主 ELF 约为 Liberums 256 KiB 文件上限的五倍，不能据此推定重构后的 i686 映像大小。 [上游功能配置](https://github.com/vim/vim/blob/8a63529a2f84bb5069b5f5e38f1704a541d067ad/src/configure.ac) · [上游特性开关](https://github.com/vim/vim/blob/8a63529a2f84bb5069b5f5e38f1704a541d067ad/src/feature.h)

## 16 KiB 堆边界

在 i686 上，`screenalloc()` 的单字节布局需要字符、16 位属性和 32 位显示列三组数组。80×25 时包含一行临时行，另有行偏移、回绕和标签页索引，合计 `26 × 80 × (1 + 2 + 4) + 25 × (4 + 1) + 80 × 2 = 14,845` 字节。此数值还未包括窗口行缓存、UTF-8 码点与组合字符数组。 [屏幕分配](https://github.com/vim/vim/blob/8a63529a2f84bb5069b5f5e38f1704a541d067ad/src/screen.c#L2493) · [类型定义](https://github.com/vim/vim/blob/8a63529a2f84bb5069b5f5e38f1704a541d067ad/src/vim.h)

禁用交换文件时，`ml_open()` 仍在内存中分配默认 4,096 字节的文件头块、根指针块和第一个数据块。屏幕数组与三个数据块同时活动时的基本开销是 27,133 字节，另需块头、哈希、缓冲区对象、撤销历史及加载文件所用的临时缓冲。常规读取路径第一次尝试 64 KiB 加一个 NUL 字节，失败后会折半重试。缩小读缓冲仍无法让现有屏幕与行存储布局适配四页堆。 [行存储初始化](https://github.com/vim/vim/blob/8a63529a2f84bb5069b5f5e38f1704a541d067ad/src/memline.c#L282) · [页配置](https://github.com/vim/vim/blob/8a63529a2f84bb5069b5f5e38f1704a541d067ad/src/memfile.c#L64) · [读取缓冲](https://github.com/vim/vim/blob/8a63529a2f84bb5069b5f5e38f1704a541d067ad/src/fileio.c#L1151) · [撤销行副本](https://github.com/vim/vim/blob/8a63529a2f84bb5069b5f5e38f1704a541d067ad/src/undo.c#L356)

## 移植需求与归属

| 范围 | 可行性与缺口 |
|---|---|
| A. 当前可用 | RAM 文件的描述符读写、偏移、元数据、删除和重命名可作为文件后端。TTY 提供 80×25、光标、部分控制序列、原始模式和 ASCII 键盘输入。堆、字符串基本操作和静态入口可作为平台适配基础。 |
| B. 用户态可补 | 先切出单缓冲区、单窗口的纯文本路径，采用 Vim 自带终端能力表的精简配置，核对清屏、定位、反色、滚动、功能键以及光标/终端类型查询。为其所需的 C 库补齐格式化、字符串、必要的流式 I/O 和环境/路径处理。禁用或分别适配交换、备份、权限/时间戳及真实路径行为。修改屏幕和行数据布局、缩小读缓冲并限制撤销历史可减少峰值。具备已知上限的缓冲也可放在映像 BSS，仍需计算 64 页映像预算和 16 KiB 栈。 |
| C. 固定内核边界 | ABI 1.2 的映像范围和外部 ELF 文件上限均为 256 KiB，动态普通页每域 4 页。只有在裁剪后的实际 i686 链接图或最小编辑路径仍不达标时，才有证据要求增大映像、普通页配额或资源窗口。若保留有期限的 `select/poll`、按键超时和休眠等真时间语义，还需可依赖的单调时钟来源。异步信号和真实进程语义也需要相应的事件/调度机制，单应用源码移植可以裁掉这些路径。 |
| D. 暂缓 | GUI、插件与脚本生态、外部命令、作业控制、PTY、进程和信号全兼容、跨重启交换恢复及完整 Unicode 显示。先用固定 ASCII 文件验收输入、保存、重新打开、资源回收和预算。 |

Unix 平台层实际包含 `mch_inchar`、`mch_settmode` 和 `mch_get_shellsize`，并依赖终端属性、大小查询、带时限等待及 SIGWINCH 等信号。文件写入路径还处理备份、权限、替换与 fsync。将这些路径改为有限的 LibOS 语义时应按真实能力报错，避免提供返回成功的空实现。 [Unix 平台层](https://github.com/vim/vim/blob/8a63529a2f84bb5069b5f5e38f1704a541d067ad/src/os_unix.c) · [终端能力表](https://github.com/vim/vim/blob/8a63529a2f84bb5069b5f5e38f1704a541d067ad/src/term.c) · [文件写入](https://github.com/vim/vim/blob/8a63529a2f84bb5069b5f5e38f1704a541d067ad/src/bufwrite.c) · [Liberums 限制](feasibility.md)

如未来分发移植版，须审阅 Vim 许可证的保留原文、修改代码分发途径与修改标识条款。本次仅研究，仓库没有引入 Vim 源码或二进制。 [Vim 许可证](https://github.com/vim/vim/blob/8a63529a2f84bb5069b5f5e38f1704a541d067ad/LICENSE)
