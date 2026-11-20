# Linux ELF32 装载器

外层由 Liberums 将原生 `build/linux-loader.elf` 装入 Ring 3。它使用项目已有启动入口、链接脚本与 ABI 1.2 Note。内层通过 `linux_guest_load_elf()` 处理 Linux 样本字节。宿主输入可以来自任意已校验长度的缓冲区。演示样本由汇编与链接目标独立生成并原样嵌入。 [接口](../include/linux_guest.h) · [样本源码](../tests/fixtures/linux-sample.S)

- 接受 ELF32、小端、EM_386、静态 ET_EXEC，System V 或 Linux OSABI 0 版。ELF 文件最多 32 KiB，最多 16 条程序头及 8 个 LOAD 段。
- 按小端字节读取 ELF 头与程序头。检查版本、头大小、程序头表以及所有已解释段的文件范围。段尺寸检查使用先比较再相减的方式处理整数边界。节表与符号表不参与装载。
- LOAD 必须非空，`p_filesz <= p_memsz`，并满足客体映像范围及总存储预算。虚拟地址与文件偏移在 4 KiB 粒度一致。大于 1 的 `p_align` 必须是二次幂并满足同余。拒绝页面重叠、未知权限位和 W+X。入口必须落在 X 段的文件数据内。
- 接受 PT_NULL 和范围内的 PT_NOTE。可接受描述实际可读映射中程序头表的 PT_PHDR，以及无文件或内存负载的非执行 PT_GNU_STACK。PT_INTERP、PT_DYNAMIC、PT_TLS 及其他未实现类型返回明确错误。
- 先验证所有段和初始栈，再替换客体状态。复制 `p_filesz` 字节，剩余 `p_memsz - p_filesz` 字节清零。装载失败保留先前成功的客体状态，装载路径不访问 RAM FS、TTY 或设备句柄。

独立样本使用 GNU As 的 `--32` 与 GNU ld 的 `-m elf_i386 -static -nostdlib --build-id=none -s`。不带动态段、解释器或 Liberums Note，无 SDK、运行时或 libc 依赖。具体头部、段大小、SHA256 和反汇编由 `build/linux-sample.txt` 记录。 [实测数据](phase3-1-validation.md)
