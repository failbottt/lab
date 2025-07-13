#include "../runtime/sys.h"

#include <stdint.h>

long main_start(uintptr_t *rsp)
{
    long argc = (long)rsp[0];
    char **argv = (char **)(rsp + 1);

    if (argc < 2)
    {
        sys_write(STDOUT, "\n", 1);
        return 0;
    }

    int skip_new_line_char = 0;

    // i = 1 to skip the program name string
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            if ((_strcmp(argv[i], "-n")) == 0)
            {
                skip_new_line_char = 1;
                continue;
            }
        }

        int written = 0;
        int len = strlen(argv[i]);

        while (written < len)
        {
            int n = sys_write(STDOUT, argv[i] + written, len - written);
            if (n == 0) break;
            if (n < 0)
            {
                const char err[] = "sys_write failed with error";
                sys_write(STDERR, err, sizeof(err)-1);
                return(n);
            }

            written += n;
        }


        if (i != argc-1)
        {
            sys_write(STDOUT, " ", 1);
        }
    }

    if (!skip_new_line_char)
    {
        sys_write(STDOUT, "\n", 1);
    }

    return(0);
}
