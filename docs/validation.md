# Phase 1 验证报告

实测使用 Debian 13 x86-64 宿主、GCC 14.2、GNU Make 4.4.1、QEMU 10.0.13 和 GDB 16.3。启动固定 commit `7d5d574e8643c3645fa3ce2a126dcbd4ae43662f` 的 Liberums，i686 / TCG / SMP 1 / 32 MiB。内核源码工作树干净。实测内核 ELF 的 SHA256 在所有 guest 前后保持一致：

```text
03ba69356bbbd607e310952e68f72cf4d8019c0fd1f8b77373177c3602f218c4
```

同一源码在不同工具版本、构建路径下生成的调试 ELF 哈希可能不同。复现命令为 `make test`、`make integration LIBERUMS=/absolute/path/to/Liberums`；省略 LIBERUMS 时自动下载固定版本。每次运行在 `build/integration/` 写入 COM1、QEMU、GDB 日志和 `results.json`，记录各输入 ELF 哈希。GDB 断言失败时返回非零。QMP 和 GDB 使用每次运行独立的 UNIX sockets。

## 结果

| 场景 | 实际验收 |
|---|---|
| 宿主 `make test` | SDK/第三方哈希；ELF32/Note/段/页布局；RAM 文件错误、偏移、短读写、EOF、重开、稀疏写和资源上限 |
| `hello` | ABI 0x00010002、features 119、配额 4/4；CPL3、CS 0x1b、SS 0x23、私有 CR3、TSS 栈和用户栈；退出 0 |
| `editor` | 真正的上游 Kilo 移植版；读原文本、屏幕与光标核对、实际 PS/2 Escape/方向键/退格/插入；Ctrl-S 保存、Ctrl-Q 退出、重新打开并比对 37 字节；退出 0 |
| `tty` | guest 内 RAM FS 与堆测试；页配额、OOM、对齐、realloc 数据、空闲复用和旧句柄拒绝；80 列、清屏、颜色、滚屏、canonical/回显、队列溢出与恢复；退出 0 |
| `no-grant` | 设备授予另一域，VGA acquire 拒绝；退出 77，授权域正常退出 |
| `no-keyboard-grant` | VGA 获准但键盘拒绝；验证 VGA 句柄/映射回滚；退出 77，授权域正常退出 |
| `oom` | 单行 4095 字节加 LF；编辑器以状态 71 退出，内核回收所有资源 |
| `revoke` | 可信 Phase 9 诊断提交撤销，真实 POLL/INFO/ACK 收回页及 PTE；状态 96，回收整域 |
| `forced-revoke` | 等待 IRQ0 达到 deadline；PTE 撤销后执行实际 CPL3 load，#PF vector 14，error 4，CR2 0x09000000 |
| `fault-and-rejection` | 非 ELF 模块被拒绝 (-8)；独立测试域真实 #UD (vector 6)，并存的健康域使用不同 CR3 并正常退出 |

GDB 验证堆 PTE 的 Present/Write/User、实际 PFN 与所有者 ID/Epoch、Capability 权限和绑定数。设备用例检查 VGA MMIO/缓存标志和两个设备的授权；键盘与定时器 IRQ 计数分别与 EOI 相等。每个场景验证退出或异常状态、内核 CR3 恢复、域和设备清理、分配页清零及空闲页计数恢复。硬件断点和可信诊断 mailbox 用来观察与触发测试；页表、IRQ 和系统调用结果由内核真实产生。

## 容量

| 项目 | 实测 |
|---|---|
| `hello.elf` | 16,884 字节含调试信息，3 个映像页 |
| `editor.elf` | 103,888 字节含调试信息，11 个映像页；`size` 报告 text 16,843、data 207、BSS 21,344 字节 |
| 用户栈 | 4 页。编辑器用户态观测到 CS 0x1b、SS 0x23、CR3 0x153000、ESP 0x083fff5c |
| 动态堆 | 4 页共 16 KiB。目标编辑会话的申请字节峰值 187，含行数据、渲染文本、高亮和序列化缓冲的并存峰值；分配头/对齐开销另计 |
| 终端设备 | VGA 单独一页 MMIO，实际屏幕 4000 字节；PS/2 内核队列 64 字节 |

OOM 的实测调用点为 `editorUpdateSyntax`，尝试 4095 字节，失败前活动申请峰值 12,328 字节。路径为 `editorOpen → editorInsertRow → editorUpdateRow → editorUpdateSyntax → kilo_alloc → realloc(NULL) → malloc`。文件副本、行内容和渲染缓冲均已驻留，加上块头和对齐后无法在现有堆容纳高亮数组。此用例保留四页配额和原内核。可以在将来的独立用户态优化中减少冗余文本副本或按屏幕范围分配高亮。

## 验收边界

本次 QMP 使用 QEMU 模拟 PS/2，VGA 内容来自 guest 显存。GTK 本地窗口和实体键盘尚未人工验收。RAM 文件仅在同一域运行期间有效，源代码兼容来自针对 Liberums 的重新构建。未修改的 Linux ELF32、真实磁盘持久化、完整 POSIX 和完整 Linux TTY 均未实现。
