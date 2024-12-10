#ifndef LIBC_FILEIO_H
#define LIBC_FILEIO_H

#include "types.h"

// Initialize the filesystem
bool fs_init(void);

// Open a file
// Mode: 0 = read, 1 = write
int fs_open(const char* path, int mode);

// Read from a file
// Returns the number of bytes read, or -1 on error
int fs_read(int fd, char* buffer, int size);

// Write to a file
// Returns the number of bytes written, or -1 on error
int fs_write(int fd, const char* buffer, int size);

// Close a file
int fs_close(int fd);

// Change the current directory
bool fs_chdir(const char* path);

// List directory contents
// The callback receives the name, size, and attributes of each entry
bool fs_list_directory(void (*callback)(const char* name, uint32_t size, uint8_t attr));

// Get the current working directory
const char* fs_getcwd(void);

// Create a new file
bool fs_create(const char* path);

// Delete a file
bool fs_delete(const char* path);

// Create a new directory
bool fs_mkdir(const char* path);

#endif /* LIBC_FILEIO_H */
