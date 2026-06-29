#include "egw_modbus_client.h"
#include "egw_crc.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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

/* ── 请求 slot ───────────────────────────────────────── */

struct egw_modbus_req_slot {
    struct egw_modbus_req_slot *next;
    struct egw_modbus_req_slot *prev;
    uint8_t                    *buf;
    size_t                     len;
    uint8_t                    unit_id;
    uint16_t                   addr;
};

/* ── Client（主站） ──────────────────────────────────── */

struct egw_modbus_client {
    egw_modbus_transport_t    transport;

    egw_modbus_req_slot_t    *slots;       /* 环形链表入口 */
    egw_modbus_req_slot_t    *current;     /* 当前等待响应的 slot */

    uint16_t                  next_tid;    /* 事务 ID 自增（RTU/TCP 均递增） */
    uint32_t                  tx_count;    /* 已发送请求数 */
    uint32_t                  rx_count;    /* 已收到响应数 */

    uint8_t                  *recv_buf;
    size_t                    recv_cap;
    size_t                    recv_len;

    egw_modbus_done_cb        done_cb;
    void                     *cb_arg;
};

egw_modbus_client_t *egw_modbus_client_create(const egw_modbus_client_params_t *params)
{
    if (!params || !params->done_cb) { return NULL; }

    egw_modbus_client_t *client = calloc(1, sizeof(*client));
    if (!client) { return NULL; }

    size_t recv_cap = params->recv_cap ? params->recv_cap : EGW_MODBUS_MAX_FRAME;
    client->recv_buf = malloc(recv_cap);
    if (!client->recv_buf) { free(client); return NULL; }
    client->recv_cap = recv_cap;

    client->transport = params->transport;

    client->done_cb = params->done_cb;
    client->cb_arg  = params->cb_arg;
    return client;
}

void egw_modbus_client_destroy(egw_modbus_client_t *client)
{
    if (!client) { return; }
    if (client->slots) {
        egw_modbus_req_slot_t *start = client->slots;
        egw_modbus_req_slot_t *node  = start;
        do {
            egw_modbus_req_slot_t *next = node->next;
            free(node->buf);
            free(node);
            node = next;
        } while (node != start);
    }
    free(client->recv_buf);
    free(client);
}

/* ── 请求管理 ─────────────────────────────────────────── */

egw_modbus_req_slot_t *egw_modbus_client_register(egw_modbus_client_t *client,
                                                    const egw_modbus_encode_params_t *params)
{
    if (!client || !params) { return NULL; }

    egw_modbus_req_slot_t *slot = malloc(sizeof(*slot));
    if (!slot) { return NULL; }

    slot->buf = egw_modbus_encode(client->transport, params, &slot->len);
    if (!slot->buf || slot->len == 0) { free(slot); return NULL; }

    slot->unit_id = params->unit_id;
    slot->addr    = params->addr;

    if (!client->slots) {
        slot->next = slot->prev = slot;
        client->slots = slot;
    } else {
        slot->next = client->slots;
        slot->prev = client->slots->prev;
        client->slots->prev->next = slot;
        client->slots->prev = slot;
    }
    return slot;
}

void egw_modbus_client_remove(egw_modbus_client_t *client,
                               egw_modbus_req_slot_t *slot)
{
    if (!client || !slot || !client->slots) { return; }

    if (slot->next == slot) {
        client->slots = NULL;
    } else {
        slot->next->prev = slot->prev;
        slot->prev->next = slot->next;
        if (client->slots == slot) {
            client->slots = slot->next;
        }
    }
    if (client->current == slot) {
        client->current = NULL;
    }
    free(slot->buf);
    free(slot);
}

/* ── Hex 日志 ────────────────────────────────────────── */

static void log_hex(const char *tag, const uint8_t *buf, size_t len)
{
    if (len == 0) { return; }
    size_t show = len > 64 ? 64 : len;
    char hex[64 * 3 + 8];
    size_t pos = 0;
    for (size_t i = 0; i < show; i++) {
        pos += snprintf(hex + pos, sizeof(hex) - pos,
                         "%s%02x", i == 0 ? "" : " ", buf[i]);
        if (pos >= sizeof(hex) - 4) { break; }
    }
    if (len > 64) {
        snprintf(hex + pos, sizeof(hex) - pos, " ...");
    }
    EGW_LOGI("  [client] %s (%zu): %s", tag, len, hex);
}

const uint8_t *egw_modbus_client_request(egw_modbus_client_t *client,
                                          egw_modbus_req_slot_t *slot,
                                          size_t *len)
{
    if (!client || !slot || !len) { return NULL; }
    client->current  = slot;
    client->recv_len = 0;

    if (client->transport == EGW_MODBUS_TCP) {
        slot->buf[0] = (uint8_t)(client->next_tid >> 8);
        slot->buf[1] = (uint8_t)(client->next_tid & 0xFF);
    }
    client->next_tid++;
    client->tx_count++;

    *len = slot->len;
    log_hex("send", slot->buf, slot->len);
    return slot->buf;
}

/* ── 接收与解析 ──────────────────────────────────────── */

static void client_handle_frame(egw_modbus_client_t *client)
{
    egw_modbus_req_slot_t *slot = client->current;
    if (!slot) { return; }

    client->rx_count++;

    uint8_t pdu[EGW_MODBUS_MAX_PDU];
    size_t pdu_len = 0;
    uint8_t unit_id = 0;

    if (egw_modbus_decode(client->transport,
                            client->recv_buf, client->recv_len,
                            &unit_id, pdu, &pdu_len) != EGW_OK) {
        egw_modbus_result_t r = { .unit_id = slot->unit_id, .addr = slot->addr,
                                   .regs = NULL, .reg_count = -1 };
        client->done_cb(&r, client->cb_arg);
        return;
    }

    log_hex("recv", client->recv_buf, client->recv_len);

    if (unit_id != slot->unit_id) {
        egw_modbus_result_t r = { .unit_id = slot->unit_id, .addr = slot->addr,
                                   .regs = NULL, .reg_count = -1 };
        client->done_cb(&r, client->cb_arg);
        return;
    }

    uint16_t regs[128];
    uint8_t funccode_off = (client->transport == EGW_MODBUS_TCP) ? 7 : 1;
    uint8_t funccode = slot->buf[funccode_off];
    int reg_count = egw_modbus_parse_read_pdu(pdu, pdu_len, funccode,
                                               regs, 128);
    if (reg_count < 0) {
        egw_modbus_result_t r = { .unit_id = slot->unit_id, .addr = slot->addr,
                                   .regs = NULL, .reg_count = -1 };
        client->done_cb(&r, client->cb_arg);
        return;
    }

    egw_modbus_result_t r = { .unit_id = slot->unit_id, .addr = slot->addr,
                               .regs = regs, .reg_count = reg_count };
    client->done_cb(&r, client->cb_arg);
}

void egw_modbus_client_feed(egw_modbus_client_t *client,
                              const uint8_t *data, size_t len)
{
    if (!client || !data || len == 0 || !client->current) { return; }

    size_t avail = client->recv_cap - client->recv_len;
    if (len > avail) { client->recv_len = 0; return; }
    memcpy(client->recv_buf + client->recv_len, data, len);
    client->recv_len += len;

    size_t frame_len = 0;
    egw_proto_result_t result = resp_parse(client->recv_buf, client->recv_len,
                                            &frame_len);
    switch (result) {
    case EGW_PROTO_FRAME_READY:
        client->recv_len = frame_len;
        client_handle_frame(client);
        break;
    case EGW_PROTO_FRAME_ERROR:
        client->recv_len = 0;
        break;
    case EGW_PROTO_NEED_MORE:
        break;
    }
}

size_t egw_modbus_client_reserve(egw_modbus_client_t *client, OUT uint8_t **buf)
{
    if (!client || !client->current || !buf) { return 0; }
    *buf = client->recv_buf + client->recv_len;
    return client->recv_cap - client->recv_len;
}

void egw_modbus_client_commit(egw_modbus_client_t *client, size_t nbytes)
{
    if (!client || nbytes == 0 || !client->current) { return; }
    if (client->recv_len + nbytes > client->recv_cap) { client->recv_len = 0; return; }
    client->recv_len += nbytes;

    size_t frame_len = 0;
    egw_proto_result_t result = resp_parse(client->recv_buf, client->recv_len,
                                            &frame_len);
    if (result == EGW_PROTO_FRAME_READY) {
        client->recv_len = frame_len;
        client_handle_frame(client);
    } else if (result == EGW_PROTO_FRAME_ERROR) {
        client->recv_len = 0;
    }
}
