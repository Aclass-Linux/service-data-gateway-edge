/**
 * @file egw_transport.c
 * @brief Transport 通用接口 —— vtable 分发
 */

#include "egw_transport.h"

egw_err_t egw_transport_close(egw_transport_t *tp) {
    if (!tp) {
        return EGW_ERR_HANDLER;
    }

    if (!tp->ops || !tp->ops->close) {
        return EGW_ERR_HANDLER;
    }

    return tp->ops->close(tp);
}

egw_err_t egw_transport_write(egw_transport_t *tp, const void *buf, size_t len) {
    if (!tp || !buf || len == 0) {
        return EGW_ERR_HANDLER;
    }

    if (!tp->ops || !tp->ops->write) {
        return EGW_ERR_HANDLER;
    }

    return tp->ops->write(tp, buf, len);
}