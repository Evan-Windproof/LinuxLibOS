# Phase 3.2 验证

基线为 LinuxLibOS `360d0de186c1e81bca339402d1897e6ea3749208`。Liberums v1.0.0 提交 `7d5d574e8643c3645fa3ce2a126dcbd4ae43662f` 的内核 ELF 在测试前后均为 SHA256 `03ba69356bbbd607e310952e68f72cf4d8019c0fd1f8b77373177c3602f218c4`。工具为 GCC 14.2、GNU Binutils 2.44、QEMU 10.0.13 和 GDB 16.3。使用 i686 TCG、32 MiB、单 CPU。

## 两个真实输入

| 输入 | 字节与 SHA256 | 串口客体输出 | 字节数、步数、退出码 |
|---|---|---|---|
| `build/linux-sample.elf` | 8,496，`933a6cb6eb3836fdb480f4e40f13b4458509bff7307ed30ad65c4b2a9c3d6d0d`，与 Phase 3.1 记录相同 | `Hello from Linux i386` 加换行 | 22，8，0 |
| `build/linux-loop.elf` | 8,392，`37071b25e6b0437e62d26e2b11777fa4033e0db59a5bc27e70da627b3b016a0b` | `Loop result: 8` 加换行 | 15，29，37 |

GNU `as --32` 和 `ld -m elf_i386 -static -nostdlib --build-id=none -s` 独立生成两个 ELF。GDB 逐字节比较磁盘原件和容器嵌入内容。两个客体 ELF 及内核的 SHA256 在干净重建后再次匹配。Liberums 拒绝直接装载额外提交的两个原始 boot module（`-ENOEXEC`）。原生容器都在 15 个映像页内，测试未为解释器申请动态 RAM 或设备。

## 执行路径

两个样本都从客体 EIP `0x08049000` 开始。首条 MOV 完成后 EIP 增加 5，目标寄存器为 4。第二个样本第一次循环跳转后位置为 `0x0804900a`，ECX 为 3，EDX 为 `0x0a32`。它运行四次循环，将算出的字符和换行写进原先为零的客体数据，并用 write 返回值选择退出分支。

在两次 write 停点，原生 CPL 为 3，CS `0x1b`，CR3 `0x14d000`，原生 EIP `0x080013c9`。客体 ESP 为 `0xbfffffb0`，写入时客体 EAX 为 4，EIP 分别为 `0x08049014` 与 `0x08049030`。从 ECX 指向的客体内存读到表中完整输出。下一条指令处 EAX 分别返回 22 与 15。exit 的客体 EIP 分别为 `0x08049021` 和 `0x0804904a`，状态为 0 与 37。随后原生调用者以状态 0 独立退出，内核回收域。

从进入解释器到返回，GDB 追踪原生内核 `syscall_dispatch`，只见 `REVOKE_POLL`（24）和 `LOG_CHAR`（2）。原生日志调用的字节参数与客体输出逐字节一致。客体地址没有硬件 PTE，设备和动态资源表为空。

## 回归与复现

- `make clean`、`make`、`make test` 全部通过。宿主 UBSan 检查包含算术状态、坏编码、未知中断、跨段指令、未映射地址、NX、只读段写入、栈边界、无效 syscall 指针、短写、`ENOSYS` 和 4096 步上限。
- `make linux-binary-tests` 的三项 QEMU/GDB 验收通过。一个独立原生测试模块在 Ring 3 运行相同的解释器负例。
- `make integration` 共 15 项通过，包含 Phase 3.1 原有装载与内存检查，以及 Phase 1/2 的 Kilo、sbase cat、Launcher、设备授权、撤销和回收。

`build/linux-sample.txt` 和 `build/linux-loop.txt` 记录 ELF 头、SHA256 和反汇编。`build/integration/results.json`、`linux-exec*.serial.log` 和 `linux-exec*.gdb.log` 保存完整运行证据。
