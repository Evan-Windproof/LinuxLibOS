/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <abi/keyboard.h>
#include <abi/vga.h>
#include "ramfs_cases.h"
#include "tty.h"

extern volatile unsigned tty_overflows;
volatile unsigned fixture_mode;
volatile unsigned fixture_line;

#define NEED(test) do { if (!(test)) { fixture_line = __LINE__; return 60; } } while (0)

static int heap_test(void)
{
    unsigned char *first = malloc(33), *other = malloc(64);
    NEED(first && other && !((uintptr_t)first % 16) && !((uintptr_t)other % 16));
    memset(first,0x5a,33);
    unsigned char *larger = realloc(first,4096);
    NEED(larger && larger != first);
    for (unsigned i = 0; i < 33; ++i) NEED(larger[i] == 0x5a);
    NEED(!realloc(larger,20000) && errno == ENOMEM && larger[32] == 0x5a);
    free(other);
    unsigned char *reused = malloc(64);
    NEED(reused == first);
    free(reused + 1);
    NEED(errno == EINVAL);
    free(reused);
    free(reused);
    NEED(errno == EINVAL);
    free(larger);
    unsigned char *zero = calloc(2048,4);
    NEED(zero == first);
    for (unsigned i = 0; i < 8192; ++i) NEED(!zero[i]);
    NEED(!calloc(SIZE_MAX,2) && errno == ENOMEM);
    free(zero);
    NEED(heap_peak_bytes >= 8192 && heap_oom_count == 2);
    return 0;
}

static int terminal_test(void)
{
    NEED(!tty_open());
    struct termios mode;
    struct winsize size;
    NEED(!tcgetattr(0,&mode) && mode.c_lflag == (ICANON | ECHO));
    NEED(isatty(0) && !ioctl(1,TIOCGWINSZ,&size) && size.ws_col == 80 && size.ws_row == 25);
    NEED(ioctl(0,0u) == -1 && errno == ENOTTY);
    static const char wide_line[] =
        "00000000001111111111222222222233333333334444444444"
        "55555555556666666666777777777Z\x1b[0K";
    NEED(write(1,wide_line,sizeof(wide_line)-1) == (ssize_t)(sizeof(wide_line)-1));
    NEED(((const volatile uint16_t *)VGA_USER_ADDRESS)[79] == 0x075a);

    static const char screen_test[] = "\x1b[2J\x1b[25;1H\x1b[31mR\r\n\x1b[0m";
    NEED(write(1,screen_test,sizeof(screen_test)-1) == (ssize_t)(sizeof(screen_test)-1));
    libos_log("TTY canonical ready\n");
    char line[4];
    NEED(read(0,line,2) == 2 && !memcmp(line,"Ab",2));
    NEED(read(0,line,2) == 1 && line[0] == '\n');
    mode.c_cc[VTIME] = 1;
    NEED(tcsetattr(0,TCSANOW,&mode) == -1 && errno == EINVAL);
    mode.c_cc[VTIME] = 0;
    mode.c_iflag = mode.c_oflag = mode.c_lflag = 0;
    NEED(!tcsetattr(0,TCSAFLUSH,&mode));
    libos_observe(3);
    libos_log("TTY raw ready\n");

    /* Use the public device's existing generation for non-consuming stats. */
    uint32_t lost = 0;
    int status;
    do {
        status = liberums_call(SYS_KBD_STATS,KBD_HANDLE,1,0,0,&lost);
        NEED(status >= 0);
    } while (!lost);
    NEED(read(0,line,1) == -1 && errno == EOVERFLOW && tty_overflows == 1);
    libos_log("TTY overflow recovered\n");
    NEED(read(0,line,1) == 1 && line[0] == 'z');
    NEED(!tty_close());
    NEED(liberums_call(SYS_KBD_POLL,KBD_HANDLE,1,0,0,0) == RES_ESTALE);
    NEED(liberums_call(SYS_VGA_CURSOR,VGA_DEVICE_HANDLE,1,0,0,0) == RES_ESTALE);
    return 0;
}

int libos_main(void)
{
    NEED(!heap_open());
    /* Ensure a real IRQ0/preemption can be observed even in this small program. */
    for (volatile unsigned i = 0; i < 8000000; ++i) { }
    libos_observe(2);
    if (fixture_mode == 1) {
        while (1) libos_safe_point();
    }
    if (fixture_mode == 2) {
        /* Wait for the controlled deadline, then attempt a real user read. */
        while (fixture_mode == 2) { }
        return *(const volatile unsigned char *)RESOURCE_WINDOW;
    }

    int status = heap_test();
    if (status) return status;
    fixture_line = (unsigned)ramfs_tests();
    if (fixture_line) return 61;

    uint32_t generation;
    NEED(liberums_call(SYS_PAGE_CLAIM,RESOURCE_ALL,0,0,0,&generation) == RES_ENOSPC);
    NEED(!heap_close());
    NEED(liberums_call(SYS_PAGE_QUERY,1,1,0,0,0) == RES_ESTALE);
    status = terminal_test();
    if (status) return status;
    libos_observe(4);
    libos_log("LinuxLibOS: guest checks passed\n");
    return 0;
}
