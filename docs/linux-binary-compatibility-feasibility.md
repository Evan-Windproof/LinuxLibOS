# 未修改的 Linux i386 ELF32 可行性研究

Phase 1 已实现并测试由公开 SDK 构建的 Liberums ELF32、部分 POSIX 风格 C API 与 Kilo 源码移植。Phase 2 扩展用户态文件接口与静态应用入口，仍通过 Liberums ABI 1.2 运行。Phase 3.1 已验证受限 Linux i386 ET_EXEC 的解析、客体映像、初始栈及首字节取指。Phase 3.2 已验证静态样本的受限指令解释与 Linux write/exit 翻译。动态链接、直接执行和通用 Linux 进程语义尚未实现。 [Phase 3.2 验证](phase3-2-validation.md)

## 当前 ABI 冲突

Linux i386 使用 `int 0x80` 时，EAX 是 Linux 调用号，参数通常为 EBX、ECX、EDX、ESI、EDI、EBP。Liberums 同样接收 `int 0x80`，但 gate 直入 Liberums 调度，按 EAX 选择 Kernel ABI 1.2。调用号和指针语义冲突，例如 Linux 的 3 是 read，而 Liberums 的 3 是 ABI_QUERY。Linux 号码不能直接传给 Liberums。普通 C 包装只影响编译或链接到该包装的代码；任意现有 ELF 在自己的指令中执行 `int 0x80` 时，CPU 不会进入 LibOS 包装。

Liberums 外部映像要求自有 ABI Note、固定映像和栈区间、静态 ET_EXEC 装载。典型 Linux i386 动态程序还需要 PT_INTERP、ld-linux.so.2、TLS、auxv、argc/argv/envp 初始栈、重定位、mmap/brk/mprotect 和多个文件映射。libc 也可能使用 vDSO/sysenter 路径。即使静态 ELF 恰好落在允许地址内，源文件重编译也无法证明原始 syscall 和 Linux 栈约定兼容。进程、文件系统、TTY、信号、中断重启语义以及用户地址检查仍需逐项实现。

## 一个静态 ELF 最小实验的边界

如果后续验证只选择一个固定的原生 i386 ET_EXEC，可先从无 PT_INTERP、无 libc 的小程序开始。装载器需要验证 ELF 类型、段权限与范围，给目标机器提供入口和映射，并按程序实际读取的契约构造 Linux 初始栈。栈从 `argc` 开始，依次包含以 NULL 结尾的 `argv` 和 `envp` 指针数组，再接以 AT_NULL 结束的 auxv。使用 libc 的静态程序通常需要更多 auxv 与调用（如 `AT_PHDR`、`AT_PAGESZ`、`AT_RANDOM`、`brk`、`set_thread_area` 等），因此验证无 libc 的 write/exit 只证明同一受限样本路径。

用户态解释器可以把目标 ELF 保持为数据，合成客体地址空间和初始栈，再将目标 `int 0x80` 实现为翻译器内部操作。让未改写的机器码直接执行则仍需内核支持受授权的 trap 委派或等价机制。常见静态 Linux 映像的段地址、堆区与独立栈也可能越过 Liberums 的 ELF 窗口。应在实现前分别核对 64 页映像、16 KiB 用户栈与 16 KiB 普通动态 RAM 的实际需求。 [Linux i386 syscall 入口](https://man7.org/linux/man-pages/man2/syscall.2.html) · [Linux auxv](https://man7.org/linux/man-pages/man3/getauxval.3.html) · [Liberums 加载与页约束](feasibility.md)

## 可能方案

| 方案 | 代价与边界 |
|---|---|
| 用户态指令解释 | 逐条解释目标指令并将 syscall 作为虚拟操作；保持内核 ABI，最容易封闭 guest 地址访问；内存、解释速度、x86 指令/异常覆盖成本高 |
| 动态翻译或受约束的重写 | 在代码缓存对 trap 指令和间接跳转做完整控制；支持运行时生成代码、多入口、重叠解码和写后执行会显著增加验证与映射成本 |
| 用户态软 CPU / 子系统模拟 | 可模拟更完整的地址空间、FPU/SIMD 与信号，但可运行程序规模受四页动态 RAM 和 64 页映像约束，模拟器本身也占资源 |
| 通用内核陷入委派 | 当前 ABI 无注册或回复机制；将指定域的 trap 和寄存器以有界事件转交可信授权的用户态处理器，需处理嵌套、恢复、超时与跨域访问 |
| 未来派生内核最小 ABI 扩展 | 先研究与系统调用号无关的用户陷入委派、用户可控 VA 区域与更弹性的受配额内存。Linux syscall 翻译、文件、信号、TTY 策略仍由 LibOS 提供 |

Phase 3.2 实际验证了第一种方案，覆盖两个静态 i386 ELF。其余方案仍需独立研究。
