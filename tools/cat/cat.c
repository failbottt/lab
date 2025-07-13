#include "../runtime/sys.h"

#include <fcntl.h>
#include <stdint.h>

long main_start(uintptr_t *rsp)
{
    long argc = (long)rsp[0];
    char **argv = (char **)(rsp + 1);

    int fd = STDIN;
    if (argc >= 2)
    {
        fd = sys_open(argv[1], O_RDONLY, 0);
        if (fd < 0)
        {
            static const char err[] = "Could not open file\n";
            sys_write(STDERR, err, sizeof(err)-1);
            return 1;
        }
    }

    if (fd < 0)
    {
        static const char err[] = "Unable to open file.";
        sys_write(STDERR, err, sizeof(err)-1);
        return 1;
    }

    // read from file
    long s = 4096;
    const char buffer[s];

    long done = 0;
    while (1)
    {
        int n = sys_read(fd, buffer, s);
        if (n == 0)
        {
            break;
        }

        if (n < 0)
        {
           static const char err[] = "error reading\n";
           sys_write(STDERR, err, sizeof(err)-1);
           return n;
        }
        // static const char msg[] = "foo bar test\n";
        sys_write(STDOUT, buffer, n);

        done += n;
    }

    return 0;
}
