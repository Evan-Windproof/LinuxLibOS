# Phase 3.1 可行性审查

基线为 LinuxLibOS `74751f243e098c91253a84a1bd8804263d915591` 与 Liberums v1.0.0 `7d5d574e8643c3645fa3ce2a126dcbd4ae43662f`。公开 ABI 为 1.2。以下边界来自固定内核的公共 ELF 和资源 ABI，以及现有 LibOS 链接脚本、启动代码、堆、RAM FS 和 Phase 2 验收结果。 [公共 ELF ABI](../sdk/include/abi/elf.h) · [公共资源 ABI](../sdk/include/abi/resource.h) · [Phase 2 验收](phase2-validation.md)

| 资源 | Liberums 限制 | Phase 3.1 用法 |
|---|---|---|
| 映像 | `[0x08000000, 0x08040000)`，最多 64 个 4 KiB 页，原生 ELF 文件最多 256 KiB | 原生加载器的 RX、R、RW 段与 36 字节 ABI Note 使用原有规则 |
| 用户栈 | `[0x083fc000, 0x08400000)`，16 KiB | C 函数与小型加载计划使用原生栈。客体栈单独保存在原生 BSS |
| 普通动态 RAM | 每域四页，共 16 KiB，可映射到 `[0x09000000, 0x09010000)` | 新加载器无需申请动态页 |
| 设备 | VGA 与 PS/2 独立授权 | 使用原有短串口日志，不申请或映射终端设备 |
| RAM FS | 当前单文件上限 4096 字节 | 演示原样嵌入独立构建的 ELF，复用缓冲区装载接口，无需改变 RAM FS 容量 |
| 进程与链接 | 无通用 spawn、fork/exec、Linux 地址空间或动态链接器 | 在一个现有 Ring 3 域内保存客体地址、映像、栈和寄存器 |

外层是 Liberums 对 LinuxLibOS 自身 ELF 的装载，负责原生页与权限。内层是 LinuxLibOS 对 Linux ET_EXEC 内容的解析和拷贝，维护按段标注 R/W/X 的客体虚拟地址空间。客体地址是数字索引，原生页表没有对应映射。计划使用 32 KiB 紧凑段存储和 4 KiB 客体栈。段之间的地址空洞无需预留原生内存。每次访问受段范围及相应权限约束。

Linux i386 与 Liberums 都使用 `int 0x80`，但编号、参数与内核处理规则不同。直接跳转到 Linux ELF 指令会绕过客体权限检查，遇到 `int 0x80` 还会进入 Liberums ABI。Phase 3.1 只从客体 EIP 做受控取指并报告未实现状态。i386 指令语义、Linux syscall 翻译和执行验收属于后续工作。 [Linux ABI 对比](linux-binary-compatibility-feasibility.md)
