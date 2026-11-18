# POSIX 风格接口

该接口面向重新编译的单域 C 程序。结构、宏和函数签名的覆盖范围见 [头文件](../include/posix.h)，与 Linux libc 的二进制布局独立。

| 接口 | 支持的语义 |
|---|---|
| `open` | 平坦名字，绝对和相对单名指向同一文件；支持 RDONLY/WRONLY/RDWR、CREAT/EXCL/TRUNC/APPEND；创建 mode 参数不参与权限判断 |
| `read` | 文件按描述符偏移读取，EOF 返回 0；读到文件末尾可以短读。stdin 由 TTY 提供 |
| `write` | 文件按偏移写或追加；越过 EOF 时零填充；容量边界允许短写，已满则 ENOSPC。stdout/stderr 共用 VGA |
| `close` | 释放描述符槽，文件仍存在。关闭后的描述符可重新分配 |
| `lseek` | 32 位有符号偏移，支持 SET/CUR/END，可超过 EOF；拒绝负位置和整数溢出 |
| `fstat` | 返回大小与常规文件/字符设备类型 |
| `ftruncate` | 只接受可写 RAM 文件，支持缩小及零填充扩展，保留已打开描述符的偏移 |
| `isatty` | 仅识别打开且完成设备授权的 0/1/2 |
| `ioctl` | TIOCGWINSZ 返回 80×25；其余请求返回 ENOTTY |
| `tcgetattr/tcsetattr` | 固定 CS8，ICRNL/OPOST/ICANON/ECHO、TCSANOW/TCSAFLUSH；VMIN=1、VTIME=0 |

错误返回 `-1` 并写入单域 `errno`，`isatty` 用 0 表示失败。主要错误为 EBADF（无效描述符或不符访问方向）、EINVAL（参数/flags/模式无效）、ENOENT（无对应文件）、EEXIST（排他创建冲突）、EMFILE（描述符用尽）、ENOSPC（文件槽或容量用尽）、ESPIPE（终端 seek）、ENOTTY（无法使用终端）、EOVERFLOW（偏移越界或键盘溢出）和 EPERM（授权失败）。若 TTY 有未完成输入，直接切换 canonical 模式返回 EBUSY，可用 TCSAFLUSH 清输入后再切换。

范围限制包括 4 个文件、每文件 4096 字节和 9 个文件描述符。没有目录遍历、unlink、权限位执行、文件描述符复制、多线程 errno、信号、管道、设备节点、`gettimeofday` 或 `clock_gettime`。C 支持包括内存、ASCII 字符、少量字符串操作和动态分配。Kilo 的私有格式化只使用 `%d`、`%s`、带精度的 `%.Ns` 和 `%%`。
