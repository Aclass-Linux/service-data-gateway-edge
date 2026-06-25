/**
 * @file egw_protocol_modbus.c
 * @brief Modbus 协议特定的帧定界 + CRC 校验
 *
 * 与 transport 层的 egw_transport_serial.c 对齐：
 * 协议特定实现 + open 函数（分配 handle + 设置 vtable）。
 */

#include "egw_protocol_internal.h"
#include "egw_crc.h"
#include <stdlib.h>

/* ── Modbus 帧长度推定 ──────────────────────────────── */

static int modbus_frame_len(const uint8_t *buf, size_t len, egw_proto_dir_t dir)
{
    if (len < 2) {
        return -1;
    }

    uint8_t func = buf[1];

    /* 异常响应：[addr, func|0x80, exc, crc_lo, crc_hi] = 5 */
    if (func & 0x80u) {
        return 5;
    }

    if (dir == EGW_PROTO_DIR_REQUEST) {
        /* ── 请求帧长度（从站侧） ── */
        switch (func) {
        case 0x01: case 0x02: case 0x03: case 0x04:
            /* 读请求：固定 8 字节 */
            return 8;
        case 0x05: case 0x06:
            /* 写单值请求：固定 8 字节 */
            return 8;
        case 0x0F: case 0x10:
            /* 写多值请求：[unit, fc, addr(2), qty(2), byte_count(1), data..., crc(2)] */
            if (len < 7) {
                return -1;
            }
            return 9 + (int)buf[6];
        default:
            return -2;
        }
    }

    /* ── 响应帧长度（主站侧） ── */
    if ((func == 0x01) || (func == 0x02) || (func == 0x03) || (func == 0x04)) {
        if (len < 3) {
            return -1;
        }
        return 3 + (int)buf[2] + 2;
    }

    if ((func == 0x05) || (func == 0x06)) {
        return 8;
    }

    if ((func == 0x0F) || (func == 0x10)) {
        return 8;
    }

    return -2;
}

/* ── Modbus CRC-16 校验 ─────────────────────────────── */

static bool modbus_validate(const uint8_t *buf, size_t frame_len)
{
    uint16_t calc = egw_crc_modbus_table(buf, frame_len - 2u);
    uint16_t recv = (uint16_t)buf[frame_len - 2]
                  | ((uint16_t)buf[frame_len - 1] << 8);
    return calc == recv;
}

/* ── vtable ─────────────────────────────────────────── */

static const egw_proto_vtable_t s_modbus_vtable = {
    .frame_len = modbus_frame_len,
    .validate  = modbus_validate,
};

/* ── open ───────────────────────────────────────────── */

egw_proto_handle_t *egw_proto_modbus_open(const egw_proto_modbus_params_t *params)
{
    if (!params) {
        return NULL;
    }

    size_t buf_size = params->buf_size;
    if (buf_size == 0) {
        buf_size = EGW_PROTO_DEFAULT_BUF_SIZE;
    }

    egw_proto_handle_t *h = calloc(1, sizeof(*h) + buf_size);
    if (h) {
        h->dir = params->dir;
        h->cap = buf_size;
        h->vt  = &s_modbus_vtable;
    }
    return h;
}
