#ifndef _FAILBOT_SYS_H
#define _FAILBOT_SYS_H

#include <fcntl.h>

#define STDIN  0
#define STDOUT 1
#define STDERR 2

// sys calls
long sys_write(int fd, const void *buf, long count);
long sys_read(int fd, const void *buf, long count);
long sys_open(const char *pathname, long flags, long mode);
void sys_exit(int code);

// strings
static long strlen(const char *s)
{
    const char *p = s;
    while(*p) p++;
    return p - s;
}

static long _strcmp(const char *a, const char *b)
{
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

#endif
