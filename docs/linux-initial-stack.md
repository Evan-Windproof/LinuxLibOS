# Linux i386 初始栈

虚拟 ESP 按 16 字节对齐。栈头由连续的小端 32 位字组成，按地址递增依次为：

| 顺序 | 内容 |
|---|---|
| 1 | `argc` |
| 2 | `argv[0] ... argv[argc-1]` 及 NULL |
| 3 | `envp[0] ... envp[envc-1]` 及 NULL |
| 4 | auxv 类型和值的成对字，最后是 `AT_NULL, 0` |

参数和环境值均为栈顶部 NUL 结尾字符串的客体地址。最多八个参数和八个环境项，每字符串最多 256 字节（含 NUL）。构造前检查字符串和表能否同时放入 4 KiB 栈。

auxv 提供 `AT_PAGESZ=4096` 与 `AT_ENTRY`。当可读 LOAD 完整覆盖程序头表时，加入其客体地址 `AT_PHDR`、条目大小 `AT_PHENT` 和条目数 `AT_PHNUM`。其余条目省略，包括 `AT_RANDOM`、vDSO、解释器地址和平台身份。

虚拟 EIP 取 ELF 入口，通用寄存器初值为 0（含静态程序的 EDX），虚拟 EFLAGS 为 `0x202`，ESP 指向 argc。整个状态保存在用户态结构中。加载器不会将这些值写入硬件寄存器并转入客体代码。 [Linux ELF 装载与栈构造](https://github.com/torvalds/linux/blob/v6.12/fs/binfmt_elf.c) · [Linux auxv](https://man7.org/linux/man-pages/man3/getauxval.3.html)
