#include "egw_modbus_client.h"
#include "egw_crc.h"
#include <string.h>
#include <stdlib.h>

#define MIN_FRAME 4

/* ── 响应帧定界（内联） ─────────────────────────────── */

static int resp_frame_len(const uint8_t *buf, size_t len)
{
    if (len < 2) { return -1; }
    uint8_t func = buf[1];
    if (func & 0x80u) { return 5; }

    if ((func == 0x01) || (func == 0x02) || (func == 0x03) || (func == 0x04)) {
        if (len < 3) { return -1; }
        return 3 + (int)buf[2] + 2;
    }
    if ((func == 0x05) || (func == 0x06) || (func == 0x0F) || (func == 0x10)) {
        return 8;
    }
    return -2;
}

static bool resp_validate(const uint8_t *buf, size_t frame_len)
{
    uint16_t calc = egw_crc_modbus_table(buf, frame_len - 2u);
    uint16_t recv = (uint16_t)buf[frame_len - 2]
                  | ((uint16_t)buf[frame_len - 1] << 8);
    return calc == recv;
}

static egw_proto_result_t resp_parse(const uint8_t *buf, size_t len,
                                      size_t *frame_len)
{
    if (len < MIN_FRAME) { return EGW_PROTO_NEED_MORE; }
    int exp = resp_frame_len(buf, len);
    if (exp == -1) { return EGW_PROTO_NEED_MORE; }
    if (exp < 0) { return EGW_PROTO_FRAME_ERROR; }
    if (len < (size_t)exp) { return EGW_PROTO_NEED_MORE; }
    if (!resp_validate(buf, (size_t)exp)) { return EGW_PROTO_FRAME_ERROR; }
    *frame_len = (size_t)exp;
    return EGW_PROTO_FRAME_READY;
}

/* ── Client（主站） ──────────────────────────────────── */

struct egw_modbus_client {
    egw_modbus_ser_wrap_fn    wrap;
    egw_modbus_ser_unwrap_fn  unwrap;

    egw_modbus_req_slot_t    *slots;       /* 请求链表 */
    egw_modbus_req_slot_t    *current;     /* 当前正等待响应的 slot */

    uint8_t                  *recv_buf;    /* 接收缓冲区 */
    size_t                    recv_cap;
    size_t                    recv_len;

    egw_modbus_done_cb        done_cb;
    void                     *cb_arg;
};

egw_modbus_client_t *egw_modbus_client_create(egw_modbus_transport_t transport,
                                                egw_modbus_done_cb done_cb,
                                                void *cb_arg)
{
    egw_modbus_client_t *c = calloc(1, sizeof(*c));
    if (!c) { return NULL; }

    c->recv_buf = malloc(EGW_MODBUS_MAX_FRAME);
    if (!c->recv_buf) { free(c); return NULL; }
    c->recv_cap = EGW_MODBUS_MAX_FRAME;

    switch (transport) {
    case EGW_MODBUS_RTU:
        c->wrap   = egw_modbus_ser_wrap_rtu;
        c->unwrap = egw_modbus_ser_unwrap_rtu;
        break;
    case EGW_MODBUS_TCP:
        c->wrap   = egw_modbus_ser_wrap_tcp;
        c->unwrap = egw_modbus_ser_unwrap_tcp;
        break;
    }

    c->done_cb = done_cb;
    c->cb_arg  = cb_arg;
    return c;
}

void egw_modbus_client_destroy(egw_modbus_client_t *c)
{
    if (!c) { return; }
    egw_modbus_req_slot_t *n = c->slots;
    while (n) {
        egw_modbus_req_slot_t *next = n->next;
        free(n->buf);
        free(n);
        n = next;
    }
    free(c->recv_buf);
    free(c);
}

egw_modbus_req_slot_t *egw_modbus_client_register(egw_modbus_client_t *c,
                                                    uint8_t unit_id,
                                                    uint8_t funccode,
                                                    uint16_t addr,
                                                    uint16_t count)
{
    if (!c) { return NULL; }

    egw_modbus_req_slot_t *s = calloc(1, sizeof(*s));
    if (!s) { return NULL; }

    s->buf = malloc(EGW_MODBUS_MAX_FRAME);
    if (!s->buf) { free(s); return NULL; }

    uint8_t pdu[EGW_MODBUS_MAX_PDU];
    size_t pdu_len = egw_modbus_build_read_pdu(pdu, funccode, addr, count);
    if (pdu_len == 0) { free(s->buf); free(s); return NULL; }

    s->len = c->wrap(s->buf, unit_id, pdu, pdu_len);
    if (s->len == 0) { free(s->buf); free(s); return NULL; }

    s->unit_id = unit_id;
    s->addr    = addr;
    s->next    = c->slots;
    c->slots   = s;
    return s;
}

const uint8_t *egw_modbus_client_send(egw_modbus_client_t *c,
                                       egw_modbus_req_slot_t *slot,
                                       size_t *len)
{
    if (!c || !slot || !len) { return NULL; }
    c->current  = slot;
    c->recv_len = 0;
    *len = slot->len;
    return slot->buf;
}

/* ── 解析已就绪的响应帧，调回调 ──────────────────────── */

static void client_handle_frame(egw_modbus_client_t *c)
{
    egw_modbus_req_slot_t *s = c->current;
    if (!s) { return; }

    uint8_t pdu[EGW_MODBUS_MAX_PDU];
    size_t pdu_len = 0;
    uint8_t unit_id = 0;

    if (c->unwrap(c->recv_buf, c->recv_len,
                   &unit_id, pdu, &pdu_len) != EGW_OK) {
        c->done_cb(s->unit_id, s->addr, NULL, -1, c->cb_arg);
        return;
    }

    if (unit_id != s->unit_id) {
        c->done_cb(s->unit_id, s->addr, NULL, -1, c->cb_arg);
        return;
    }

    uint16_t regs[128];
    uint8_t funccode = s->buf[1];
    int n = egw_modbus_parse_read_pdu(pdu, pdu_len, funccode, regs, 128);
    if (n < 0) {
        c->done_cb(s->unit_id, s->addr, NULL, -1, c->cb_arg);
        return;
    }

    c->done_cb(s->unit_id, s->addr, regs, n, c->cb_arg);
}

void egw_modbus_client_feed(egw_modbus_client_t *c,
                              const uint8_t *data, size_t len)
{
    if (!c || !data || len == 0 || !c->current) { return; }

    size_t avail = c->recv_cap - c->recv_len;
    if (len > avail) { c->recv_len = 0; return; }
    memcpy(c->recv_buf + c->recv_len, data, len);
    c->recv_len += len;

    size_t frame_len = 0;
    egw_proto_result_t r = resp_parse(c->recv_buf, c->recv_len, &frame_len);
    if (r == EGW_PROTO_FRAME_READY) {
        c->recv_len = frame_len;
        client_handle_frame(c);
    } else if (r == EGW_PROTO_FRAME_ERROR) {
        c->recv_len = 0;
    }
}

uint8_t *egw_modbus_client_reserve(egw_modbus_client_t *c, size_t *avail)
{
    if (!c || !c->current || !avail) {
        if (avail) { *avail = 0; }
        return NULL;
    }
    *avail = c->recv_cap - c->recv_len;
    return c->recv_buf + c->recv_len;
}

void egw_modbus_client_commit(egw_modbus_client_t *c, size_t n)
{
    if (!c || n == 0 || !c->current) { return; }
    if (c->recv_len + n > c->recv_cap) { c->recv_len = 0; return; }
    c->recv_len += n;

    size_t frame_len = 0;
    egw_proto_result_t r = resp_parse(c->recv_buf, c->recv_len, &frame_len);
    if (r == EGW_PROTO_FRAME_READY) {
        c->recv_len = frame_len;
        client_handle_frame(c);
    } else if (r == EGW_PROTO_FRAME_ERROR) {
        c->recv_len = 0;
    }
}
