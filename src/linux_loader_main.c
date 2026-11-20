/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "libos.h"
#include "linux_guest.h"

extern const unsigned char linux_sample_start[], linux_sample_end[];

struct linux_guest guest;
enum linux_guest_status load_status, step_status, rx_write_status, stack_fetch_status;
uint8_t fetched_byte;

int libos_main(void)
{
    static const char *const argv[] = { "linux-sample", "phase3.1" };
    static const char *const envp[] = { "LANG=C" };
    size_t size = (uintptr_t)linux_sample_end - (uintptr_t)linux_sample_start;
    load_status = linux_guest_load_elf(&guest,linux_sample_start,size,2,argv,1,envp);
    if (load_status != LINUX_GUEST_OK) {
        libos_log("Linux ELF32: ");
        libos_log(linux_guest_error(load_status));
        libos_log("\n");
        return 1;
    }
    libos_observe(2);

    step_status = linux_guest_step(&guest,&fetched_byte);
    rx_write_status = linux_guest_write(&guest,guest.cpu.eip,&fetched_byte,1);
    uint8_t discarded;
    stack_fetch_status = linux_guest_fetch(&guest,guest.cpu.esp,&discarded,1);
    if (step_status != LINUX_GUEST_UNSUPPORTED_INSTRUCTION ||
        rx_write_status != LINUX_GUEST_PERMISSION || stack_fetch_status != LINUX_GUEST_PERMISSION) {
        libos_log("Linux ELF32: guest access check failed\n");
        return 2;
    }
    libos_observe(3);
    libos_log("Linux ELF32 loaded; stack ready; execution deferred\n");
    return 0;
}
