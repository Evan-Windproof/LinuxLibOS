/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "application.h"
#include "libos.h"
#include "posix.h"

int app_call(int (*entry)(void));
_Noreturn void app_unwind(unsigned status);

static int active;
static int aborted;
unsigned app_returns, app_leak_count;
int app_last_status;

int app_run(int (*entry)(void))
{
    struct termios saved;
    if (active || heap_live_bytes || tcgetattr(STDIN_FILENO,&saved))
        return APP_CLEANUP_FAILED;

    aborted = 0;
    active = 1;
    int status = app_call(entry);
    active = 0;

    unsigned descriptors = posix_close_files();
    if (!aborted && (descriptors || heap_live_bytes)) {
        ++app_leak_count;
        status = APP_CLEANUP_FAILED;
    }
    heap_reset();
    if (tcsetattr(STDIN_FILENO,TCSAFLUSH,&saved)) status = APP_CLEANUP_FAILED;

    app_last_status = status;
    ++app_returns;
    libos_observe(5);
    return status;
}

_Noreturn void app_abort(unsigned status)
{
    if (active) {
        aborted = 1;
        app_unwind(status);
    }
    libos_exit(status);
}

int screen_write(const char *text)
{
    size_t size = strlen(text);
    while (size) {
        ssize_t sent = write(STDOUT_FILENO,text,size);
        if (sent < 1) return -1;
        text += sent;
        size -= (size_t)sent;
    }
    return 0;
}

int screen_clear(void)
{
    return screen_write("\x1b[0m\x1b[2J\x1b[H");
}

int screen_wait_enter(void)
{
    for (;;) {
        char byte;
        ssize_t n = read(STDIN_FILENO,&byte,1);
        if (n == 1 && (byte == '\n' || byte == '\r')) return 0;
        if (n < 0 && errno != EOVERFLOW) return -1;
    }
}
