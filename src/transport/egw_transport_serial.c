#include "egw_transport.h"
#include "egw_transport_internal.h"
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

/* ── static: 打开 fd + termios ───────────────────────── */

static egw_err_t open_serial(struct egw_transport_handle *h, const void *params)
{
    const egw_transport_serial_params_t *p = params;
    if (!h || !p) {
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
    default:     close(fd);          return EGW_RET_CODE(ERR_INVALID_ARG);
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
    /* Linux termios 历史遗留：
     *
     * VMIN/VTIME 起源于 1970s UNIX 串口驱动，原设计用于阻塞式 read。
     * 在 O_NONBLOCK + epoll 模式下行为发生偏移：
     *   VMIN=1   = 有数据时尽量填满用户缓冲区，减少系统调用次数
     *   VTIME=0  = 不启用间隔定时器
     *
     * O_NONBLOCK 保证无数据时立即返回 EAGAIN，不会被 VMIN=1 阻塞。
     * 二者配合：fd 可读时一次 read 尽可能多取，空缓冲时立刻返回给 epoll。
     * 反之 VMIN=0,VTIME=0 会导致每次只取 1 字节，增加 epoll 重入次数。
     */

    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        close(fd);
        return EGW_RET_CODE(ERR_OPEN);
    }

    tcflush(fd, TCIOFLUSH);

    h->fd = fd;
    return EGW_OK;
}

/* ── static: read ─────────────────────────────────────── */

static egw_err_t read_serial(struct egw_transport_handle *h, void *buf,
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

static egw_err_t write_serial(struct egw_transport_handle *h, const void *data,
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

/* ── static: close(not free) ────────────────────────── */

static void close_serial(struct egw_transport_handle *h)
{
    if (!h || h->fd < 0) {
        return;
    }
    close(h->fd);
    h->fd = -1;
}

/* ── 公共 API ─────────────────────────────────────────── */

egw_transport_handle_t *egw_transport_serial_open(const egw_transport_serial_params_t *params)
{
    if (!params) { return NULL; }

    struct egw_transport_handle *h = calloc(1, sizeof(*h));
    if (!h) { return NULL; }

    h->fd    = -1;
    h->read  = read_serial;
    h->write = write_serial;
    h->close = close_serial;

    if (open_serial(h, params) != EGW_OK) {
        free(h);
        return NULL;
    }

    return h;
}
