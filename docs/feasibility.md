# Phase 1 可行性审查

基准为 [Liberums v1.0.0](https://github.com/Evan-Windproof/Liberums/tree/7d5d574e8643c3645fa3ce2a126dcbd4ae43662f)，annotated tag 对象 `444dbe54ef59c61f828cfc425b97d5b15721216d` 指向 `7d5d574e8643c3645fa3ce2a126dcbd4ae43662f`。审查依据包括 release README、`include/abi/`、`docs/public-abi.md`、`docs/phase9-abi-contract.md`、`docs/known-limitations.md`、`docs/libos-development/`、`examples/libos/` 和 `docs/release-validation.md`。Aelthuvor v1.0.0 标签解析到 `a10ccdab9a662a34907ec97e8af0fb398a8d90a9`。本项目只消费 Kernel SDK。

| 资源 | 固定内核边界 | Phase 1 方案 |
|---|---|---|
| ELF 文件 | ELF32 little-endian、ET_EXEC、EM_386；外部文件 ≤ 0x40000 字节，至多 8 个 LOAD 段，静态链接 | RX、R、RW 分页对齐，R 段包含 36 字节 ABI Note；单独检查磁盘大小 |
| 映像含 BSS | `[0x08000000,0x08040000)`，最多 64 个映像页 | 代码/只读数据约 16–32 KiB，BSS 约 20 KiB，其中 RAM FS 4 × 4096 字节；以实测 ELF 为准 |
| 用户栈 | `[0x083fc000,0x08400000)`，另计 16 KiB | i386 入口对齐栈，避免大型局部数组和无界递归 |
| 普通 RAM | 每域 4 × 4096 字节，全局 16 页；每资源最多 4 个绑定；窗口 `[0x09000000,0x09010000)` | 映射 4 个连续 VA 页做堆，页句柄存于 BSS，检查映射回滚；RAM 文件不占动态页 |
| 抢占与撤销 | 1.2 抢占 feature mask 119，用户 IF=1，IRQ0 驱动；撤销可异步移除 PTE | 使用抢占 profile；安全点检测撤销并终止会话；强制撤销时旧指针可能触发真实 #PF |
| VGA | 80×25，4000 字节，设备映射页 4096 字节，`0x09400000`；光标调用只支持位置 | LibOS 实现文本与 ANSI 子集，独占授权来自可信启动，隐藏光标序列不改变硬件可见性 |
| PS/2 | 64 字节 translated set 1 队列，POLL 返回原始字节；overflow 丢批 | LibOS 解码 US ASCII、Shift/Ctrl、扩展键；溢出时重置状态并报告 |
| 时钟、文件、进程 | 无公开时钟读取、磁盘、文件系统、fork/exec、信号、动态链接器、通用 syscall 委派 | 使用同域 RAM FS、固定窗口尺寸、阻塞读取和应用定义的 Escape 边界；移除 Kilo 墙钟及 SIGWINCH 依赖 |

Kilo 需要独立字符串/分配支持、打开/读取/保存/截断文件、原始终端输入、ANSI 输出和固定窗口尺寸。选定子集为 `read/write/open/close/lseek/fstat/ftruncate/isatty/ioctl/tcgetattr/tcsetattr`。上游的 stdio/行读取、时间、信号和终端探测回退可通过可审阅补丁替换；限定为小型 ASCII 文本实验。堆同时承担行描述、字符、展开文本、高亮、搜索和序列化副本；大文件、多行、密集 Tab 和碎片会先耗尽 16 KiB 堆，4096 字节是单文件容量上限，不能保证任意内容均能在编辑器中加载。另需限制高亮递归并测量文本编辑路径。

预期无内核改动即可验证同域 RAM 文件的编辑与保存。持久性、时钟、真实 Linux 进程入口和 Linux syscall 兼容均缺乏现成机制。设备发现和 ABI Note 均不授予资源，设备授权分别由可信 Multiboot 配置赋予模块序号。
