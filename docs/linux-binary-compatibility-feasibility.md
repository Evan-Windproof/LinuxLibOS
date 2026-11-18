# 未修改的 Linux i386 ELF32 可行性研究

Phase 1 已实现并测试由公开 SDK 构建的 Liberums ELF32、部分 POSIX 风格 C API 与 Kilo 源码移植。未实现 Linux ELF 装载器、Linux syscall 翻译、动态链接或 Linux 进程语义；也未运行未重编译的 Linux ELF。

## 当前 ABI 冲突

Linux i386 使用 `int 0x80` 时，EAX 是 Linux 调用号，参数通常为 EBX、ECX、EDX、ESI、EDI、EBP。Liberums 同样接收 `int 0x80`，但 gate 直入 Liberums 调度，按 EAX 选择 Kernel ABI 1.2。调用号和指针语义冲突，例如 Linux 的 3 是 read，而 Liberums 的 3 是 ABI_QUERY。Linux 号码不能直接传给 Liberums。普通 C 包装只影响编译或链接到该包装的代码；任意现有 ELF 在自己的指令中执行 `int 0x80` 时，CPU 不会进入 LibOS 包装。

Liberums 外部映像要求自有 ABI Note、固定映像和栈区间、静态 ET_EXEC 装载。典型 Linux i386 动态程序还需要 PT_INTERP、ld-linux.so.2、TLS、auxv、argc/argv/envp 初始栈、重定位、mmap/brk/mprotect 和多个文件映射；libc 可能使用 vDSO/sysenter 路径。即使静态 ELF 恰好落在允许地址内，源文件重编译也无法证明原始 syscall 和 Linux 栈约定兼容。进程、文件系统、TTY、信号、中断重启语义以及用户地址检查仍需逐项实现。

## 可能方案

| 方案 | 代价与边界 |
|---|---|
| 用户态指令解释 | 逐条解释目标指令并将 syscall 作为虚拟操作；保持内核 ABI，最容易封闭 guest 地址访问；内存、解释速度、x86 指令/异常覆盖成本高 |
| 动态翻译或受约束的重写 | 在代码缓存对 trap 指令和间接跳转做完整控制；支持运行时生成代码、多入口、重叠解码和写后执行会显著增加验证与映射成本 |
| 用户态软 CPU / 子系统模拟 | 可模拟更完整的地址空间、FPU/SIMD 与信号，但可运行程序规模受四页动态 RAM 和 64 页映像约束，模拟器本身也占资源 |
| 通用内核陷入委派 | 当前 ABI 无注册或回复机制；将指定域的 trap 和寄存器以有界事件转交可信授权的用户态处理器，需处理嵌套、恢复、超时与跨域访问 |
| 未来派生内核最小 ABI 扩展 | 先研究与系统调用号无关的用户陷入委派、用户可控 VA 区域与更弹性的受配额内存。Linux syscall 翻译、文件、信号、TTY 策略仍由 LibOS 提供 |

表中方案均未执行。若后续批准研究，最小实验可选一个固定静态 i386 ELF，在用户态解释少量指令并显式处理 write/exit。完整 Linux 动态用户空间或直接执行未修改的 Linux trap 仍需要单独设计与验证。
