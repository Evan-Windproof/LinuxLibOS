# 兼容性矩阵

| 层级 | 状态 | 验证 |
|---|---|---|
| Native LibOS | 已实现 | ELF32 独立构建，QEMU CPL3、ABI 1.2 与正常退出 |
| POSIX Subset | 部分支持 | 宿主和 guest 文件测试、guest TTY；[接口范围](posix-subset.md) |
| Existing Source Port | Kilo 与 sbase `cat` | 两个固定上游应用在 Liberums 同一域中共享 RAM 文件 |
| Linux i386 静态 ET_EXEC 装载 | 受限支持 | 原样解析、LOAD/BSS、客体栈和首字节取指已在 Ring 3 验证 |
| 未修改 Linux ELF32 解释执行 | 受限支持 | 两个静态 i386 ELF 真正执行指令并通过 Linux write/exit 验收 |
| Linux ELF32 硬件直接执行 | 未实现 | 客体寄存器与原生寄存器由 GDB 分开核对 |
| General Linux ABI | 未实现 | 只启用 Liberums 公开 ABI 1.2 |

[Phase 3.2 验收](phase3-2-validation.md) · [Phase 3.1 验收](phase3-1-validation.md) · [Phase 2 验收](phase2-validation.md) · [Phase 1 历史验收](validation.md)
