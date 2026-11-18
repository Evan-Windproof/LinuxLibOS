# RAM 文件系统

4 个槽保存在 ELF BSS；每个文件容量为 4096 字节，名字最多 63 字节。可以使用 `file` 或 `/file`。同一文件多次打开时有各自的读写偏移。稀疏写和截断扩展的间隙清零，超过容量时允许部分写入。满槽和满文件分别按 ENOSPC 处理，打开失败不会先创建文件。

集成用例在一个域内建立 `/hello.txt`，初始文本为 `Hello from Liberums\n`。QMP 通过模拟 PS/2 键盘编辑文本，Kilo 执行截断、写入与关闭。退出编辑器回到 LibOS 后，重新 open/read/close；GDB 从用户态回读缓冲区逐字节校验 `Hello from Linux Compatibility LibOS\n`。文件保留到域结束；重启或域回收不保留数据。
