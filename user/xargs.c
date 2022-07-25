#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

int
readline(char **new_argv, int offset) {
    char buf[1024];
    int n = 0;
    while (read(0, buf + n, 1)) {
        if (buf[n] == '\n') {
            break;
        }
        n++;
    }
    buf[n] = 0;
    int i = 0, j = 0;
    while (i < n) {
        if (buf[i] == ' ') {
            buf[i] = 0;
            new_argv[offset++] = buf + j;
            j = i + 1;
        }
        i++;
    }
    new_argv[offset++] = buf + j;
    new_argv[offset] = 0;
    return n;
}

int
main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "usage:xargs command argument...\n");
        exit(1);
    }
    char *new_argv[MAXARG];
    for (int i = 1; i < argc; i++) {
        new_argv[i - 1] = argv[i];
    }
    int offset = argc - 1;
    while (readline(new_argv, offset)) {
        if (fork() == 0) {
            exec(new_argv[0], new_argv);
            fprintf(2, "fail to exec\n");
            exit(1);
        } else {
            wait(0);
        }
    }
    exit(0);
}