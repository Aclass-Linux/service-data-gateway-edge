#include "egw_transport.h"
#include "egw_serial.h"
#include "egw_transport_internal.h"

#include <uv.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

struct egw_serial {
    struct egw_transport_header hdr;
    uv_pipe_t       pipe;
    uv_write_t      write_req;
    unsigned char  *write_buf;
    char           *path_copy;
    int             fd;
    bool            writing;
    egw_serial_params_t params;
};

/* ── libuv 回调 ──────────────────────────────── */

static void egw_serial_on_alloc(uv_handle_t *handle, size_t suggested_size,
                                 uv_buf_t *buf) {
    (void)handle;
    buf->base = malloc(suggested_size);
    buf->len = (buf->base) ? (unsigned long)suggested_size : 0;
}

static void egw_serial_on_read(uv_stream_t *stream, ssize_t nread,
                                const uv_buf_t *buf) {
    struct egw_serial *serial = (struct egw_serial *)stream->data;
    struct egw_transport_header *hdr = &serial->hdr;

    if (nread < 0) {
        if (hdr->cbs.on_close) {
            egw_err_t err = (nread == UV_EOF) ? EGW_OK : EGW_ERR_READ;
            hdr->cbs.on_close(hdr, err);
        }
        free(buf->base);
        hdr->close_fn(hdr);
        return;
    }

    if (nread > 0 && hdr->cbs.on_data) {
        hdr->cbs.on_data(hdr, buf->base, (size_t)nread);
    }

    free(buf->base);
}

static void egw_serial_on_write_done(uv_write_t *req, int status) {
    struct egw_serial *serial = (struct egw_serial *)req->data;
    struct egw_transport_header *hdr = &serial->hdr;

    free(serial->write_buf);
    serial->write_buf = NULL;
    serial->writing = false;

    if (hdr->cbs.on_write) {
        egw_err_t err = (status == 0) ? EGW_OK : EGW_ERR_WRITE;
        hdr->cbs.on_write(hdr, err);
    }
}

static void egw_serial_on_close_handle(uv_handle_t *handle) {
    struct egw_serial *serial = (struct egw_serial *)handle->data;
    struct egw_transport_header *hdr = &serial->hdr;

    egw_transport_remove(hdr->inst, hdr);

    if (serial->fd >= 0) {
        close(serial->fd);
        serial->fd = -1;
    }
    serial->hdr.opened = false;

    if (hdr->cbs.on_close) {
        hdr->cbs.on_close(hdr, EGW_OK);
    }

    free(serial->path_copy);
    free(serial);
}

/* ── termios 配置 ────────────────────────────── */

static egw_err_t egw_serial_set_termios(int fd,
                                         const egw_serial_params_t *params) {
    struct termios tio;
    if (tcgetattr(fd, &tio) != 0) {
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

    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        return EGW_ERR_OPEN;
    }

    tcflush(fd, TCIOFLUSH);
    return EGW_OK;
}

/* ── 打开 ────────────────────────────────────── */

static egw_err_t egw_serial_open_fn(struct egw_transport_header *hdr) {
    struct egw_serial *serial = (struct egw_serial *)hdr;
    uv_loop_t *loop = &hdr->inst->loop;

    serial->fd = open(serial->path_copy, O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial->fd < 0) {
        if (hdr->cbs.on_open) {
            hdr->cbs.on_open(hdr, EGW_ERR_OPEN);
        }
        return EGW_ERR_OPEN;
    }

    if (fcntl(serial->fd, F_SETFL, 0) != 0) {
        close(serial->fd);
        serial->fd = -1;
        if (hdr->cbs.on_open) {
            hdr->cbs.on_open(hdr, EGW_ERR_OPEN);
        }
        return EGW_ERR_OPEN;
    }

    egw_err_t terr = egw_serial_set_termios(serial->fd, &serial->params);
    if (terr != EGW_OK) {
        close(serial->fd);
        serial->fd = -1;
        if (hdr->cbs.on_open) {
            hdr->cbs.on_open(hdr, terr);
        }
        return terr;
    }

    int rc = uv_pipe_init(loop, &serial->pipe, 0);
    if (rc != 0) {
        close(serial->fd);
        serial->fd = -1;
        if (hdr->cbs.on_open) {
            hdr->cbs.on_open(hdr, EGW_ERR_OPEN);
        }
        return EGW_ERR_OPEN;
    }

    rc = uv_pipe_open(&serial->pipe, serial->fd);
    if (rc != 0) {
        uv_close((uv_handle_t *)&serial->pipe, NULL);
        close(serial->fd);
        serial->fd = -1;
        if (hdr->cbs.on_open) {
            hdr->cbs.on_open(hdr, EGW_ERR_OPEN);
        }
        return EGW_ERR_OPEN;
    }

    serial->pipe.data = serial;
    hdr->opened = true;

    rc = uv_read_start((uv_stream_t *)&serial->pipe,
                        egw_serial_on_alloc, egw_serial_on_read);
    if (rc != 0) {
        uv_close((uv_handle_t *)&serial->pipe, NULL);
        close(serial->fd);
        serial->fd = -1;
        hdr->opened = false;
        if (hdr->cbs.on_open) {
            hdr->cbs.on_open(hdr, EGW_ERR_READ);
        }
        return EGW_ERR_READ;
    }

    if (hdr->cbs.on_open) {
        hdr->cbs.on_open(hdr, EGW_OK);
    }

    return EGW_OK;
}

/* ── 关闭 ────────────────────────────────────── */

static void egw_serial_close_fn(struct egw_transport_header *hdr) {
    struct egw_serial *serial = (struct egw_serial *)hdr;

    if (!hdr->opened) {
        return;
    }

    uv_read_stop((uv_stream_t *)&serial->pipe);
    hdr->opened = false;
    uv_close((uv_handle_t *)&serial->pipe, egw_serial_on_close_handle);
}

/* ── 写 ──────────────────────────────────────── */

egw_err_t egw_serial_write(egw_serial_t *tp, const void *buf, size_t len) {
    if (!tp || !buf || len == 0) {
        return EGW_ERR_INVAL;
    }

    struct egw_serial *serial = (struct egw_serial *)tp;
    if (!serial->hdr.opened) {
        return EGW_ERR_CLOSE;
    }

    if (serial->writing) {
        return EGW_ERR_BUSY;
    }

    serial->write_buf = malloc(len);
    if (!serial->write_buf) {
        return EGW_ERR_INVAL;
    }

    memcpy(serial->write_buf, buf, len);
    serial->writing = true;
    serial->write_req.data = serial;

    uv_buf_t uvbuf = uv_buf_init((char *)serial->write_buf,
                                  (unsigned int)len);
    int rc = uv_write(&serial->write_req, (uv_stream_t *)&serial->pipe,
                       &uvbuf, 1, egw_serial_on_write_done);
    if (rc != 0) {
        free(serial->write_buf);
        serial->write_buf = NULL;
        serial->writing = false;
        return EGW_ERR_WRITE;
    }

    return EGW_OK;
}

/* ── 关闭（公开 API）────────────────────────────── */

egw_err_t egw_serial_close(egw_serial_t *tp) {
    if (!tp || !((struct egw_serial *)tp)->hdr.inst) {
        return EGW_ERR_INVAL;
    }

    struct egw_serial *serial = (struct egw_serial *)tp;
    egw_serial_close_fn(&serial->hdr);
    return EGW_OK;
}

/* ── 注册 ────────────────────────────────────── */

egw_err_t egw_serial_register(egw_transport_instance_t *inst,
                               const egw_serial_params_t *params,
                               const egw_transport_cbs_t *cbs,
                               egw_serial_t **tp) {
    if (!inst || !params || !cbs || !tp) {
        return EGW_ERR_INVAL;
    }
    if (!params->path) {
        return EGW_ERR_INVAL;
    }

    struct egw_serial *serial = calloc(1, sizeof(*serial));
    if (!serial) {
        return EGW_ERR_NOMEM;
    }

    serial->hdr.open_fn  = egw_serial_open_fn;
    serial->hdr.close_fn = egw_serial_close_fn;
    serial->hdr.cbs      = *cbs;
    serial->fd           = -1;
    serial->writing      = false;

    serial->path_copy = strdup(params->path);
    if (!serial->path_copy) {
        free(serial);
        return EGW_ERR_NOMEM;
    }

    serial->params.path      = serial->path_copy;
    serial->params.baud      = params->baud;
    serial->params.parity    = params->parity;
    serial->params.data_bits = params->data_bits;
    serial->params.stop_bits = params->stop_bits;

    egw_err_t err = egw_transport_add(inst, &serial->hdr);
    if (err != EGW_OK) {
        free(serial->path_copy);
        free(serial);
        return err;
    }

    *tp = (egw_serial_t *)serial;
    egw_transport_do_open(&serial->hdr);

    return EGW_OK;
}
