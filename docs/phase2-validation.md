# Phase 2 验证

基准为 LinuxLibOS `ff73978130997fd5b8fa33cb9e31dc18d2ee7c93` 和 Liberums `7d5d574e8643c3645fa3ce2a126dcbd4ae43662f`。实测内核 ELF SHA256 为 `03ba69356bbbd607e310952e68f72cf4d8019c0fd1f8b77373177c3602f218c4`，测试前后一致。

- GCC 15.3.1 在原 Phase 1 复现了数组初始化报错。修订后 GCC 14.2.0 和 GCC 15.3.1 均通过 `make clean`、`make` 和 `make test`，包含重新应用补丁、校验原件和检查 ELF32 布局。
- QEMU 10.0.13 / GDB 16.3 下 `make integration` 全部 11 个场景通过。包括历史 ABI、映射、授权、回收、撤销和域隔离，以及 RAM FS 新增语义、独立 Kilo 结束画面、长行保存失败和 Launcher 循环。
- Phase 1 中，干净文件一次 Ctrl-Q 即退出，未保存文件需要四次。两条路径最终以状态 0 回收用户域，释放 VGA 后保留最后一帧。Phase 2 直接启动在释放 VGA 前显示结束画面，Launcher 内回到菜单。
- Kilo 保存后，sbase `cat` 两次输出完全匹配的 37 字节。五次应用返回后验证活动堆为零、没有残留应用描述符、终端模式恢复，设备代次和 CR3 保持一致。
- 16 KiB 堆的活动申请峰值为普通会话 142 字节、长行失败保存 12,331 字节。4095 个 Tab 的样本请求 36,856 字节并以 71 退出，原文件 4096 字节回读一致。纯文本满容量文件再插入一字节时，保存返回 ENOSPC，原文件保持不变。
- GCC 14 构建的独立编辑器为 114,864 字节和 11 个映像页，Launcher 为 125,324 字节和 12 个映像页，均含调试信息。Phase 2 GTK 实体键盘人工回归尚未执行。

集成命令为 `make integration LIBERUMS=/absolute/path/to/Liberums`。结果在 `build/integration/results.json`，同目录的 `*.serial.log`、`*.gdb.log` 和 `*.vga.txt` 包含串口、调试器和结束画面证据。 [Phase 1 历史验收](validation.md)
