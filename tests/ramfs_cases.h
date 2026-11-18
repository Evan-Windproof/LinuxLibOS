/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "libos.h"
#include "posix.h"

#define CHECK(test) do { if (!(test)) return __LINE__; } while (0)

static int ramfs_tests(void)
{
    char data[8];
    struct stat info;
    CHECK(open("/missing",O_RDONLY) == -1 && errno == ENOENT);
    CHECK(open("/dir/file",O_CREAT) == -1 && errno == EINVAL);
    CHECK(open("/test",O_CREAT | O_RDONLY | O_TRUNC) == -1 && errno == EINVAL);
    CHECK(open("/test",0x10000) == -1 && errno == EINVAL);

    int fd = open("/test",O_CREAT | O_EXCL | O_RDWR,0600);
    CHECK(fd == 3);
    CHECK(open("/test",O_CREAT | O_EXCL | O_WRONLY,0600) == -1 && errno == EEXIST);
    CHECK(write(fd,"hello",5) == 5);
    CHECK(fstat(fd,&info) == 0 && info.st_size == 5 && info.st_mode == S_IFREG);
    CHECK(read(fd,data,1) == 0 && lseek(fd,0,SEEK_SET) == 0);
    CHECK(read(fd,data,sizeof(data)) == 5 && memcmp(data,"hello",5) == 0);
    CHECK(read(fd,data,sizeof(data)) == 0);
    CHECK(lseek(fd,-3,SEEK_CUR) == 2 && write(fd,"XX",2) == 2);
    CHECK(lseek(fd,1,SEEK_END) == 6 && write(fd,"!",1) == 1);
    CHECK(lseek(fd,-8,SEEK_END) == -1 && errno == EINVAL);
    CHECK(lseek(fd,INT32_MAX,SEEK_CUR) == -1 && errno == EOVERFLOW);
    CHECK(lseek(fd,0,SEEK_SET) == 0 && read(fd,data,7) == 7);
    CHECK(memcmp(data,"heXXo\0!",7) == 0);

    CHECK(ftruncate(fd,4097) == -1 && errno == ENOSPC);
    CHECK(fstat(fd,&info) == 0 && info.st_size == 7);
    CHECK(lseek(fd,4095,SEEK_SET) == 4095 && write(fd,"xyz",3) == 1);
    CHECK(write(fd,"z",1) == -1 && errno == ENOSPC);
    CHECK(ftruncate(fd,3) == 0 && ftruncate(fd,5) == 0);
    CHECK(close(fd) == 0 && close(fd) == -1 && errno == EBADF);

    fd = open("test",O_RDONLY);
    CHECK(fd == 3 && read(fd,data,sizeof(data)) == 5 && !memcmp(data,"heX\0\0",5));
    CHECK(isatty(fd) == 0 && errno == ENOTTY);
    CHECK(write(fd,"z",1) == -1 && errno == EBADF && close(fd) == 0);
    fd = open("/test",O_WRONLY | O_APPEND);
    CHECK(fd == 3 && lseek(fd,0,SEEK_SET) == 0 && write(fd,"z",1) == 1);
    CHECK(lseek(fd,0,SEEK_CUR) == 6 && close(fd) == 0);
    CHECK(read(1,data,1) == -1 && errno == EBADF);
    CHECK(write(0,"z",1) == -1 && errno == EBADF);
    CHECK(lseek(0,0,SEEK_SET) == -1 && errno == ESPIPE);

    for (int i = 3; i < 12; ++i) CHECK(open("/test",O_RDONLY) == i);
    CHECK(open("/new",O_CREAT | O_RDWR,0600) == -1 && errno == EMFILE);
    for (int i = 3; i < 12; ++i) CHECK(close(i) == 0);
    CHECK(open("/new",O_RDONLY) == -1 && errno == ENOENT);

    const char *names[] = { "/one", "/two", "/three" };
    for (unsigned i = 0; i < 3; ++i) {
        fd = open(names[i],O_CREAT | O_RDWR,0600);
        CHECK(fd == 3 && close(fd) == 0);
    }
    CHECK(open("/four",O_CREAT | O_RDWR,0600) == -1 && errno == ENOSPC);
    return 0;
}
