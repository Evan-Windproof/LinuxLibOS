# Phase 3.1

LinuxLibOS 能在 Liberums v1.0.0 的 Ring 3 域内解析独立构建的 Linux i386 静态 ET_EXEC，拷贝 LOAD 段、清零 BSS，并准备一份客体 CPU 状态和 Linux 风格初始栈。验证使用含 Linux i386 `write` 和 `exit` 系统调用的原始 ELF。 [可行性审查](phase3-1-feasibility.md) · [验证报告](phase3-1-validation.md)

`linux_guest_step` 可从虚拟 EIP 获取一个可执行段字节。当前返回未实现执行，保留所有寄存器。指令解码、执行和 Linux syscall 翻译尚未实现。

```sh
make linux-elf-tests    # 宿主解析和内存检查，生成样本 ELF 与工具报告
make linux-loader-test  # 固定内核上的单项 QEMU/GDB 验证
make run-linux-loader   # 查看原生加载器的串口输出，完成后 Ctrl+C 退出 QEMU
```

通用调用入口接受宿主缓冲区。演示用 `.incbin` 嵌入真实 ELF 原件，并通过 QEMU/GDB 逐字节比较。现有 Kilo 和 Launcher 入口继续使用原来的 ELF。 [ELF 校验规则](linux-elf32-loader.md) · [客体地址空间](linux-guest-memory.md) · [初始栈](linux-initial-stack.md)
