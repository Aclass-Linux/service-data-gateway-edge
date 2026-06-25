/**
 * @file egw_protocol_common.c
 * @brief 协议帧定界通用逻辑（协议无关）
 *
 * 与 transport 层的 egw_transport_common.c 对齐：
 * 通用 wrapper 通过 vtable 分派到协议特定实现。
 */

#include "egw_protocol_internal.h"
#include <stdlib.h>
#include <string.h>

#define MIN_FRAME 4

/* ── 内部：对 h->buf[0..len) 跑定界 + 校验 ──────────── */

static egw_proto_result_t parse_and_check(egw_proto_handle_t *h)
{
    if (h->len < MIN_FRAME) {
        return EGW_PROTO_NEED_MORE;
    }

    int exp = h->vt->frame_len(h->buf, h->len, h->dir);
    if (exp < 0) {
        if (exp == -1) {
            return EGW_PROTO_NEED_MORE;
        }
        egw_proto_reset(h);
        return EGW_PROTO_FRAME_ERROR;
    }

    if ((size_t)exp > h->cap) {
        egw_proto_reset(h);
        return EGW_PROTO_FRAME_ERROR;
    }

    if (h->len < (size_t)exp) {
        return EGW_PROTO_NEED_MORE;
    }

    if (!h->vt->validate(h->buf, (size_t)exp)) {
        egw_proto_reset(h);
        return EGW_PROTO_FRAME_ERROR;
    }

    /* 帧就绪：截断 len 到帧边界 */
    h->len = (size_t)exp;
    return EGW_PROTO_FRAME_READY;
}

/* ── 通用 API ───────────────────────────────────────── */

uint8_t *egw_proto_reserve(egw_proto_handle_t *h, size_t *avail)
{
    if (!h || !avail) {
        if (avail) {
            *avail = 0;
        }
        return NULL;
    }

    *avail = h->cap - h->len;
    return h->buf + h->len;
}

egw_proto_result_t egw_proto_commit(egw_proto_handle_t *h, size_t n)
{
    if (!h) {
        return EGW_PROTO_NEED_MORE;
    }

    if (n == 0) {
        return EGW_PROTO_NEED_MORE;
    }

    if (h->len + n > h->cap) {
        egw_proto_reset(h);
        return EGW_PROTO_FRAME_ERROR;
    }

    h->len += n;
    return parse_and_check(h);
}

egw_proto_result_t egw_proto_feed(egw_proto_handle_t *h,
                                    const uint8_t *data, size_t len)
{
    if (!h || !data || len == 0) {
        return EGW_PROTO_NEED_MORE;
    }

    size_t avail = 0;
    uint8_t *wp = egw_proto_reserve(h, &avail);
    if (!wp || len > avail) {
        egw_proto_reset(h);
        return EGW_PROTO_FRAME_ERROR;
    }

    memcpy(wp, data, len);
    return egw_proto_commit(h, len);
}

const uint8_t *egw_proto_get_frame(egw_proto_handle_t *h, size_t *frame_len)
{
    if (!h || !frame_len) {
        return NULL;
    }

    *frame_len = h->len;
    return h->buf;
}

void egw_proto_reset(egw_proto_handle_t *h)
{
    if (h) {
        h->len = 0;
    }
}

void egw_proto_close(egw_proto_handle_t *h)
{
    free(h);
}
