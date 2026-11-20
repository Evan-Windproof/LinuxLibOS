# 已知限制

- 应用共用同一用户域，静态入口调用没有应用之间的内存隔离、PID、fork、exec 或动态加载。
- RAM FS 只支持根下平坦文件名。最多四个文件对象、每对象 4096 字节和九个文件描述符槽。打开但已解除链接的对象占用文件槽，直到最后一个描述符关闭。域退出或虚拟机重启后数据消失。
- 16 KiB 堆仍会限制大量短行、密集 Tab 或碎片化增长。文件容量与编辑堆容量分别受限。
- 终端为 80×25 VGA 与 US ASCII PS/2 子集。没有终端 EOF 键、信号、作业控制、PTY 或完整 VT100。Launcher 中的 sbase `cat` 查看固定路径。
- 当前交付验证使用 QMP 模拟 PS/2。Phase 2 GTK 与实体键盘未做人工回归。
- Linux 客体装载仅支持受限静态 i386 ET_EXEC。段存储为 32 KiB、栈为 4 KiB、文件上限为 32 KiB。虚拟指令预算为 4096 步，支持少数整数、栈和控制流编码，以及 Linux `write`/ `exit`。演示的 stdout 使用有配额的 COM1 文本日志。 [执行边界](linux-i386-execution.md)
- Vim Tiny、BusyBox 和 glibc 程序目前无法运行。没有硬件直接执行、动态链接、通用 Linux ABI 或 Linux 进程创建。
