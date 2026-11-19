#include "fxcg/heap.h"
#include <unistd.h>

ssize_t read(int fd, void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    (void)count;
    return -1;
}

ssize_t write(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    (void)count;
    return -1;
}

int close(int fd)
{
    (void)fd;
    return -1;
}

off_t lseek(int fd, off_t offset, int whence)
{
    (void)fd;
    (void)offset;
    (void)whence;
    return -1;
}

void *fxlibc_hal_malloc(size_t size)
{
    return sys_malloc(size);
}

void fxlibc_hal_free(void *ptr)
{
    sys_free(ptr);
}

void *fxlibc_hal_realloc(void *ptr, size_t size)
{
    return sys_realloc(ptr, size);
}

void kfree(void *ptr)
{
    sys_free(ptr);
}
