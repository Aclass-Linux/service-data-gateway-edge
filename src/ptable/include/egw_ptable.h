/**
 * @file egw_ptable.h
 * @brief 点表数据库访问层
 *
 * ptable 层只认识 egw_manifest 一张清单表，
 *
 * 数据结构定义（egw_sb_point_t 等）保留在此供 App 层使用。
 */

#ifndef EGW_PTABLE_H
#define EGW_PTABLE_H

#include "egw_defs.h"

/* ── 元数据表名 ────────────────────────────────────── */

#define EGW_MANIFEST_TABLE  "egw_manifest"

/* ── 表发现 ──────────────────────────────────────────── */

typedef struct {
    char name[32];       /* 数据库中的表名 */
    char protocol[32];   /* 协议（modbus_rtu / modbus_tcp / 空字符串） */
} egw_ptable_tbl_t;

/* ── 南向点表条目 ──────────────────────────────────── */

#define EGW_SB_FLAG_ENABLED          (1u << 0)
#define EGW_SB_FLAG_HAS_SCALE_OFFSET (1u << 1)
#define EGW_SB_FLAG_HAS_DEADBAND     (1u << 2)

typedef struct {
    uint16_t device_id;
    uint32_t sig_id;
    uint8_t  func_code;
    uint16_t reg_addr;
    uint16_t reg_count;
    uint8_t  ctype;
    uint32_t poll_interval_ms;
    uint8_t  flags;
    float    scale;
    float    offset;
    float    deadband;
} egw_sb_point_t;

/* ── 北向点表条目 ──────────────────────────────────── */

#define EGW_NB_FLAG_ENABLED          (1u << 0)
#define EGW_NB_FLAG_HAS_SCALE_OFFSET (1u << 1)
#define EGW_NB_FLAG_HAS_DEADBAND     (1u << 2)

typedef struct {
    uint16_t device_id;
    uint32_t sig_id;
    uint8_t  func_code;
    uint16_t reg_addr;
    uint8_t  ctype;
    uint8_t  flags;
    float    scale;
    float    offset;
    float    deadband;
} egw_nb_point_t;

/* ── 路由表条目 ────────────────────────────────────── */

typedef struct {
    uint16_t device_id;
    uint32_t sig_id;
    uint8_t  ctype;
} egw_route_entry_t;

/* ── 点表句柄 ──────────────────────────────────────── */

typedef struct egw_ptable egw_ptable_t;

/* ── 生命周期 ───────────────────────────────────────── */

egw_ptable_t *egw_ptable_open(const char *db_path);
void          egw_ptable_close(egw_ptable_t *pt);

/* ── 发现的业务表 ────────────────────────────────────── */

uint32_t                egw_ptable_table_count(const egw_ptable_t *pt);
const egw_ptable_tbl_t *egw_ptable_table_get(const egw_ptable_t *pt,
                                               uint32_t index);

/* ── 文件头信息 ──────────────────────────────────────── */

typedef struct {
    uint32_t schema_version;   /* 模式版本号 */
} egw_ptable_head_t;

egw_err_t egw_ptable_head_get(const egw_ptable_t *pt, egw_ptable_head_t *head);

#endif /* EGW_PTABLE_H */
