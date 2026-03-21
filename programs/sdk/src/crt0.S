.global _start
.extern main
.extern clib_exit

.section .text
_start:
    call main
    push %eax
    call clib_exit
1:
    hlt
    jmp 1b
