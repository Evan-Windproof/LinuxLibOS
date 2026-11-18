/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LINUXLIBOS_POSIX_H
#define LINUXLIBOS_POSIX_H

#include <stddef.h>
#include <stdint.h>

typedef int32_t off_t;
typedef int32_t ssize_t;

extern int errno;
enum {
    EPERM = 1, ENOENT = 2, EIO = 5, EBADF = 9, EAGAIN = 11, ENOMEM = 12,
    EBUSY = 16, EEXIST = 17, EINVAL = 22, EMFILE = 24, ENOTTY = 25, ENOSPC = 28,
    ESPIPE = 29, ENAMETOOLONG = 36, ENOSYS = 38, EOVERFLOW = 75, ESTALE = 116
};
const char *strerror(int error);

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0x40
#define O_EXCL 0x80
#define O_TRUNC 0x200
#define O_APPEND 0x400
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define S_IFREG 0100000u
#define S_IFCHR 0020000u

struct stat {
    off_t st_size;
    unsigned st_mode;
};

int open(const char *name, int flags, ...);
int close(int fd);
ssize_t read(int fd, void *buffer, size_t count);
ssize_t write(int fd, const void *buffer, size_t count);
off_t lseek(int fd, off_t offset, int whence);
int fstat(int fd, struct stat *stat);
int ftruncate(int fd, off_t size);
int isatty(int fd);
int ioctl(int fd, unsigned long request, ...);

#define TIOCGWINSZ 0x5413u
struct winsize {
    unsigned short ws_row, ws_col, ws_xpixel, ws_ypixel;
};

#define ICRNL 1u
#define OPOST 1u
#define CS8 1u
#define ECHO 1u
#define ICANON 2u
#define VMIN 0
#define VTIME 1
#define TCSANOW 0
#define TCSAFLUSH 2
struct termios {
    unsigned c_iflag, c_oflag, c_cflag, c_lflag;
    unsigned char c_cc[2];
};

int tcgetattr(int fd, struct termios *mode);
int tcsetattr(int fd, int action, const struct termios *mode);

#endif
