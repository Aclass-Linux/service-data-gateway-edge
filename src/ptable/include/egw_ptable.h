/**
 * @file egw_ptable.h
 * @brief 点表数据结构和查询 API
 *
 * 从 SQLite 数据库加载三张表（southbound / northbound / route），
 * 构建内存有序数组，通过二分查找提供 O(log n) 查询。
 */

#ifndef EGW_PTABLE_H
#define EGW_PTABLE_H

#include "egw_defs.h"

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

/** @brief 打开 SQLite 数据库，加载三表到内存
 *  @param db_path 数据库文件路径
 *  @return 点表句柄，失败返回 NULL */
egw_ptable_t *egw_ptable_open(const char *db_path);

/** @brief 关闭点表，释放所有内存 */
void egw_ptable_close(egw_ptable_t *pt);

/* ── 查询 ──────────────────────────────────────────── */

/** @brief 按 (device_id, sig_id) 查找南向条目，O(log n) */
const egw_sb_point_t *egw_ptable_sb_lookup(const egw_ptable_t *pt,
                                            uint16_t device_id,
                                            uint32_t sig_id);

/** @brief 按 (device_id, sig_id) 查找北向条目，O(log n) */
const egw_nb_point_t *egw_ptable_nb_lookup(const egw_ptable_t *pt,
                                            uint16_t device_id,
                                            uint32_t sig_id);

/** @brief 按 (device_id, sig_id) 查找路由条目，O(log n) */
const egw_route_entry_t *egw_ptable_route_lookup(const egw_ptable_t *pt,
                                                  uint16_t device_id,
                                                  uint32_t sig_id);

/* ── 遍历 ──────────────────────────────────────────── */

uint32_t egw_ptable_sb_count(const egw_ptable_t *pt);
const egw_sb_point_t *egw_ptable_sb_at(const egw_ptable_t *pt, uint32_t index);

uint32_t egw_ptable_nb_count(const egw_ptable_t *pt);
const egw_nb_point_t *egw_ptable_nb_at(const egw_ptable_t *pt, uint32_t index);

#endif /* EGW_PTABLE_H */
