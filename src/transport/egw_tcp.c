#include "egw_tcp.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

/* ── open ─────────────────────────────────────────────── */

static egw_err_t open_tcp(const void *params, int *out_fd)
{
    const egw_tcp_params_t *p = params;
    if (!p || !out_fd) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(p->port);

    if (inet_pton(AF_INET, p->host, &addr.sin_addr) != 1) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }

    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd < 0) {
        return EGW_RET_CODE(ERR_OPEN);
    }

    int rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc == 0) {
        *out_fd = fd;
        return EGW_OK;
    }

    if (errno == EINPROGRESS) {
        *out_fd = fd;
        return EGW_OK;
    }

    close(fd);
    return EGW_RET_CODE(ERR_OPEN);
}

/* ── close ────────────────────────────────────────────── */

static void close_tcp(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}

/* ── read ─────────────────────────────────────────────── */

static egw_err_t read_tcp(int fd, void *buf, size_t *out_len, size_t cap)
{
    if (!buf || !out_len || cap == 0) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }

    ssize_t n = read(fd, buf, cap);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *out_len = 0;
            return EGW_OK;
        }
        return EGW_RET_CODE(ERR_READ);
    }

    if (n == 0) {
        return EGW_RET_CODE(ERR_READ);
    }

    *out_len = (size_t)n;
    return EGW_OK;
}

/* ── write ────────────────────────────────────────────── */

static egw_err_t write_tcp(int fd, const void *data, size_t *out_written, size_t len)
{
    if (!data || !out_written || len == 0) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }

    ssize_t n = write(fd, data, len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *out_written = 0;
            return EGW_OK;
        }
        return EGW_RET_CODE(ERR_WRITE);
    }

    *out_written = (size_t)n;
    return EGW_OK;
}

/* ── vtable ────────────────────────────────────────────── */

static const struct egw_transport tcp_vtable = {
    .open  = open_tcp,
    .close = close_tcp,
    .read  = read_tcp,
    .write = write_tcp,
};

const struct egw_transport *egw_tcp_vtable(void)
{
    return &tcp_vtable;
}
