#include "fifo_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

static const char *fifo_path = NULL;
static int fifo_fd = -1;

static void sigpipe_handler(int sig) {
    (void)sig;
    fprintf(stderr, "SIGPIPE: FIFO reader disconnected, continuing...\n");
}

int fifo_writer_init(const char *path) {
    fifo_path = path;

    if (mkfifo(path, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo");
        return -1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigpipe_handler;
    sigaction(SIGPIPE, &sa, NULL);

    fprintf(stderr, "Waiting for FIFO reader on %s ...\n", path);
    fifo_fd = open(path, O_WRONLY);
    if (fifo_fd == -1) {
        perror("open fifo");
        return -1;
    }
    fprintf(stderr, "FIFO reader connected\n");

    return 0;
}

void fifo_writer_write(double temperature) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%.1f\n", temperature);
    if (write(fifo_fd, buf, (size_t)len) == -1 && errno == EPIPE) {
        fprintf(stderr, "FIFO write failed: reader disconnected\n");
    }
}

void fifo_writer_cleanup(void) {
    if (fifo_fd >= 0) {
        close(fifo_fd);
        fifo_fd = -1;
    }
    if (fifo_path) {
        unlink(fifo_path);
        fifo_path = NULL;
    }
}
