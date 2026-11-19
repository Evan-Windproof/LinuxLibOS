# 编辑器移植结果

[Kilo 上游来源与补丁](../ports/kilo/README.md) 可独立审阅。实测通过的路径包括启动、VGA 渲染原文本、独立 Escape、方向键移动和光标位置、删除、插入 ASCII 文本、Ctrl-S 保存、Ctrl-Q 退出、同域再次打开和内容比对。用来覆盖 canonical 与键盘溢出的 fixture 独立于编辑器验收。

编辑器默认只打开一个纯文本 RAM 文件。补丁保留搜索代码，自动验收覆盖编辑/保存主路径；没有单独验证完整搜索和所有复杂行操作。语法模式选择、窗口变化信号、限时状态消息、Unicode 与磁盘保存超出这个移植的支持范围。4 KiB RAM 文件可早于文件容量达到 16 KiB 堆上限，见 [验证报告](validation.md) 的可复现 OOM。

Phase 2 采用严格上游补丁解决 GCC 15 的 NUL 终止诊断。直接启动提供结束画面，Launcher 入口在退出 Kilo 后回到菜单。未保存内容仍用三次追加 Ctrl-Q 确认。长纯文本行减少了渲染副本，密集 Tab 的扩展仍可触发 OOM。 [Phase 2 验证](phase2-validation.md)
