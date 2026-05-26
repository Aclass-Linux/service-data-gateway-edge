#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>

/* ============================================================
 * Pipe vs UDS 通讯速度对比测试
 * ============================================================
 * 每次测试发送 64MB 数据，记录吞吐量。
 * 每个消息尺寸都经过：warmup → flush cache → pipe → flush cache → uds
 * ============================================================ */

#define TOTAL_BYTES      (64LL * 1024 * 1024)   /* 每次发送总数据量  */
#define CACHE_FLUSH_BYTES (32LL * 1024 * 1024)   /* 刷 cache 的数据量 */
#define WARMUP_BYTES     (16LL * 1024 * 1024)    /* warmup 数据量    */

static const size_t msg_sizes[] = {
    1, 4, 16, 64, 256, 1024, 4096, 16384, 65536, 262144
};
static const int num_sizes = sizeof(msg_sizes) / sizeof(msg_sizes[0]);

/* ---- 高精度计时 ---- */
static double now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e6 + ts.tv_nsec / 1e3;
}

/* ---- 刷 cache：写入大块数据，迫使 CPU 清空 L3 ---- */
static void flush_cache(void) {
    volatile char *buf = malloc(CACHE_FLUSH_BYTES);
    if (buf) {
        for (size_t i = 0; i < CACHE_FLUSH_BYTES; i += 64)
            buf[i] = (char)i;
        free((void *)buf);
    }
}

/* ---- 单次吞吐量测试（返回 MB/s），fd 是 data 通道的发送端 ---- */
static double run_test(int data_write_fd, int sync_read_fd,
                       size_t msg_size, size_t total_bytes)
{
    char *buf = malloc(msg_size);
    if (!buf) return -1;
    /* 填充数据，防止写时拷贝优化 */
    memset(buf, 0xAB, msg_size);

    double t0 = now_us();
    size_t sent = 0;
    while (sent < total_bytes) {
        ssize_t n = write(data_write_fd, buf, msg_size);
        if (n <= 0) break;
        sent += n;
    }
    /* 等待子进程读完 */
    char ack;
    read(sync_read_fd, &ack, 1);
    double t1 = now_us();

    free(buf);
    double elapsed_s = (t1 - t0) / 1e6;
    return (total_bytes / (1024.0 * 1024.0)) / elapsed_s;
}

/* ---- Pipe 测试 ---- */
static double test_pipe(size_t msg_size) {
    int data_fd[2];
    int sync_fd[2];
    if (pipe(data_fd) < 0 || pipe(sync_fd) < 0) {
        perror("pipe");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }

    if (pid == 0) {
        /* child: 从 data_fd[0] 读，向 sync_fd[1] 写 ack */
        close(data_fd[1]);
        close(sync_fd[0]);

        char *buf = malloc(msg_size > 65536 ? msg_size : 65536);
        if (!buf) _exit(1);
        size_t received = 0;
        while (received < TOTAL_BYTES) {
            ssize_t n = read(data_fd[0], buf, msg_size);
            if (n <= 0) break;
            received += n;
        }
        free(buf);
        write(sync_fd[1], "", 1);
        close(data_fd[0]);
        close(sync_fd[1]);
        _exit(0);
    }

    /* parent: 向 data_fd[1] 写，从 sync_fd[0] 读 ack */
    close(data_fd[0]);
    close(sync_fd[1]);

    double mbps = run_test(data_fd[1], sync_fd[0], msg_size, TOTAL_BYTES);

    close(data_fd[1]);
    close(sync_fd[0]);
    waitpid(pid, NULL, 0);
    return mbps;
}

/* ---- UDS 测试 ---- */
static double test_uds(size_t msg_size) {
    int data_fd[2];
    int sync_fd[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, data_fd) < 0
        || pipe(sync_fd) < 0) {
        perror("socketpair / pipe");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }

    if (pid == 0) {
        /* child: 从 data_fd[1] 读，向 sync_fd[1] 写 ack */
        close(data_fd[0]);
        close(sync_fd[0]);

        char *buf = malloc(msg_size > 65536 ? msg_size : 65536);
        if (!buf) _exit(1);
        size_t received = 0;
        while (received < TOTAL_BYTES) {
            ssize_t n = read(data_fd[1], buf, msg_size);
            if (n <= 0) break;
            received += n;
        }
        free(buf);
        write(sync_fd[1], "", 1);
        close(data_fd[1]);
        close(sync_fd[1]);
        _exit(0);
    }

    /* parent: 向 data_fd[0] 写，从 sync_fd[0] 读 ack */
    close(data_fd[1]);
    close(sync_fd[1]);

    double mbps = run_test(data_fd[0], sync_fd[0], msg_size, TOTAL_BYTES);

    close(data_fd[0]);
    close(sync_fd[0]);
    waitpid(pid, NULL, 0);
    return mbps;
}

/* ---- warmup：跑一次但不记录 ---- */
static void warmup(void) {
    test_pipe(4096);
    test_uds(4096);
}

int main(void) {
    printf("============================================================\n");
    printf("  Pipe vs UDS 吞吐量对比\n");
    printf("  每次测试发送 %lld MB，已屏蔽 cache 影响\n",
           (long long)(TOTAL_BYTES / (1024 * 1024)));
    printf("============================================================\n");
    printf("  %-12s  %12s  %12s  %12s\n",
           "Msg Size", "Pipe (MB/s)", "UDS (MB/s)", "Ratio");
    printf("------------------------------------------------------------\n");

    for (int i = 0; i < num_sizes; i++) {
        size_t sz = msg_sizes[i];

        /* warmup */
        warmup();

        /* pipe */
        flush_cache();
        double pipe_mbps = test_pipe(sz);

        /* uds */
        flush_cache();
        double uds_mbps = test_uds(sz);

        /* ratio */
        const char *winner;
        double ratio;
        if (pipe_mbps >= uds_mbps) {
            ratio = pipe_mbps / uds_mbps;
            winner = "Pipe";
        } else {
            ratio = uds_mbps / pipe_mbps;
            winner = "UDS";
        }

        printf("  %-12zu  %12.1f  %12.1f  %s x%.1f\n",
               sz, pipe_mbps, uds_mbps, winner, ratio);
    }

    printf("============================================================\n");
    return 0;
}
