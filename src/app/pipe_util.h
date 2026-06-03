#ifndef PIPE_UTIL_H
#define PIPE_UTIL_H

#include <unistd.h>

int pipe_util_create(int fds[2]);
ssize_t pipe_util_write(int fd, const void *buf, size_t count);
ssize_t pipe_util_read(int fd, void *buf, size_t count);
void pipe_util_close(int fd);

#endif
