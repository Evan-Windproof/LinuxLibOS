# Phase 2

本阶段沿用 Phase 1 提交 `ff73978130997fd5b8fa33cb9e31dc18d2ee7c93` 的公开 ABI 1.2、堆、终端和域隔离。改进包括可复现的 Kilo GCC 修复、退出可见状态和回读诊断、RAM 文件对象生命周期，以及同一域内运行 Kilo、sbase `cat` 和内置文件信息应用。

`make run` 仍直接进入 Kilo。Launcher 独立入口为 `make run-launcher`。在菜单内用 Kilo 编辑并保存，退出后可选文件查看，查看内容与已保存文件一致。 [应用生命周期](application-lifecycle.md) · [验收](phase2-validation.md)

Vim Tiny 仅做可行性研究，原生 Linux ELF32 与 Linux 系统调用模拟仍未实现。 [Vim Tiny](vim-tiny-feasibility.md) · [Linux ELF32](linux-binary-compatibility-feasibility.md)
