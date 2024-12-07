#ifndef SYSTEM_H
#define SYSTEM_H

#define SYSCALL_EXIT  0
#define SYSCALL_PRINT 1

void init_syscalls(void);
void syscall_handler(void);

#endif