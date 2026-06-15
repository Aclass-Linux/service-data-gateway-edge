/**
 * @file egw_protocol.h
 * @brief 协议帧解析（Modbus RTU）
 *
 * 每端口独立一个 Protocol 上下文，通过 egw_proto_feed() 推入字节，
 * 内部状态机做帧定界 + CRC 校验。同步返回结果。
 */

#ifndef EGW_PROTOCOL_H
#define EGW_PROTOCOL_H

#include "egw_defs.h"

/* ── 不透明上下文 ───────────────────────────────────── */

typedef struct egw_proto_ctx egw_proto_ctx_t;

/* ── 推入结果 ───────────────────────────────────────── */

typedef enum {
    EGW_PROTO_NEED_MORE,     /* 帧不完整，需要更多字节 */
    EGW_PROTO_FRAME_READY,   /* 完整帧已就绪，调用 get_frame 获取 */
    EGW_PROTO_FRAME_ERROR,   /* 帧异常（CRC 错、长度异常等），内部已重置 */
} egw_proto_result_t;

/* ── API ────────────────────────────────────────────── */

/**
 * @brief 创建协议上下文
 * @return 成功返回句柄，失败返回 NULL
 */
egw_proto_ctx_t *egw_proto_ctx_create(void);

/**
 * @brief 销毁协议上下文
 * @param ctx 协议上下文，可为 NULL
 */
void egw_proto_ctx_destroy(egw_proto_ctx_t *ctx);

/**
 * @brief 推入原始字节，帧定界 + CRC 校验
 * @param ctx   协议上下文
 * @param data  原始字节缓冲区
 * @param len   字节数
 * @return EGW_PROTO_FRAME_READY  完整帧已就绪
 *         EGW_PROTO_NEED_MORE    等待更多字节
 *         EGW_PROTO_FRAME_ERROR  帧异常，已自动重置
 */
egw_proto_result_t egw_proto_feed(egw_proto_ctx_t *ctx,
                                   const uint8_t *data, size_t len);

/**
 * @brief 获取已就绪的完整帧指针
 * @param ctx       协议上下文
 * @param frame_len 输出帧长度
 * @return 帧数据指针（指向 ctx 内部缓冲区，下次 feed 前有效）
 *
 * 仅在 egw_proto_feed() 返回 FRAME_READY 后调用有效。
 * 下一次调用 egw_proto_feed() 会覆盖内部缓冲区。
 */
const uint8_t *egw_proto_get_frame(egw_proto_ctx_t *ctx, size_t *frame_len);

/**
 * @brief 重置协议上下文（丢弃当前累积的字节）
 * @param ctx 协议上下文
 */
void egw_proto_reset(egw_proto_ctx_t *ctx);

#endif /* EGW_PROTOCOL_H */
