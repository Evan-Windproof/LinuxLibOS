/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "libos.h"
#include "linux_guest.h"
#ifndef LINUX_NATIVE_CHECKS
#include <stdio.h>
#endif

#define CODE UINT32_C(0x08049000)
#define DATA UINT32_C(0x0804a000)
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

static struct linux_guest guest;
static unsigned output_calls, output_size, safe_points;
static int32_t output_result;
static unsigned char output_bytes[8];

static int32_t output(void *context, const unsigned char *bytes, size_t size)
{
    (void)context;
    if (size > sizeof(output_bytes)) return -5;
    ++output_calls;
    output_size = (unsigned)size;
    memcpy(output_bytes,bytes,size);
    return output_result;
}

static void safe_point(void *context)
{
    (void)context;
    ++safe_points;
}

static const struct linux_guest_services services = { output, safe_point, NULL };

/* Synthetic test-only instructions; real executable samples are separate
   inputs with loader and end-to-end checks. */
static void prepare(const unsigned char *code, unsigned size)
{
    memset(&guest,0,sizeof(guest));
    guest.regions[0] = (struct linux_guest_region){ CODE,size,0,LINUX_PF_R | LINUX_PF_X };
    guest.regions[1] = (struct linux_guest_region){ DATA,8,size,LINUX_PF_R | LINUX_PF_W };
    guest.region_count = 2;
    guest.cpu = (struct linux_i386_cpu){ .eip = CODE, .esp = LINUX_GUEST_STACK_TOP, .eflags = 0x202 };
    memcpy(guest.image,code,size);
    output_calls = output_size = safe_points = 0;
    output_result = 0;
}

static int expect_failure(enum linux_guest_status expected)
{
    struct linux_i386_cpu before = guest.cpu;
    unsigned steps = guest.steps;
    unsigned char memory[32];
    memcpy(memory,guest.image,sizeof(memory));
    CHECK(linux_guest_step(&guest,&services) == expected);
    CHECK(!memcmp(&guest.cpu,&before,sizeof(before)) && guest.steps == steps);
    CHECK(!memcmp(guest.image,memory,sizeof(memory)) && !output_calls && !guest.exited);
    return 0;
}

static int failures(void)
{
    static const unsigned char interrupt[] = {0xcd,0x81};
    static const unsigned char unknown[] = {0x0f};
    static const unsigned char short_mov[] = {0xb8,0,0,0};
    static const unsigned char sib[] = {0x8b,0x04};
    static const unsigned char branch[] = {0xeb,0x7f};
    static const unsigned char store[] = {0x89,0x0d,0x00,0x90,0x04,0x08};
    static const unsigned char push[] = {0x50};
    static const unsigned char pop[] = {0x58};

    prepare(interrupt,sizeof(interrupt));
    CHECK(!expect_failure(LINUX_GUEST_UNSUPPORTED_INSTRUCTION));
    prepare(unknown,sizeof(unknown));
    CHECK(!expect_failure(LINUX_GUEST_UNSUPPORTED_INSTRUCTION));
    guest.cpu.eip = UINT32_MAX;
    CHECK(!expect_failure(LINUX_GUEST_UNMAPPED));
    guest.cpu.eip = DATA;
    CHECK(!expect_failure(LINUX_GUEST_PERMISSION));
    prepare(short_mov,sizeof(short_mov));
    guest.regions[1] = (struct linux_guest_region){ CODE+4,8,4,LINUX_PF_X };
    CHECK(!expect_failure(LINUX_GUEST_TRUNCATED_INSTRUCTION));
    prepare(sib,sizeof(sib));
    CHECK(!expect_failure(LINUX_GUEST_ENCODING));
    prepare(branch,sizeof(branch));
    CHECK(!expect_failure(LINUX_GUEST_BAD_JUMP));
    prepare(store,sizeof(store));
    guest.cpu.ecx = UINT32_MAX;
    CHECK(!expect_failure(LINUX_GUEST_PERMISSION));
    prepare(push,sizeof(push));
    guest.cpu.esp = LINUX_GUEST_STACK_TOP - LINUX_GUEST_STACK_SIZE;
    CHECK(!expect_failure(LINUX_GUEST_UNMAPPED));
    guest.cpu.esp = DATA+4;
    CHECK(!expect_failure(LINUX_GUEST_UNMAPPED));
    prepare(pop,sizeof(pop));
    CHECK(!expect_failure(LINUX_GUEST_UNMAPPED));
    return 0;
}

static int syscall_case(uint32_t number, uint32_t fd, uint32_t address, uint32_t length,
                        int32_t backend, int32_t result, unsigned calls)
{
    static const unsigned char syscall[] = {0xcd,0x80};
    prepare(syscall,sizeof(syscall));
    CHECK(linux_guest_write(&guest,DATA,"ab\0cd",5) == LINUX_GUEST_OK);
    guest.cpu.eax = number;
    guest.cpu.ebx = fd;
    guest.cpu.ecx = address;
    guest.cpu.edx = length;
    output_result = backend;
    CHECK(linux_guest_step(&guest,&services) == LINUX_GUEST_OK);
    CHECK(guest.cpu.eax == (uint32_t)result && guest.cpu.eip == CODE+2 && guest.steps == 1);
    CHECK(guest.cpu.ebx == fd && guest.cpu.ecx == address && guest.cpu.edx == length);
    CHECK(output_calls == calls && !guest.exited);
    if (calls) CHECK(output_size == 5 && !memcmp(output_bytes,"ab\0cd",5));
    return 0;
}

static int execute(void)
{
    /* ADD +1 overflows, JZ falls through, PUSH/POP round trip, exit 0x125. */
    static const unsigned char code[] = {
        0xb8,0xff,0xff,0xff,0x7f, 0x83,0xc0,1, 0x74,2, 0x50,0x5a,
        0xbb,0x25,0x01,0,0, 0xb8,1,0,0,0, 0xcd,0x80
    };
    prepare(code,sizeof(code));
    CHECK(linux_guest_step(&guest,&services) == LINUX_GUEST_OK && guest.cpu.eip == CODE+5);
    CHECK(linux_guest_step(&guest,&services) == LINUX_GUEST_OK && guest.cpu.eip == CODE+8);
    CHECK(guest.cpu.eax == UINT32_C(0x80000000) && guest.cpu.eflags == 0xa96);
    CHECK(linux_guest_run(&guest,&services) == LINUX_GUEST_EXITED);
    CHECK(guest.steps == 8 && guest.exit_status == 37 && guest.cpu.eip == CODE+sizeof(code));
    CHECK(guest.cpu.esp == LINUX_GUEST_STACK_TOP && guest.cpu.edx == UINT32_C(0x80000000));
    CHECK(linux_guest_step(&guest,&services) == LINUX_GUEST_EXITED && guest.steps == 8);

    static const unsigned char loop[] = {0xeb,0xfe};
    prepare(loop,sizeof(loop));
    CHECK(linux_guest_run(&guest,&services) == LINUX_GUEST_STEP_EXHAUSTED);
    CHECK(guest.steps == LINUX_GUEST_STEP_LIMIT && guest.cpu.eip == CODE && !guest.exited);
    CHECK(safe_points == LINUX_GUEST_STEP_LIMIT / 32 + 1);
    return 0;
}

static int checks(void)
{
    int line = failures();
    if (line) return line;
    CHECK(!syscall_case(4,1,DATA,5,3,3,1));
    CHECK(!syscall_case(4,1,DATA,5,-11,-11,1));
    CHECK(!syscall_case(4,1,DATA,5,6,-5,1));
    CHECK(!syscall_case(4,2,DATA,5,0,-9,0));
    CHECK(!syscall_case(4,1,0,0,0,0,0));
    CHECK(!syscall_case(4,1,UINT32_MAX,5,0,-14,0));
    CHECK(!syscall_case(4,1,DATA,UINT32_MAX,0,-14,0));
    CHECK(!syscall_case(4,1,DATA+7,2,0,-14,0));
    CHECK(!syscall_case(3,1,DATA,5,0,-38,0));
    return execute();
}

#ifdef LINUX_NATIVE_CHECKS
int linux_exec_failure;

int libos_main(void)
{
    libos_observe(2);
    linux_exec_failure = checks();
    libos_observe(3);
    return linux_exec_failure ? 1 : 0;
}
#else
int main(void)
{
    int line = checks();
    if (line) {
        fprintf(stderr,"Linux execution check failed at line %d\n",line);
        return 1;
    }
    puts("PASS: Linux execution, memory faults, syscall errors and instruction budget");
    return 0;
}
#endif
