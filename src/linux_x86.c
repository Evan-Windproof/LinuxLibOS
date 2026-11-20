/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "linux_exec.h"

#define STACK_BASE (LINUX_GUEST_STACK_TOP - LINUX_GUEST_STACK_SIZE)
#define SIGN_BIT UINT32_C(0x80000000)
enum { CF = 1u, PF = 4u, AF = 16u, ZF = 64u, SF = 128u, OF = 2048u };

static uint32_t *gpr(struct linux_i386_cpu *cpu, unsigned reg)
{
    switch (reg & 7) {
    case 0: return &cpu->eax;
    case 1: return &cpu->ecx;
    case 2: return &cpu->edx;
    case 3: return &cpu->ebx;
    case 4: return &cpu->esp;
    case 5: return &cpu->ebp;
    case 6: return &cpu->esi;
    default: return &cpu->edi;
    }
}

static uint32_t read32(const unsigned char *bytes)
{
    return bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static enum linux_guest_status read_word(const struct linux_guest *guest, uint32_t address,
                                        uint32_t *result)
{
    unsigned char bytes[4];
    enum linux_guest_status status = linux_guest_read(guest,address,bytes,sizeof(bytes));
    if (status == LINUX_GUEST_OK) *result = read32(bytes);
    return status;
}

static enum linux_guest_status write_word(struct linux_guest *guest, uint32_t address, uint32_t value)
{
    unsigned char bytes[4] = {value,value >> 8,value >> 16,value >> 24};
    return linux_guest_write(guest,address,bytes,sizeof(bytes));
}

/* EIP's first byte was already executable. Validate the entire encoding
   from that address so an operand cannot run into a separate region. */
static enum linux_guest_status encoding(const struct linux_guest *guest, unsigned char *bytes,
                                       unsigned length)
{
    enum linux_guest_status status = linux_guest_fetch(guest,guest->cpu.eip,bytes,length);
    return status == LINUX_GUEST_UNMAPPED ? LINUX_GUEST_TRUNCATED_INSTRUCTION : status;
}

static int32_t signed_byte(unsigned char byte)
{
    return byte < 128 ? byte : (int32_t)byte - 256;
}

static void arithmetic(struct linux_i386_cpu *cpu, uint32_t *reg, unsigned operation, uint32_t b)
{
    uint32_t a = *reg;
    int subtract = operation != 0;
    uint32_t result = subtract ? a-b : a+b;
    unsigned parity = result & 255;
    parity ^= parity >> 4;
    parity ^= parity >> 2;
    parity ^= parity >> 1;
    int carry = subtract ? a < b : result < a;
    uint32_t overflow = subtract ? (a ^ b) & (a ^ result) : ~(a ^ b) & (a ^ result);
    cpu->eflags = (cpu->eflags & ~(uint32_t)(CF | PF | AF | ZF | SF | OF)) |
        (carry ? CF : 0) | ((parity & 1) ? 0 : PF) | ((a ^ b ^ result) & AF) |
        (result ? 0 : ZF) | ((result & SIGN_BIT) ? SF : 0) | ((overflow & SIGN_BIT) ? OF : 0);
    if (operation != 7) *reg = result;
}

static enum linux_guest_status stack_step(struct linux_guest *guest, struct linux_i386_cpu *cpu,
                                         unsigned char opcode)
{
    uint32_t *reg = gpr(cpu,opcode);
    if (opcode < 0x58) {
        if (cpu->esp < STACK_BASE + 4 || cpu->esp > LINUX_GUEST_STACK_TOP)
            return LINUX_GUEST_UNMAPPED;
        enum linux_guest_status status = write_word(guest,cpu->esp-4,*reg);
        if (status == LINUX_GUEST_OK) cpu->esp -= 4;
        return status;
    }
    if (cpu->esp < STACK_BASE || cpu->esp > LINUX_GUEST_STACK_TOP - 4)
        return LINUX_GUEST_UNMAPPED;
    uint32_t value;
    enum linux_guest_status status = read_word(guest,cpu->esp,&value);
    if (status == LINUX_GUEST_OK) {
        cpu->esp += 4;
        *reg = value;
    }
    return status;
}

static enum linux_guest_status decode_execute(struct linux_guest *guest, struct linux_i386_cpu *cpu,
                                             const struct linux_guest_services *services)
{
    unsigned char bytes[6];
    enum linux_guest_status status = linux_guest_fetch(guest,cpu->eip,bytes,1);
    if (status != LINUX_GUEST_OK) return status;

    unsigned char opcode = bytes[0];
    unsigned length;
    switch (opcode) {
    case 0x50: case 0x51: case 0x52: case 0x53:
    case 0x54: case 0x55: case 0x56: case 0x57:
    case 0x58: case 0x59: case 0x5a: case 0x5b:
    case 0x5c: case 0x5d: case 0x5e: case 0x5f:
        length = 1;
        status = stack_step(guest,cpu,opcode);
        break;
    case 0xb8: case 0xb9: case 0xba: case 0xbb:
    case 0xbc: case 0xbd: case 0xbe: case 0xbf:
        length = 5;
        status = encoding(guest,bytes,length);
        if (status == LINUX_GUEST_OK) *gpr(cpu,opcode) = read32(bytes+1);
        break;
    case 0x89: case 0x8b: {
        status = encoding(guest,bytes,2);
        if (status != LINUX_GUEST_OK) return status;
        unsigned char modrm = bytes[1];
        uint32_t *reg = gpr(cpu,modrm >> 3);
        if ((modrm & 0xc0) == 0xc0) {
            length = 2;
            uint32_t *rm = gpr(cpu,modrm);
            if (opcode == 0x8b) *reg = *rm;
            else *rm = *reg;
        } else if ((modrm & 0xc7) == 5) {
            length = 6;
            status = encoding(guest,bytes,length);
            if (status != LINUX_GUEST_OK) return status;
            uint32_t address = read32(bytes+2);
            status = opcode == 0x8b ? read_word(guest,address,reg) : write_word(guest,address,*reg);
        } else {
            return LINUX_GUEST_ENCODING;
        }
        break;
    }
    case 0x3d:
        length = 5;
        status = encoding(guest,bytes,length);
        if (status == LINUX_GUEST_OK) arithmetic(cpu,&cpu->eax,7,read32(bytes+1));
        break;
    case 0x83: {
        length = 3;
        status = encoding(guest,bytes,2);
        if (status != LINUX_GUEST_OK) return status;
        unsigned operation = bytes[1] >> 3 & 7;
        if ((bytes[1] & 0xc0) != 0xc0 || (operation != 0 && operation != 5 && operation != 7))
            return LINUX_GUEST_ENCODING;
        status = encoding(guest,bytes,length);
        if (status == LINUX_GUEST_OK)
            arithmetic(cpu,gpr(cpu,bytes[1]),operation,(uint32_t)signed_byte(bytes[2]));
        break;
    }
    case 0x74: case 0x75: case 0xeb: {
        length = 2;
        status = encoding(guest,bytes,length);
        if (status != LINUX_GUEST_OK) return status;
        int zero = (cpu->eflags & ZF) != 0;
        if (opcode == 0xeb || (opcode == 0x74 ? zero : !zero)) {
            int64_t target = (int64_t)cpu->eip + length + signed_byte(bytes[1]);
            unsigned char next;
            if (target < 0 || target > UINT32_MAX ||
                linux_guest_fetch(guest,(uint32_t)target,&next,1) != LINUX_GUEST_OK)
                return LINUX_GUEST_BAD_JUMP;
            cpu->eip = (uint32_t)target;
            return LINUX_GUEST_OK;
        }
        break;
    }
    case 0xcd:
        length = 2;
        status = encoding(guest,bytes,length);
        if (status != LINUX_GUEST_OK) return status;
        if (bytes[1] != 0x80) return LINUX_GUEST_UNSUPPORTED_INSTRUCTION;
        status = linux_guest_syscall(guest,cpu,services);
        break;
    default:
        return LINUX_GUEST_UNSUPPORTED_INSTRUCTION;
    }
    if (status == LINUX_GUEST_OK || status == LINUX_GUEST_EXITED) cpu->eip += length;
    return status;
}

enum linux_guest_status linux_guest_step(struct linux_guest *guest,
                                        const struct linux_guest_services *services)
{
    if (!guest) return LINUX_GUEST_ARGUMENT;
    if (guest->exited) return LINUX_GUEST_EXITED;
    if (guest->steps >= LINUX_GUEST_STEP_LIMIT) return LINUX_GUEST_STEP_EXHAUSTED;
    struct linux_i386_cpu cpu = guest->cpu;
    enum linux_guest_status status = decode_execute(guest,&cpu,services);
    if (status == LINUX_GUEST_OK || status == LINUX_GUEST_EXITED) {
        guest->cpu = cpu;
        ++guest->steps;
    }
    return status;
}

enum linux_guest_status linux_guest_run(struct linux_guest *guest,
                                       const struct linux_guest_services *services)
{
    if (!guest) return LINUX_GUEST_ARGUMENT;
    enum linux_guest_status status;
    do {
        if (services && services->safe_point && guest->steps % 32 == 0)
            services->safe_point(services->context);
        status = linux_guest_step(guest,services);
    } while (status == LINUX_GUEST_OK);
    return status;
}
