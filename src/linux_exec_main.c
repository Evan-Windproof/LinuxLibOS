/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <abi/syscall.h>
#include "libos.h"
#include "linux_guest.h"

extern const unsigned char linux_sample_start[], linux_sample_end[];

struct linux_guest guest;
enum linux_guest_status run_status;

static int32_t serial_output(void *context, const unsigned char *bytes, size_t size)
{
    (void)context;
    for (size_t i = 0; i < size; ++i) {
        /* The kernel takes one C-string byte. NUL cannot be emitted. */
        int32_t result = bytes[i] ? liberums_call(SYS_LOG_CHAR,bytes[i],0,0,0,0) : -5;
        if (result) return i ? (int32_t)i : (result == RES_EAGAIN ? -11 : -5);
    }
    return (int32_t)size;
}

static void revoke_check(void *context)
{
    (void)context;
    libos_safe_point();
}

int libos_main(void)
{
    static const char *const argv[] = { "linux-guest" };
    static const struct linux_guest_services services = { serial_output, revoke_check, NULL };
    size_t size = (uintptr_t)linux_sample_end - (uintptr_t)linux_sample_start;
    run_status = linux_guest_load_elf(&guest,linux_sample_start,size,1,argv,0,NULL);
    if (run_status != LINUX_GUEST_OK) {
        libos_log(linux_guest_error(run_status));
        libos_log("\n");
        return 1;
    }

    libos_log("Linux stdout:\n");
    libos_observe(2);
    run_status = linux_guest_run(&guest,&services);
    libos_observe(3);
    if (run_status != LINUX_GUEST_EXITED) {
        libos_log("\nLinux error: ");
        libos_log(linux_guest_error(run_status));
        libos_log("\n");
        return 2;
    }

    char message[] = "\nLinux exit 000\n";
    message[12] = '0' + guest.exit_status / 100;
    message[13] = '0' + guest.exit_status / 10 % 10;
    message[14] = '0' + guest.exit_status % 10;
    libos_log(message);
    return 0;
}
