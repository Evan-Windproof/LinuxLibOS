/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "application.h"
#include "kilo_port.h"
#ifdef APP_LAUNCHER
#include "sbase_cat.h"
#endif

static char path[] = "/hello.txt";
static struct kilo_save saved;
unsigned char editor_readback[4096];
size_t editor_readback_size;
static int readback_ok;

static int seed_file(void)
{
    int fd = open(path,O_CREAT | O_EXCL | O_WRONLY,0644);
    if (fd < 0) return -1;
#if defined(OOM_FIXTURE) || defined(LONG_FIXTURE)
    char chunk[128];
#ifdef OOM_FIXTURE
    memset(chunk,'\t',sizeof(chunk));
#else
    memset(chunk,'x',sizeof(chunk));
#endif
    for (unsigned i = 0; i < 4096 / sizeof(chunk); ++i) {
        if (i == 4096 / sizeof(chunk) - 1) chunk[sizeof(chunk)-1] = '\n';
        if (write(fd,chunk,sizeof(chunk)) != (ssize_t)sizeof(chunk)) {
            close(fd);
            return -1;
        }
    }
#else
    static const char text[] = "Hello from Liberums\n";
    if (write(fd,text,sizeof(text)-1) != (ssize_t)sizeof(text)-1) {
        close(fd);
        return -1;
    }
#endif
    return close(fd);
}

static int read_file(void)
{
    int fd = open(path,O_RDONLY);
    if (fd < 0) return -1;
    size_t count = 0;
    while (count < sizeof(editor_readback)) {
        ssize_t size = read(fd,editor_readback+count,sizeof(editor_readback)-count);
        if (size < 0) {
            close(fd);
            return -1;
        }
        if (!size) break;
        count += (size_t)size;
    }
    if (close(fd)) return -1;
    editor_readback_size = count;
    return 0;
}

static int edit(void)
{
    return kilo_run(path,&saved);
}

static int run_editor(void)
{
    readback_ok = 0;
    if (read_file()) return 70;
    saved = (struct kilo_save){
        .size = editor_readback_size,
        .hash = kilo_hash(KILO_HASH_INIT,editor_readback,editor_readback_size)
    };
    int status = app_run(edit);
    libos_log(!saved.attempts ? "Save: none\n" : saved.error ? "Save: failed\n" : "Save: ok\n");
    if (!read_file()) {
        readback_ok = saved.size == editor_readback_size &&
            saved.hash == kilo_hash(KILO_HASH_INIT,editor_readback,editor_readback_size);
    }
    libos_log(readback_ok ? "Readback: PASS\n" : "Readback: FAIL\n");
    libos_observe(3);
    return readback_ok ? status : 70;
}

#ifdef APP_LAUNCHER
static int cat_file(void)
{
    return sbase_cat(2,(const char *const[]){"cat",path});
}

static int inspect_file(void)
{
    struct stat info;
    char message[96];
    if (stat(path,&info)) return 1;
    kilo_format(message,sizeof(message),"%s\n%d bytes\n",path,(int)info.st_size);
    return screen_write(message) != 0;
}

static int launcher(void)
{
    const char *menu = "LinuxLibOS Phase 2\n\n"
                       "1. Kilo Editor\n2. File Utility (cat)\n"
                       "3. File Information\n4. Exit\n\n> ";
    for (;;) {
        if (screen_clear() || screen_write(menu)) return 73;
        char line[128];
        ssize_t size = read(STDIN_FILENO,line,sizeof(line));
        if (size < 0 && errno != EOVERFLOW) return 73;
        if (size != 2 || line[1] != '\n') continue;
        if (line[0] == '4') return 0;
        if (line[0] < '1' || line[0] > '3') continue;
        if (screen_clear()) return 73;
        int status;
        if (line[0] == '1') {
            status = run_editor();
            if (!status) continue;
        } else {
            status = app_run(line[0] == '2' ? cat_file : inspect_file);
        }
        if (status == APP_CLEANUP_FAILED) return status;
        if (status && screen_write(status == 71 ? "\nKilo ran out of memory.\n" :
                                                   "\nApplication failed.\n")) return 73;
        if (screen_write("\nPress Enter to return.\n") || screen_wait_enter()) return 73;
    }
}
#endif

static int final_screen(int status)
{
    if (screen_clear() || screen_write("LinuxLibOS\n\n")) return -1;
#ifndef APP_LAUNCHER
    if (screen_write(status == 0 ? "Kilo session ended.\n" : "Kilo session failed.\n") ||
        screen_write(readback_ok ? "RAM file verification: PASS\n" : "RAM file verification: FAIL\n"))
        return -1;
    if (saved.error && readback_ok && screen_write("Save failed; previous file preserved.\n"))
        return -1;
#endif
    return screen_write(status ? "\nSession failed.\n" : "\nSession complete.\n");
}

int libos_main(void)
{
    if (heap_open()) {
        libos_log("Heap setup failed\n");
        return 72;
    }
    if (tty_open()) {
        int status = errno == EPERM ? 77 : 73;
        libos_log("TTY setup failed\n");
        if (heap_close()) libos_log("RAM: release failed\n");
        return status;
    }
    int status = 70;
    if (seed_file()) {
        libos_log("RAM file initialization failed\n");
    } else {
#ifdef APP_LAUNCHER
        status = launcher();
#else
        status = run_editor();
#endif
    }
    if (final_screen(status) && !status) status = 73;
    int tty_status = tty_close();
    int heap_status = heap_close();
    libos_log(tty_status ? "TTY:error " : "TTY:ok ");
    libos_log(heap_status ? "RAM:error\n" : "RAM:ok\n");
    if (tty_status || heap_status) status = APP_CLEANUP_FAILED;
    libos_observe(4);
    char message[24];
    kilo_format(message,sizeof(message),"Exit:%d\n",status);
    libos_log(message);
    return status;
}
