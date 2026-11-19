# Kilo 上游与移植

| 项目 | 来源 |
|---|---|
| 上游 | [antirez/kilo](https://github.com/antirez/kilo/tree/323d93b29bd89a2cb446de90c4ed4fea1764176e) |
| 固定提交 | `323d93b29bd89a2cb446de90c4ed4fea1764176e` |
| 源码版本宏 | `0.0.1` |
| 原始源码与许可 | [upstream/kilo.c](upstream/kilo.c) · [BSD-2-Clause](upstream/LICENSE) |
| `kilo.c` SHA256 | `4a44dd0e41670a9e49ecccb338ee199334f0dd472fc7f86467569cf99c391abe` |
| 移植补丁 | [0001-liberums-port.patch](patches/0001-liberums-port.patch) |

构建校验原始源码与许可文件哈希，再用精确上下文应用补丁至 `build/kilo.c`。原版的行结构、编辑、光标移动、搜索、状态界面和保存流程保留。

- 入口改为 `kilo_run`，固定尺寸通过 `TIOCGWINSZ` 查询。Ctrl-Q 后归还控制权，允许同一域重新打开已保存文件。
- stdio 行读取替换为 RAM FS 描述符读取和 ASCII/LF 切行。接受 CRLF 并以 LF 保存。移除了 `atexit`、SIGWINCH、终端探测回退和时间依赖，状态消息保持到下一次更新。
- raw-like 输入使用 TTY 的同键续字节读取。键盘溢出显示诊断后继续等待。输出采用 128 字节缓冲分段渲染。
- 检查所有动态分配。OOM 记录尝试字节数和 Kilo 调用点，退出码 71；内核回收域。关闭语法模式选择，保留纯文本的即时搜索高亮。修改上游删除时的行长顺序和长行合并/搜索的横向边界。
- 保存前由 `ftruncate` 检查 RAM 文件容量，状态栏说明保存位置。终端和文件错误传播到诊断与退出状态。

Phase 2 补丁将 `singleline_comment_start` 扩为三个字节，让双字符 `//` 的 NUL 终止符有空间。多行注释的 `/*`、`*/` 数组已有三个字节。原始源码的 SHA256 保持不变。

当前打开操作使用定长扫描缓冲并直接填充行数据。普通行复用文本作为渲染数据；保存按行写入，退出由应用边界恢复 termios 与清理资源。 [实测与内存边界](../../docs/phase2-validation.md)
