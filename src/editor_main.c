/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "kilo_port.h"

static char path[] = "/hello.txt";
unsigned char editor_readback[4096];
size_t editor_readback_size;

static int seed_file(void)
{
#ifndef OOM_FIXTURE
    static const char text[] = "Hello from Liberums\n";
#else
    static unsigned char text[4096];
    memset(text,'x',sizeof(text) - 1);
    text[sizeof(text) - 1] = '\n';
#endif
    int fd = open(path,O_CREAT | O_EXCL | O_WRONLY,0644);
    if (fd < 0) return -1;
#ifndef OOM_FIXTURE
    size_t size = sizeof(text) - 1;
#else
    size_t size = sizeof(text);
#endif
    ssize_t written = write(fd,text,size);
    if (written < 0) {
        int error = errno;
        close(fd);
        errno = error;
        return -1;
    }
    if (close(fd)) return -1;
    if (written != (ssize_t)size) {
        errno = ENOSPC;
        return -1;
    }
    return 0;
}

int libos_main(void)
{
    if (heap_open()) {
        libos_log("LinuxLibOS: heap setup failed\n");
        return 72;
    }
    if (tty_open()) {
        libos_log("LinuxLibOS: TTY setup failed\n");
        return errno == EPERM ? 77 : 73;
    }
    if (seed_file()) kilo_fail("seed RAM file");

    int status = kilo_run(path);
    if (status) return status;

    int fd = open(path,O_RDONLY);
    if (fd < 0) kilo_fail("reopen saved file");
    ssize_t count = read(fd,editor_readback,sizeof(editor_readback));
    if (count < 0 || close(fd)) kilo_fail("read reopened file");
    editor_readback_size = (size_t)count;
    libos_observe(3);
    int tty_status = tty_close();
    if (heap_close() || tty_status) return 74;

    libos_observe(4);
    libos_log("LinuxLibOS: RAM file reopened; session closed\n");
    return 0;
}
