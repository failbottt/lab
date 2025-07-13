#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>

#include "sys.h"

ssize_t sys_read(int fd, const void* buf, long count)
{
    ssize_t ret = 0;

    asm volatile (
        "mov $0, %%rax\n\t"    // syscall number for read = 0
        "syscall"
        : "=a"(ret)
        : "D"(fd), "S"(buf), "d"(count)
        : "rcx", "r11", "memory"
    );

    return(ret);
}

long sys_write(int fd, const void* buf, long count)
{
    long ret = 0;
    asm volatile (
        "syscall"
        : "=a"(ret)
        : "a"(1),                 // syscall number → rax
          "D"(fd),                // fd             → rdi
          "S"(buf),               // buf            → rsi
          "d"(count)              // count          → rdx
        : "rcx", "r11", "memory"
    );
    return(ret);
}

long sys_open(const char *pathname, long flags, long mode) {
    long ret = 0;
    register long r10 __asm__("r10") = mode;

    long at_fdcwd = -100L;

    asm volatile (
        "mov $257, %%rax\n\t"     // syscall: openat
        "mov %3, %%r10\n\t"       // manually move to r10
        "syscall"
        : "=a"(ret)
        : "D"(at_fdcwd), "S"(pathname), "d"(flags), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return(ret);
}


void sys_exit(int code)
{
    asm volatile (
            "mov $60, %%rax\n\t" // SYS_exit = 60
            "mov %0,  %%rdi\n\t" // exit code
            "syscall"
            : /* no outputs */
            : "r"((long)code)
            : "rax", "rdi"
            );

    __builtin_unreachable();
}
