#ifndef RINGOS_USER_CLIB_H
#define RINGOS_USER_CLIB_H

/* Minimal user-space C library for RingOS flat binaries. */

typedef unsigned int uint32_t;
typedef unsigned int size_t;

#define CLIB_O_READ  0
#define CLIB_O_WRITE 1

/* Syscall wrappers */
void clib_exit(int status);
void clib_putc(char c);
void clib_puts(const char* s);
int clib_open(const char* path, int mode);
int clib_read(int fd, char* buf, int size);
int clib_write(int fd, const char* buf, int size);
int clib_close(int fd);

/* Tiny utility functions */
size_t clib_strlen(const char* s);
void* clib_memcpy(void* dst, const void* src, size_t n);
void* clib_memset(void* dst, int value, size_t n);

#endif
