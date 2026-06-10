/**
 * @file egw_serial.c
 * @brief Transport 串口变种 —— 异步 UART 收发
 *
 * 基于 libuv uv_pipe 实现。termios 由 egw_serial_set_termios 管理，
 * libuv 只负责 I/O 多路复用，不修改串口参数。
 */

#include "egw_serial.h"

#ifdef USE_JSON_CONFIG
#include "config.h"
#endif

#include <uv.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>


/* ── 串口变种结构体（内部）────────────────────── */

typedef struct egw_serial {
    egw_transport_t base;
    uv_pipe_t       pipe;
    uv_write_t      write_req;
    unsigned char  *write_buf;
    char           *path_copy;
    int             fd;
    bool            opened;
    bool            writing;
    egw_serial_params_t params;
} egw_serial_t;

/* ── vtable 前向声明 ────────────────────────────── */

static egw_err_t egw_serial_do_open(egw_transport_t *tp);
static egw_err_t egw_serial_do_close(egw_transport_t *tp);
static egw_err_t egw_serial_do_write(egw_transport_t *tp, const void *buf, size_t len);

static const struct egw_transport_ops egw_serial_ops = {
    .open  = egw_serial_do_open,
    .close = egw_serial_do_close,
    .write = egw_serial_do_write,
};

/* ── libuv 回调 ──────────────────────────────── */

static void egw_serial_on_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    (void)handle;
    buf->base = malloc(suggested_size);
    buf->len = (buf->base) ? (unsigned long)suggested_size : 0;
}

static void egw_serial_on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    egw_serial_t *serial = (egw_serial_t *)stream->data;
    egw_transport_t *tp = &serial->base;

    if (nread < 0) {
        if (tp->cbs.on_close) {
            egw_err_t err = (nread == UV_EOF) ? EGW_OK : EGW_ERR_READ;
            tp->cbs.on_close(tp, err);
        }
        free(buf->base);
        uv_read_stop(stream);
        uv_close((uv_handle_t *)stream, NULL);
        serial->opened = false;
        return;
    }

    if (nread > 0 && tp->cbs.on_data) {
        tp->cbs.on_data(tp, buf->base, (size_t)nread);
    }

    free(buf->base);
}

static void egw_serial_on_write_done(uv_write_t *req, int status) {
    egw_serial_t *serial = (egw_serial_t *)req->data;
    egw_transport_t *tp = &serial->base;

    free(serial->write_buf);
    serial->write_buf = NULL;
    serial->writing = false;

    if (tp->cbs.on_write) {
        egw_err_t err = (status == 0) ? EGW_OK : EGW_ERR_WRITE;
        tp->cbs.on_write(tp, err);
    }
}

static void egw_serial_on_close_handle(uv_handle_t *handle) {
    egw_serial_t *serial = (egw_serial_t *)handle->data;
    egw_transport_t *tp = &serial->base;

    if (serial->fd >= 0) {
        close(serial->fd);
        serial->fd = -1;
    }
    serial->opened = false;

    if (tp->cbs.on_close) {
        tp->cbs.on_close(tp, EGW_OK);
    }

    free(serial->path_copy);
    free(serial);
}

/* ── termios 配置 ────────────────────────────── */

static egw_err_t egw_serial_set_termios(int fd, const egw_serial_params_t *params) {
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

/* ── vtable 实现 ──────────────────────────────── */

static egw_err_t egw_serial_do_open(egw_transport_t *tp) {
    egw_serial_t *serial = (egw_serial_t *)tp;
    uv_loop_t *loop = uv_default_loop();

    serial->fd = open(serial->path_copy, O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial->fd < 0) {
        if (tp->cbs.on_open) {
            tp->cbs.on_open(tp, EGW_ERR_OPEN);
        }
        return EGW_ERR_OPEN;
    }

    if (fcntl(serial->fd, F_SETFL, 0) != 0) {
        close(serial->fd);
        serial->fd = -1;
        if (tp->cbs.on_open) {
            tp->cbs.on_open(tp, EGW_ERR_OPEN);
        }
        return EGW_ERR_OPEN;
    }

    egw_err_t terr = egw_serial_set_termios(serial->fd, &serial->params);
    if (terr != EGW_OK) {
        close(serial->fd);
        serial->fd = -1;
        if (tp->cbs.on_open) {
            tp->cbs.on_open(tp, terr);
        }
        return terr;
    }

    int rc = uv_pipe_init(loop, &serial->pipe, 0);
    if (rc != 0) {
        close(serial->fd);
        serial->fd = -1;
        if (tp->cbs.on_open) {
            tp->cbs.on_open(tp, EGW_ERR_OPEN);
        }
        return EGW_ERR_OPEN;
    }

    rc = uv_pipe_open(&serial->pipe, serial->fd);
    if (rc != 0) {
        uv_close((uv_handle_t *)&serial->pipe, NULL);
        close(serial->fd);
        serial->fd = -1;
        if (tp->cbs.on_open) {
            tp->cbs.on_open(tp, EGW_ERR_OPEN);
        }
        return EGW_ERR_OPEN;
    }

    serial->pipe.data = serial;
    serial->opened = true;

    rc = uv_read_start((uv_stream_t *)&serial->pipe, egw_serial_on_alloc, egw_serial_on_read);
    if (rc != 0) {
        uv_close((uv_handle_t *)&serial->pipe, NULL);
        close(serial->fd);
        serial->fd = -1;
        serial->opened = false;
        if (tp->cbs.on_open) {
            tp->cbs.on_open(tp, EGW_ERR_READ);
        }
        return EGW_ERR_READ;
    }

    if (tp->cbs.on_open) {
        tp->cbs.on_open(tp, EGW_OK);
    }

    return EGW_OK;
}

static egw_err_t egw_serial_do_close(egw_transport_t *tp) {
    if (!tp) {
        return EGW_ERR_INVAL;
    }

    egw_serial_t *serial = (egw_serial_t *)tp;
    if (!serial->opened) {
        return EGW_OK;
    }

    uv_read_stop((uv_stream_t *)&serial->pipe);
    serial->opened = false;
    uv_close((uv_handle_t *)&serial->pipe, egw_serial_on_close_handle);

    return EGW_OK;
}

static egw_err_t egw_serial_do_write(egw_transport_t *tp, const void *buf, size_t len) {
    if (!tp || !buf || len == 0) {
        return EGW_ERR_INVAL;
    }

    egw_serial_t *serial = (egw_serial_t *)tp;
    if (!serial->opened) {
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

    uv_buf_t uvbuf = uv_buf_init((char *)serial->write_buf, (unsigned int)len);
    int rc = uv_write(&serial->write_req, (uv_stream_t *)&serial->pipe, &uvbuf, 1, egw_serial_on_write_done);
    if (rc != 0) {
        free(serial->write_buf);
        serial->write_buf = NULL;
        serial->writing = false;
        return EGW_ERR_WRITE;
    }

    return EGW_OK;
}

/* ── 公共接口 ────────────────────────────────── */

egw_err_t egw_serial_open(egw_transport_t **tp,
                           const egw_serial_params_t *params,
                           const egw_transport_cbs_t *cbs) {
    if (!tp || !params || !cbs) {
        return EGW_ERR_INVAL;
    }
    if (!params->path) {
        return EGW_ERR_INVAL;
    }

    egw_serial_t *serial = calloc(1, sizeof(egw_serial_t));
    if (!serial) {
        return EGW_ERR_INVAL;
    }

    serial->base.ops = &egw_serial_ops;
    serial->base.id  = 0;
    serial->base.seq = 0;
    serial->base.cbs = *cbs;
    serial->fd       = -1;
    serial->opened   = false;
    serial->writing  = false;

    serial->path_copy = strdup(params->path);
    if (!serial->path_copy) {
        free(serial);
        return EGW_ERR_INVAL;
    }

    serial->params.path      = serial->path_copy;
    serial->params.baud      = params->baud;
    serial->params.parity    = params->parity;
    serial->params.data_bits  = params->data_bits;
    serial->params.stop_bits  = params->stop_bits;

    *tp = &serial->base;

    return serial->base.ops->open(*tp);
}

#ifdef USE_JSON_CONFIG

egw_err_t egw_serial_from_config(egw_transport_t **tp,
                                  egw_conf_t *cfg, const char *path,
                                  const egw_transport_cbs_t *cbs) {
    if (!tp || !cfg || !path || !cbs) {
        return EGW_ERR_INVAL;
    }

    egw_conf_enter(cfg, path);

    egw_serial_params_t params;
    char *path_str, *parity_str;
    egw_conf_get_string(cfg, "/path", &path_str, NULL);
    egw_conf_get_string(cfg, "/parity", &parity_str, "N");
    params.path      = path_str;
    params.parity    = parity_str[0];
    params.baud      = 9600;
    params.data_bits = 8;
    params.stop_bits = 1;
    egw_conf_get_int(cfg, "/baud",      &params.baud, 9600);
    egw_conf_get_int(cfg, "/data_bits", &params.data_bits, 8);
    egw_conf_get_int(cfg, "/stop_bits", &params.stop_bits, 1);

    egw_conf_enter(cfg, "");

    if (!params.path) {
        free(path_str);
        free(parity_str);
        if (cbs->on_open) {
            cbs->on_open(NULL, EGW_ERR_NOTFOUND);
        }
        return EGW_ERR_NOTFOUND;
    }

    egw_err_t err = egw_serial_open(tp, &params, cbs);
    free(path_str);
    free(parity_str);
    return err;
}

#endif