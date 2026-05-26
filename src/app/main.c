#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/wait.h>

#define TEST_FIFO_NAME "./testfifo"




int main(void) 
{
    if ((mkfifo(TEST_FIFO_NAME, 0666) == -1) 
                                    && (errno != EEXIST)) {
        perror("mkfifo");
        return 1;
    }

    for (size_t depth = 0; depth < 5; depth++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(1);
        }
        if (pid == 0) {
            char log[1000];
            log[999] = '\0';
            int log_len = snprintf(log, 999,
                        "Level %zu: PID=%d, Parent=%d\n",
                        depth, getpid(), getppid());
            int fifo_fd = open(TEST_FIFO_NAME, O_WRONLY);
            if (fifo_fd == -1) {
                _exit(1);
            }
            if (write(fifo_fd, log, log_len) != log_len) {
                _exit(1);
            }
            close(fifo_fd);  // 可选，_exit 会关闭
            _exit(0);
        } else {
            char log[1000];
            log[999] = '\0';
            int fifo_fd = open(TEST_FIFO_NAME, O_RDONLY);
            if (fifo_fd == -1) {
                    exit(1);
                }
            int log_len = read(fifo_fd, log, 999);
            if (log_len == -1) {
                perror("readerror");
                close(fifo_fd);
                exit(1);
            }
            log[log_len] = '\0';
            printf("recv data :\n %s", log);
            close(fifo_fd);   // 修复泄漏
            wait(NULL);
        }
    }
    unlink(TEST_FIFO_NAME);
    return 0;
}

