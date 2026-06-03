#include "data_gen.h"
#include "pipe_util.h"
#include "fifo_writer.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define FIFO_PATH "/tmp/temp_fifo"

int main(void) {
    int pipe_fds[2];

    if (pipe_util_create(pipe_fds) == -1) {
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        /* child: data generator → write to anonymous pipe */
        pipe_util_close(pipe_fds[0]);
        data_gen_init();

        while (1) {
            double temp = data_gen_get_temperature();
            if (pipe_util_write(pipe_fds[1], &temp, sizeof(temp)) == -1) {
                break;
            }
            sleep(1);
        }

        pipe_util_close(pipe_fds[1]);
        exit(0);
    }

    /* parent: read from anonymous pipe → write to FIFO */
    pipe_util_close(pipe_fds[1]);

    if (fifo_writer_init(FIFO_PATH) == -1) {
        kill(pid, SIGTERM);
        wait(NULL);
        return 1;
    }

    double temp;
    while (pipe_util_read(pipe_fds[0], &temp, sizeof(temp)) > 0) {
        fifo_writer_write(temp);
    }

    fifo_writer_cleanup();
    pipe_util_close(pipe_fds[0]);
    wait(NULL);
    return 0;
}
