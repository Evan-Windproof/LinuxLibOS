/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdarg.h>
#include "libos.h"
#include "posix.h"
#include "tty.h"

#define FILE_LIMIT 4
#define FD_LIMIT 12
#define NAME_BYTES 64
#define FILE_BYTES 4096

struct file {
    char name[NAME_BYTES];
    size_t size;
    unsigned open_count;
    unsigned char data[FILE_BYTES];
};

struct descriptor {
    struct file *file;
    size_t offset;
    int flags;
    int active;
};

static struct file files[FILE_LIMIT];
static struct descriptor descriptors[FD_LIMIT] = {
    [0] = { .flags = O_RDONLY, .active = 1 },
    [1] = { .flags = O_WRONLY, .active = 1 },
    [2] = { .flags = O_WRONLY, .active = 1 }
};

int errno;

static int fail(int error)
{
    errno = error;
    return -1;
}

const char *strerror(int error)
{
    switch (error) {
    case EPERM: return "permission denied";
    case ENOENT: return "no such file";
    case EIO: return "I/O error";
    case EBADF: return "bad descriptor";
    case EAGAIN: return "try again";
    case ENOMEM: return "out of memory";
    case EACCES: return "access denied";
    case EBUSY: return "resource busy";
    case EEXIST: return "file exists";
    case EINVAL: return "invalid argument";
    case EMFILE: return "too many open descriptors";
    case ENOTTY: return "not a terminal";
    case ENOSPC: return "RAM file space exhausted";
    case ESPIPE: return "descriptor cannot seek";
    case ENAMETOOLONG: return "name too long";
    case ENOSYS: return "unsupported operation";
    case EOVERFLOW: return "input overflow";
    case ESTALE: return "stale resource";
    default: return "unknown error";
    }
}

static struct descriptor *lookup(int fd)
{
    if (fd < 0 || fd >= FD_LIMIT || !descriptors[fd].active) {
        errno = EBADF;
        return NULL;
    }
    return &descriptors[fd];
}

static const char *file_name(const char *path)
{
    if (!path) {
        errno = EINVAL;
        return NULL;
    }
    if (*path == '/') ++path;
    if (!*path) {
        errno = ENOENT;
        return NULL;
    }
    if (strchr(path,'/') || !strncmp(path,".",2) || !strncmp(path,"..",3)) {
        errno = EINVAL;
        return NULL;
    }
    if (strlen(path) >= NAME_BYTES) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    return path;
}

static struct file *find_file(const char *name)
{
    for (unsigned i = 0; i < FILE_LIMIT; ++i)
        if (!strncmp(files[i].name,name,NAME_BYTES)) return &files[i];
    return NULL;
}

static struct file *free_file(void)
{
    for (unsigned i = 0; i < FILE_LIMIT; ++i)
        if (!files[i].name[0] && !files[i].open_count) return &files[i];
    return NULL;
}

int open(const char *name, int flags, ...)
{
    const int supported = 3 | O_CREAT | O_EXCL | O_TRUNC | O_APPEND;
    int direction = flags & 3;
    if ((flags & ~supported) || direction == 3 ||
        ((flags & O_TRUNC) && direction == O_RDONLY) ||
        ((flags & O_EXCL) && !(flags & O_CREAT))) return fail(EINVAL);
    name = file_name(name);
    if (!name) return -1;

    int fd = STDERR_FILENO + 1;
    while (fd < FD_LIMIT && descriptors[fd].active) ++fd;
    if (fd == FD_LIMIT) return fail(EMFILE);

    struct file *file = find_file(name);
    if (file && (flags & O_EXCL)) return fail(EEXIST);
    if (!file) {
        if (!(flags & O_CREAT)) return fail(ENOENT);
        file = free_file();
        if (!file) return fail(ENOSPC);
        file->size = 0;
        memcpy(file->name,name,strlen(name)+1);
    }
    if (flags & O_TRUNC) file->size = 0;
    ++file->open_count;
    descriptors[fd] = (struct descriptor){ .file = file, .flags = flags, .active = 1 };
    return fd;
}

int close(int fd)
{
    struct descriptor *desc = lookup(fd);
    if (!desc) return -1;
    if (desc->file) --desc->file->open_count;
    *desc = (struct descriptor){0};
    return 0;
}

unsigned posix_close_files(void)
{
    unsigned count = 0;
    for (int fd = STDERR_FILENO + 1; fd < FD_LIMIT; ++fd) {
        if (descriptors[fd].active) {
            close(fd);
            ++count;
        }
    }
    return count;
}

ssize_t read(int fd, void *buffer, size_t count)
{
    struct descriptor *desc = lookup(fd);
    if (!desc || (desc->flags & 3) == O_WRONLY) return fail(EBADF);
    if (!count) return 0;
    if (!buffer || count > INT32_MAX) return fail(EINVAL);
    if (!desc->file) return tty_read(buffer,count);
    if (desc->offset >= desc->file->size) return 0;
    size_t left = desc->file->size - desc->offset;
    if (count > left) count = left;
    memcpy(buffer,desc->file->data + desc->offset,count);
    desc->offset += count;
    return (ssize_t)count;
}

ssize_t write(int fd, const void *buffer, size_t count)
{
    struct descriptor *desc = lookup(fd);
    if (!desc || (desc->flags & 3) == O_RDONLY) return fail(EBADF);
    if (!count) return 0;
    if (!buffer || count > INT32_MAX) return fail(EINVAL);
    struct file *file = desc->file;
    if (!file) return tty_write(buffer,count);
    size_t offset = desc->flags & O_APPEND ? file->size : desc->offset;
    if (offset >= FILE_BYTES) return fail(ENOSPC);
    if (count > FILE_BYTES - offset) count = FILE_BYTES - offset;
    if (offset > file->size) memset(file->data + file->size,0,offset - file->size);
    memcpy(file->data + offset,buffer,count);
    desc->offset = offset + count;
    if (desc->offset > file->size) file->size = desc->offset;
    return (ssize_t)count;
}

off_t lseek(int fd, off_t offset, int whence)
{
    struct descriptor *desc = lookup(fd);
    if (!desc) return -1;
    if (!desc->file) return fail(ESPIPE);
    off_t base;
    switch (whence) {
    case SEEK_SET: base = 0; break;
    case SEEK_CUR: base = (off_t)desc->offset; break;
    case SEEK_END: base = (off_t)desc->file->size; break;
    default: return fail(EINVAL);
    }
    if (offset > INT32_MAX - base) return fail(EOVERFLOW);
    if (offset < -base) return fail(EINVAL);
    return (off_t)(desc->offset = (size_t)(base + offset));
}

int fstat(int fd, struct stat *info)
{
    struct descriptor *desc = lookup(fd);
    if (!desc) return -1;
    if (!info) return fail(EINVAL);
    if (!desc->file && !tty_ready()) return fail(ENOTTY);
    *info = desc->file ? (struct stat){ (off_t)desc->file->size,S_IFREG }
                      : (struct stat){ 0,S_IFCHR };
    return 0;
}

int stat(const char *name, struct stat *info)
{
    if (!info) return fail(EINVAL);
    name = file_name(name);
    if (!name) return -1;
    struct file *file = find_file(name);
    if (!file) return fail(ENOENT);
    *info = (struct stat){ (off_t)file->size,S_IFREG };
    return 0;
}

int unlink(const char *name)
{
    name = file_name(name);
    if (!name) return -1;
    struct file *file = find_file(name);
    if (!file) return fail(ENOENT);
    file->name[0] = '\0';
    return 0;
}

int access(const char *name, int mode)
{
    if (mode & ~(R_OK | W_OK | X_OK)) return fail(EINVAL);
    name = file_name(name);
    if (!name) return -1;
    if (!find_file(name)) return fail(ENOENT);
    if (mode & X_OK) return fail(EACCES);
    return 0;
}

int rename(const char *old_path, const char *new_path)
{
    const char *source_name = file_name(old_path);
    if (!source_name) return -1;
    const char *target_name = file_name(new_path);
    if (!target_name) return -1;
    struct file *source = find_file(source_name);
    if (!source) return fail(ENOENT);
    struct file *target = find_file(target_name);
    if (source == target) return 0;

    /* Open descriptors still reference the old target after replacement. */
    if (target) target->name[0] = '\0';
    memcpy(source->name,target_name,strlen(target_name)+1);
    return 0;
}

int ftruncate(int fd, off_t size)
{
    struct descriptor *desc = lookup(fd);
    if (!desc) return -1;
    if (!desc->file || (desc->flags & 3) == O_RDONLY || size < 0) return fail(EINVAL);
    if (size > FILE_BYTES) return fail(ENOSPC);
    struct file *file = desc->file;
    if ((size_t)size > file->size)
        memset(file->data + file->size,0,(size_t)size - file->size);
    file->size = (size_t)size;
    return 0;
}

int isatty(int fd)
{
    struct descriptor *desc = lookup(fd);
    if (!desc) return 0;
    if (desc->file || !tty_ready()) {
        errno = ENOTTY;
        return 0;
    }
    return 1;
}

int ioctl(int fd, unsigned long request, ...)
{
    if (!isatty(fd)) return -1;
    if (request != TIOCGWINSZ) return fail(ENOTTY);
    va_list ap;
    va_start(ap,request);
    struct winsize *size = va_arg(ap,struct winsize *);
    va_end(ap);
    if (!size) return fail(EINVAL);
    *size = (struct winsize){ 25,80,0,0 };
    return 0;
}
