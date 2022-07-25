#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[]) {
    if (argc != 1) {
        fprintf(2, "usage:pingpong\n");
        exit(1);
    }
    int p[2];
    pipe(p);
    char buf[2];
    if (fork() == 0) {
        if (read(p[0], buf, 1) == 1) {
            fprintf(1, "%d: received ping\n", getpid());
        } else {
            fprintf(2, "fail to read in child\n");
            exit(1);
        }
        close(p[0]);
        if (write(p[1], "pong", 1) != 1) {
            fprintf(2, "fail to write in child\n");
            exit(1);
        }
        close(p[1]);
    } else {
        if (write(p[1], "ping", 1) != 1) {
            fprintf(2, "fail to write in parent\n");
            exit(1);
        }
        close(p[1]);
        if (read(p[0], buf, 1) == 1) {
            fprintf(1, "%d: received pong\n", getpid());
        } else {
            fprintf(2, "fail to read in child\n");
            exit(1);
        }
        close(p[0]);
    }
    exit(0);
}