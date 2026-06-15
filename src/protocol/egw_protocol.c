#include "egw_protocol.h"
#include <stdlib.h>
#include <string.h>

#define BUF_SIZE   256
#define MIN_FRAME  4

struct egw_proto_ctx {
    uint8_t  buf[BUF_SIZE];
    size_t   len;
};

/* ── CRC-16 (Modbus) ────────────────────────────────── */

static uint16_t crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1u) {
                crc = (crc >> 1) ^ 0xA001u;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

/* ── 根据功能码推定期望帧长度 ────────────────────────── */

static int expect_len(const uint8_t *buf, size_t len)
{
    if (len < 2) {
        return -1;
    }

    uint8_t func = buf[1];

    /* 异常响应：[addr, func|0x80, exc, crc_lo, crc_hi] = 5 */
    if (func & 0x80u) {
        return 5;
    }

    /* 读线圈/离散/保持/输入寄存器：需要 byte_count 字段在 buf[2] */
    if ((func == 0x01) || (func == 0x02) || (func == 0x03) || (func == 0x04)) {
        if (len < 3) {
            return -1;
        }
        return 3 + (int)buf[2] + 2;
    }

    /* 写单线圈/单寄存器：响应固定 8 字节 */
    if ((func == 0x05) || (func == 0x06)) {
        return 8;
    }

    /* 写多线圈/多寄存器：响应固定 8 字节 */
    if ((func == 0x0F) || (func == 0x10)) {
        return 8;
    }

    /* 未知功能码 */
    return -2;
}

/* ── 生命周期 ───────────────────────────────────────── */

egw_proto_ctx_t *egw_proto_ctx_create(void)
{
    egw_proto_ctx_t *ctx = calloc(1, sizeof(*ctx));
    return ctx;
}

void egw_proto_ctx_destroy(egw_proto_ctx_t *ctx)
{
    free(ctx);
}

/* ── 推入字节 ───────────────────────────────────────── */

egw_proto_result_t egw_proto_feed(egw_proto_ctx_t *ctx,
                                   const uint8_t *data, size_t len)
{
    if (!ctx || !data || len == 0) {
        return EGW_PROTO_NEED_MORE;
    }

    /* 防止溢出：截断到缓冲区剩余空间 */
    if (ctx->len + len > BUF_SIZE) {
        egw_proto_reset(ctx);
        return EGW_PROTO_FRAME_ERROR;
    }

    memcpy(ctx->buf + ctx->len, data, len);
    ctx->len += len;

    /* 至少需要最小帧长度才能判断完整性 */
    if (ctx->len < MIN_FRAME) {
        return EGW_PROTO_NEED_MORE;
    }

    /* 判定期望长度 */
    int exp = expect_len(ctx->buf, ctx->len);
    if (exp < 0) {
        if (exp == -1) {
            return EGW_PROTO_NEED_MORE;
        }
        egw_proto_reset(ctx);
        return EGW_PROTO_FRAME_ERROR;
    }

    if ((size_t)exp > BUF_SIZE) {
        egw_proto_reset(ctx);
        return EGW_PROTO_FRAME_ERROR;
    }

    /* 尚未收齐 */
    if (ctx->len < (size_t)exp) {
        return EGW_PROTO_NEED_MORE;
    }

    /* CRC 校验 */
    uint16_t calc = crc16(ctx->buf, (size_t)exp - 2u);
    uint16_t recv = (uint16_t)ctx->buf[exp - 2]
                   | ((uint16_t)ctx->buf[exp - 1] << 8);
    if (calc != recv) {
        egw_proto_reset(ctx);
        return EGW_PROTO_FRAME_ERROR;
    }

    return EGW_PROTO_FRAME_READY;
}

/* ── 获取帧 ─────────────────────────────────────────── */

const uint8_t *egw_proto_get_frame(egw_proto_ctx_t *ctx, size_t *frame_len)
{
    if (!ctx || !frame_len) {
        return NULL;
    }

    *frame_len = ctx->len;
    return ctx->buf;
}

/* ── 重置 ───────────────────────────────────────────── */

void egw_proto_reset(egw_proto_ctx_t *ctx)
{
    if (ctx) {
        ctx->len = 0;
    }
}
