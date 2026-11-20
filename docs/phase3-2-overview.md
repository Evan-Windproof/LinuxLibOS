# Phase 3.2

LinuxLibOS 在固定 Liberums v1.0.0 的 Ring 3 域内解释执行两个未经修改的静态 Linux i386 ELF32。沿用 Phase 3.1 的装载、客体地址空间和初始栈。新增指令解码、寄存器更新、有限步数运行，以及 Linux `write` 和 `exit` 翻译。 [指令范围](linux-i386-execution.md) · [验收数据](phase3-2-validation.md)

```sh
make run-linux-elf       # 原 Phase 3.1 样本，COM1 输出后 Ctrl+C 退出 QEMU
make run-linux-loop      # 第二个独立样本
make linux-binary-tests  # 宿主检查、两次真实执行和一次 QEMU 负例
make linux-loader-test   # 独立保留的 Phase 3.1 装载验收
```

客体文件由 GNU As 与 ld 独立构建并原样嵌入演示容器。替换样本生成文件会重新汇编容器，装载仍施加 Phase 3.1 校验。运行入口使用内置样本。 [装载规则](linux-elf32-loader.md)

通用 Linux ABI、直接 CPU 执行、进程模型、glibc 和动态链接未实现。 [兼容性矩阵](compatibility-matrix.md)
