#include "egw_serial.h"
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

/* ── open ─────────────────────────────────────────────── */

static egw_err_t open_serial(const void *params, int *out_fd)
{
    const egw_serial_params_t *p = params;
    if (!p || !out_fd) {
        return EGW_RET_CODE(ERR_INVALID_ARG);
    }

    int fd = open(p->path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return EGW_RET_CODE(ERR_OPEN);
    }

    struct termios tio;
    if (tcgetattr(fd, &tio) != 0) {
        close(fd);
        return EGW_RET_CODE(ERR_OPEN);
    }

    cfmakeraw(&tio);
    tio.c_cflag |= (CLOCAL | CREAD);

    speed_t baud_val = B9600;
    switch (p->baud) {
    case 9600:   baud_val = B9600;   break;
    case 19200:  baud_val = B19200;  break;
    case 38400:  baud_val = B38400;  break;
    case 57600:  baud_val = B57600;  break;
    case 115200: baud_val = B115200; break;
    }
    cfsetispeed(&tio, baud_val);
    cfsetospeed(&tio, baud_val);

    tio.c_cflag &= ~CSIZE;
    switch (p->data_bits) {
    case 5: tio.c_cflag |= CS5; break;
    case 6: tio.c_cflag |= CS6; break;
    case 7: tio.c_cflag |= CS7; break;
    case 8: default: tio.c_cflag |= CS8; break;
    }

    switch (p->parity) {
    case 'E': case 'e':
        tio.c_cflag |= PARENB;
        tio.c_cflag &= ~PARODD;
        break;
    case 'O': case 'o':
        tio.c_cflag |= PARENB | PARODD;
        break;
    default:
        tio.c_cflag &= ~PARENB;
        break;
    }

    switch (p->stop_bits) {
    case 2: tio.c_cflag |= CSTOPB; break;
    default: tio.c_cflag &= ~CSTOPB; break;
    }

    tio.c_iflag &= ~(IXON | IXOFF | IXANY);
    tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tio.c_oflag &= ~OPOST;

    tio.c_cc[VMIN]  = 1;
    tio.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        close(fd);
        return EGW_RET_CODE(ERR_OPEN);
    }

    tcflush(fd, TCIOFLUSH);

    *out_fd = fd;
    return EGW_OK;
}

/* ── close ────────────────────────────────────────────── */

static void close_serial(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}

/* ── read ─────────────────────────────────────────────── */

static egw_err_t read_serial(int fd, void *buf, size_t *out_len, size_t cap)
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

static egw_err_t write_serial(int fd, const void *data, size_t *out_written, size_t len)
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

static const struct egw_transport serial_vtable = {
    .open  = open_serial,
    .close = close_serial,
    .read  = read_serial,
    .write = write_serial,
};

const struct egw_transport *egw_serial_vtable(void)
{
    return &serial_vtable;
}
