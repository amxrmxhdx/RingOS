# RingOS User Program SDK

This folder now supports building custom C user programs for RingOS.

## Quick Start

Build sample program:

```bash
./tools/build-userprog.sh programs/hello.c filesystem/userland/apps/HELLO.BIN
```

Rebuild disk image and run:

```bash
make
./buildrun-silicone.sh
```

Inside RingOS shell:

```text
cd APPS
exec HELLO.BIN
```

## Build Your Own Program

Create a new C file, for example `programs/myapp.c`:

```c
#include "clib.h"

int main(void) {
    clib_puts("My app!\\n");
    return 0;
}
```

Compile it:

```bash
./tools/build-userprog.sh programs/myapp.c filesystem/userland/apps/MYAPP.BIN
```

Then rebuild image (`make`) and run with `exec MYAPP.BIN`.

## Available clib Functions

- `clib_putc(char c)`
- `clib_puts(const char* s)`
- `clib_exit(int status)`
- `clib_strlen(const char* s)`
- `clib_memcpy(void* dst, const void* src, size_t n)`
- `clib_memset(void* dst, int value, size_t n)`
- `clib_open(const char* path, int mode)`
- `clib_read(int fd, char* buf, int size)`
- `clib_write(int fd, const char* buf, int size)`
- `clib_close(int fd)`

Modes:

- `CLIB_O_READ` (0)
- `CLIB_O_WRITE` (1)
