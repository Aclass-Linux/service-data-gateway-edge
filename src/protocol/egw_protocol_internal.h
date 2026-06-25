/**
 * @file egw_protocol_internal.h
 * @brief Protocol 层私有定义（不对外暴露）
 *
 * 与 transport 层的 egw_transport_internal.h 对齐：
 * - handle struct 定义（含 vtable 函数指针）
 * - 各协议实现文件 include 此头，设置 vtable
 */

#ifndef EGW_PROTOCOL_INTERNAL_H
#define EGW_PROTOCOL_INTERNAL_H

#include "egw_protocol.h"
#include <stdbool.h>

/* ── vtable：协议特定的帧定界 + 校验 ────────────────── */

/**
 * @brief 协议特定函数表
 * - frame_len：根据已收字节推定期望帧长度
 * - validate：校验完整帧（CRC / checksum）
 */
typedef struct {
    int   (*frame_len)(const uint8_t *buf, size_t len, egw_proto_dir_t dir);
    bool  (*validate)(const uint8_t *buf, size_t frame_len);
} egw_proto_vtable_t;

/* ── handle 定义 ────────────────────────────────────── */

struct egw_proto_handle {
    egw_proto_dir_t            dir;   /* 解析方向 */
    size_t                     cap;   /* 缓冲区容量 */
    size_t                     len;   /* 已累积字节数 */
    const egw_proto_vtable_t  *vt;    /* 协议特定函数表 */
    uint8_t                    buf[]; /* flexible array member — 单次分配 */
};

#endif /* EGW_PROTOCOL_INTERNAL_H */
