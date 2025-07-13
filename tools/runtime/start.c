// start.c
#include "sys.h"

// No prologue
__attribute__((naked))
void _start(void) {
    asm volatile (
        "mov %%rsp, %%rdi\n\t"    // pass rsp to main_start
        "call main_start\n\t"     // call C logic
        "mov %%rax, %%rdi\n\t"    // exit code
        "mov $60, %%rax\n\t"      // sys_exit
        "syscall"
        :
        :
        : "rdi", "rax"
    );
    // __builtin_unreachable();
}
