# 用户态终端

可信启动参数 `multi-external multi-vga=1 multi-keyboard=1` 授权给 QEMU Multiboot 模块 1。两个设备分别经过 INFO、ACQUIRE；VGA 映射到 `0x09400000`。如果只获得 VGA 而键盘被拒绝，初始化回滚 VGA 映射和句柄。VGA 区域独立于普通页池，用户调用 SDK，不执行端口 I/O。

PS/2 解码处理 translated set 1 的 make/break、E0 扩展键、双 Shift/Ctrl、Caps Lock、US ASCII、Enter、Backspace 和 Esc。方向键生成 VT100 字节序列，即使底层按字节读取也可区分独立 Esc 和同次生成的连续字节。输入队列溢出返回 EOVERFLOW 并丢弃该批次、清空内部字节和修饰键状态；Kilo 报告错误并继续等待。无需墙钟超时。

默认 canonical 模式收集换行，处理退格和回显；一行最多 127 字节正文加换行。raw-like 模式阻塞等待至少一个字节，支持方向键序列和不回显。无作业控制、信号键、非 US 键盘布局或输入法。

输出支持 CR/LF、退格、Tab、滚动、光标绝对与相对移动、清屏/清行、默认色/基本前景色和反色。连续 write 可跨调用传递部分 CSI。ABI 只有光标位置操作，Kilo 的隐藏/显示光标序列会被解析但无法更改硬件可见性。没有终端响应序列或完整 VT100 状态机。
