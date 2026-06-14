#include "egw_serial.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>

#define WRITE_BUF_SIZE  4096

struct egw_serial {
    int                 fd;
    char               *path_copy;
    egw_serial_params_t params;

    unsigned char       write_buf[WRITE_BUF_SIZE];
    size_t              write_len;
};

/* ── open ──────────────────────────────────────── */

egw_err_t egw_serial_open(const egw_serial_params_t *params,
                           egw_serial_t **tp)
{
    if (!params || !tp) {
        return EGW_ERR_INVAL;
    }

    struct egw_serial *s = calloc(1, sizeof(*s));
    if (!s) {
        return EGW_ERR_NOMEM;
    }

    s->fd = open(params->path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (s->fd < 0) {
        free(s);
        return EGW_ERR_OPEN;
    }

    /* termios */
    struct termios tio;
    if (tcgetattr(s->fd, &tio) != 0) {
        close(s->fd);
        free(s);
        return EGW_ERR_OPEN;
    }

    cfmakeraw(&tio);
    tio.c_cflag |= (CLOCAL | CREAD);

    speed_t baud_val = B9600;
    switch (params->baud) {
        case 9600:   baud_val = B9600;   break;
        case 19200:  baud_val = B19200;  break;
        case 38400:  baud_val = B38400;  break;
        case 57600:  baud_val = B57600;  break;
        case 115200: baud_val = B115200; break;
        default:     baud_val = B9600;   break;
    }
    cfsetispeed(&tio, baud_val);
    cfsetospeed(&tio, baud_val);

    tio.c_cflag &= ~CSIZE;
    switch (params->data_bits) {
        case 5: tio.c_cflag |= CS5; break;
        case 6: tio.c_cflag |= CS6; break;
        case 7: tio.c_cflag |= CS7; break;
        case 8: default: tio.c_cflag |= CS8; break;
    }

    switch (params->parity) {
        case 'E': case 'e':
            tio.c_cflag |= PARENB;
            tio.c_cflag &= ~PARODD;
            break;
        case 'O': case 'o':
            tio.c_cflag |= PARENB | PARODD;
            break;
        case 'N': default:
            tio.c_cflag &= ~PARENB;
            break;
    }

    switch (params->stop_bits) {
        case 2: tio.c_cflag |= CSTOPB; break;
        case 1: default: tio.c_cflag &= ~CSTOPB; break;
    }

    tio.c_iflag &= ~(IXON | IXOFF | IXANY);
    tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tio.c_oflag &= ~OPOST;

    tio.c_cc[VMIN]  = 1;
    tio.c_cc[VTIME] = 0;

    if (tcsetattr(s->fd, TCSANOW, &tio) != 0) {
        close(s->fd);
        free(s);
        return EGW_ERR_OPEN;
    }

    tcflush(s->fd, TCIOFLUSH);

    s->path_copy = strdup(params->path);
    if (!s->path_copy) {
        close(s->fd);
        free(s);
        return EGW_ERR_NOMEM;
    }

    s->params = *params;
    s->params.path = s->path_copy;

    *tp = s;
    return EGW_OK;
}

/* ── close ─────────────────────────────────────── */

void egw_serial_close(egw_serial_t *tp)
{
    if (!tp) {
        return;
    }

    if (tp->fd >= 0) {
        close(tp->fd);
        tp->fd = -1;
    }

    free(tp->path_copy);
    free(tp);
}

/* ── get_fd ────────────────────────────────────── */

int egw_serial_get_fd(const egw_serial_t *tp)
{
    return tp ? tp->fd : -1;
}

/* ── read ──────────────────────────────────────── */

egw_err_t egw_serial_read(egw_serial_t *tp, void *buf,
                           size_t *len, size_t cap)
{
    if (!tp || !buf || !len || cap == 0) {
        return EGW_ERR_INVAL;
    }

    ssize_t n = read(tp->fd, buf, cap);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *len = 0;
            return EGW_OK;
        }
        return EGW_ERR_READ;
    }

    if (n == 0) {
        return EGW_ERR_READ;
    }

    *len = (size_t)n;
    return EGW_OK;
}

/* ── write (enqueue) ───────────────────────────── */

egw_err_t egw_serial_write(egw_serial_t *tp, const void *buf, size_t len)
{
    if (!tp || !buf || len == 0) {
        return EGW_ERR_INVAL;
    }

    if (len > WRITE_BUF_SIZE - tp->write_len) {
        return EGW_ERR_BUSY;
    }

    memcpy(tp->write_buf + tp->write_len, buf, len);
    tp->write_len += len;

    return EGW_OK;
}

/* ── flush ─────────────────────────────────────── */

egw_err_t egw_serial_flush(egw_serial_t *tp)
{
    if (!tp) {
        return EGW_ERR_INVAL;
    }

    while (tp->write_len > 0)
    {
        ssize_t n = write(tp->fd, tp->write_buf, tp->write_len);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            return EGW_ERR_WRITE;
        }
        if (n > 0) {
            size_t remaining = tp->write_len - (size_t)n;
            memmove(tp->write_buf, tp->write_buf + n, remaining);
            tp->write_len = remaining;
        }
    }

    return EGW_OK;
}

/* ── has_pending ────────────────────────────────── */

bool egw_serial_has_pending(const egw_serial_t *tp)
{
    return tp ? (tp->write_len > 0) : false;
}
