#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
new_proc(int r) {
    int prime;
    if (read(r, &prime, 4)) {
        fprintf(1, "prime %d\n", prime);
    } else {
        exit(0);
    }
    int p[2];
    pipe(p);
    if (fork() == 0) {
        close(r);
        close(p[1]);
        new_proc(p[0]);
    } else {
        close(p[0]);
        int n;
        while (read(r, &n, 4)) {
            if (n % prime) {
                if (write(p[1], &n, 4) != 4) {
                    fprintf(2, "fail to write\n");
                    exit(1);
                }
            }
        }
        close(r);
        close(p[1]);
        wait(0);
    }
    exit(0);
}

int 
main(int argc, char *argv[]) {
    if (argc != 1) {
        fprintf(2, "usage:primes\n");
        exit(1);
    }
    int p[2];
    pipe(p);
    if (fork() == 0) {
        close(p[1]);
        new_proc(p[0]);
    } else {
        close(p[0]);
        for (int i = 2; i <= 35; i++) {
            if (write(p[1], &i, 4) != 4) {
                fprintf(2, "fail to write\n");
                exit(1);
            }
        }
        close(p[1]);
        wait(0);
    }
    exit(0);
}