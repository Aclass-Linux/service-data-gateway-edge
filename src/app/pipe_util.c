#include "pipe_util.h"
#include <stdio.h>
#include <unistd.h>

int pipe_util_create(int fds[2]) {
    if (pipe(fds) == -1) {
        perror("pipe");
        return -1;
    }
    return 0;
}

ssize_t pipe_util_write(int fd, const void *buf, size_t count) {
    return write(fd, buf, count);
}

ssize_t pipe_util_read(int fd, void *buf, size_t count) {
    return read(fd, buf, count);
}

void pipe_util_close(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}
