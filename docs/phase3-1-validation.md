# Phase 3.1 验证

LinuxLibOS 基线 `74751f243e098c91253a84a1bd8804263d915591`。固定 Liberums v1.0.0 提交 `7d5d574e8643c3645fa3ce2a126dcbd4ae43662f` 的 ELF 在测试前后 SHA256 一致，为 `03ba69356bbbd607e310952e68f72cf4d8019c0fd1f8b77373177c3602f218c4`。工具为 GCC 14.2、GNU Binutils 2.44、QEMU 10.0.13 与 GDB 16.3。QEMU 采用 i686 TCG、32 MiB、单 CPU。

## 样本

通过以下命令生成 Linux ELF。测试读取构建原件，演示对这份文件执行 `.incbin`。原件未被重写，也未重链接到 Liberums SDK 或运行时。

```sh
as --32 tests/fixtures/linux-sample.S -o build/linux-sample.o
ld -m elf_i386 -static -nostdlib --build-id=none -s build/linux-sample.o -o build/linux-sample.elf
```

生成文件 8496 字节，SHA256 为 `933a6cb6eb3836fdb480f4e40f13b4458509bff7307ed30ad65c4b2a9c3d6d0d`。ELF32，小端，System V，EM_386，ET_EXEC，入口 `0x08049000`，程序头偏移 52，五条程序头。四条 LOAD 合计 375 字节客体存储，其中 102 字节由装载器零填充。第五条为 GNU_STACK RW。无动态段、PT_INTERP 或 Note。反汇编显示从 `mov $0x4,%eax` 开始，分别对 Linux write（4）和 exit（1）编码 `cd 80`。完整头部和反汇编见 `build/linux-sample.txt`。

## 结果

| 检查 | 实测 |
|---|---|
| `make clean`、`make`、`make test` | 通过。验证 SDK、第三方哈希、宿主 RAM FS、ELF32 布局和 Linux 装载器宿主测试 |
| 宿主装载器测试 | 真实 ELF 的全部 LOAD 文件内容与 BSS、R/W/X、边界、重新装载、初始栈。错误输入覆盖 Magic、ELF64、字节序、机器类型、ET_DYN、PT_INTERP、PT_DYNAMIC、程序头与段越界、整数边界、对齐、重叠、非法入口、配额与栈容量 |
| QEMU/GDB 原始文件 | 固定内核接受原生 LibOS 模块，拒绝作为额外 boot module 提交的原始 Linux ELF（`-ENOEXEC`）。在 Ring 3 逐字节核对嵌入文件及每条 LOAD，检查 BSS、权限访问返回值和整份栈表 |
| QEMU/GDB 虚拟 CPU | 客体 EIP `0x08049000`、ESP `0xbfffff90`、首取字节 `0xb8`。取指保持寄存器原值。原生 CPL 3（CS `0x1b`）和 CR3 由 GDB 单独核对 |
| QEMU/GDB 权限与回收 | Linux 段地址、客体栈、动态资源窗口和 VGA 无原生 PTE。域的设备授权、活动句柄和动态页计数为 0。原生加载器退出状态 0，内核回收域 |
| `make integration` | 12 个场景全过，包含 Phase 1/2 的 Kilo、sbase cat、TTY、授权、OOM 和撤销回归 |

原生加载器 ELF 为 49,024 字节，使用 14 个映像页。36 KiB 客体存储计入原生 BSS。没有执行 Linux i386 客体指令或 Linux 系统调用，串口亦未出现样本的输出字符串。

构建结果保留在 `build/`。集成记录为 `build/integration/results.json`、`linux-loader.gdb.log` 和 `linux-loader.serial.log`。 [Phase 2 基线结果](phase2-validation.md)
