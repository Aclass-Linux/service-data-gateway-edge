#include "egw_transport.h"
#include "egw_transport_internal.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

/* ── static: 非阻塞 connect ───────────────────────────── */

static egw_err_t open_tcp(struct egw_transport_handle *h, const void *params)
{
    const egw_transport_tcp_params_t *p = params;
    if (!h || !p) {
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
    if (rc == 0 || (rc < 0 && errno == EINPROGRESS)) {
        h->fd = fd;
        return EGW_OK;
    }

    close(fd);
    return EGW_RET_CODE(ERR_OPEN);
}

/* ── static: read ─────────────────────────────────────── */

static egw_err_t read_tcp(struct egw_transport_handle *h, void *buf,
                           size_t *out_len, size_t cap)
{
    if (!h || !buf || !out_len || cap == 0) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }

    ssize_t n = read(h->fd, buf, cap);
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

/* ── static: write ────────────────────────────────────── */

static egw_err_t write_tcp(struct egw_transport_handle *h, const void *data,
                            size_t *out_written, size_t len)
{
    if (!h || !data || !out_written || len == 0) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }

    ssize_t n = write(h->fd, data, len);
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

/* ── static: close (not free) ─────────────────────────── */

static void close_tcp(struct egw_transport_handle *h)
{
    if (!h || h->fd < 0) {
        return;
    }
    close(h->fd);
    h->fd = -1;
}

/* ── 公共 API ─────────────────────────────────────────── */

egw_transport_handle_t *egw_transport_tcp_open(const egw_transport_tcp_params_t *params)
{
    if (!params) { return NULL; }

    struct egw_transport_handle *h = calloc(1, sizeof(*h));
    if (!h) { return NULL; }

    h->fd    = -1;
    h->read  = read_tcp;
    h->write = write_tcp;
    h->close = close_tcp;

    if (open_tcp(h, params) != EGW_OK) {
        free(h);
        return NULL;
    }

    return h;
}
