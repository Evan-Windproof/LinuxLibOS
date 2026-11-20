# 兼容性矩阵

| 层级 | 状态 | 验证 |
|---|---|---|
| Native LibOS | 已实现 | ELF32 独立构建，QEMU CPL3、ABI 1.2 与正常退出 |
| POSIX Subset | 部分支持 | 宿主和 guest 文件测试、guest TTY；[接口范围](posix-subset.md) |
| Existing Source Port | Kilo 与 sbase `cat` | 两个固定上游应用在 Liberums 同一域中共享 RAM 文件 |
| Linux i386 静态 ET_EXEC 装载 | 受限支持 | 原样解析、LOAD/BSS、客体栈和首字节取指已在 Ring 3 验证 |
| 未修改 Linux ELF32 执行 | 未实现 | 尚无指令语义、系统调用翻译或执行验收 |
| General Linux ABI | 未实现 | 只启用 Liberums 公开 ABI 1.2 |

[Phase 3.1 验收](phase3-1-validation.md) · [Phase 2 验收](phase2-validation.md) · [Phase 1 历史验收](validation.md)
