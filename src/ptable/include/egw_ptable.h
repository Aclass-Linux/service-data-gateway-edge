/**
 * @file egw_ptable.h
 * @brief 点表数据库访问层
 *
 * App 通过 egw_ptable_register 注册字段映射，
 * ptable 自动加载、校验类型、填行结构体回调。
 */

#ifndef EGW_PTABLE_H
#define EGW_PTABLE_H

#include "egw_defs.h"
#include <stdint.h>

/* ── 表发现 ──────────────────────────────────────────── */

#define EGW_PTABLE_DESC_MAX     (128)

/** @brief manifest 中一条记录（表名 + 协议） */
typedef struct {
    char name[32];
    char protocol[32];
} egw_ptable_tbl_t;

/* ── head 树（独立生命周期，与 DB 无关） ────────── */

/** @brief thread 子节点类型 */
typedef enum {
    EGW_THREAD_NODE_PROTOCOL = 1,
    EGW_THREAD_NODE_PORT,
    EGW_THREAD_NODE_SQLITE,
} egw_node_type_t;

/** @brief thread 的一个子节点（链表） */
typedef struct egw_node {
    struct egw_node         *next;
    egw_node_type_t         type;
    char                    desc[EGW_PTABLE_DESC_MAX];
} egw_node_t;

/** @brief 一个线程（链表），包含若干子节点 */
typedef struct egw_thread {
    struct egw_thread       *next;
    egw_node_t              *nodes;
    int                     thread_id;
    char                    desc[EGW_PTABLE_DESC_MAX];
} egw_thread_t;

/** @brief head 树根节点（独立于 DB，egw_head_load 返回） */
typedef struct {
    int                 version;
    char                desc[EGW_PTABLE_DESC_MAX];
    egw_thread_t        *threads;
} egw_head_t;

/** @brief 从 .db 文件加载 head 树（内部分配，用完 head_free） */
egw_head_t *egw_ptable_head_load(const char *db_path);

/** @brief 释放 head 树 */
void        egw_ptable_head_free(egw_head_t *head);

/* ── 点表句柄（需要 DB 的操作） ──────────────────── */

typedef struct egw_ptable egw_ptable_t;

/** @brief 打开数据库，校验 version 与 head 一致 */
egw_ptable_t *egw_ptable_open(const char *db_path, int head_version);

/** @brief 关闭数据库 */
void          egw_ptable_close(egw_ptable_t *pt);

/* ── 发现业务表（从 egw_manifest 扫描） ────────────────── */

typedef struct egw_manifest egw_manifest_t;

/** @brief 扫描 manifest 表，返回匹配的业务表列表（内部分配） */
egw_manifest_t *egw_ptable_discover(egw_ptable_t *pt, const char *manifest);

/** @brief 有效业务表数量 */
uint32_t        egw_manifest_count(const egw_manifest_t *mh);

/** @brief 按索引取业务表信息 */
const egw_ptable_tbl_t *egw_manifest_get(const egw_manifest_t *mh,
                                           uint32_t index);

/** @brief 释放 manifest 句柄 */
void            egw_manifest_free(egw_manifest_t *mh);

/* ── 行数据注册 ──────────────────────────────────────── */

/** @brief 行比较函数签名（qsort / bsearch 兼容）
 *
 * 比较 row_a（数据集内一行）与 key_b（调用方构造的查找键），
 * 返回 <0 / 0 / >0。
 */
typedef int (*egw_ptable_cmp_fn)(const void *row_a, const void *key_b);

typedef struct {
    const egw_field_t *fields;
    size_t             nfields;
    size_t             row_size;
    const char        *order_by;   /* "device_id, reg_addr" → SQL ORDER BY */
    egw_ptable_cmp_fn  lkp;        /* key vs row，bsearch 用 */
} egw_schema_t;

/** @brief 行集句柄（不透明）
 *
 * 通过 egw_ptable_register 加载业务表全部行后返回。
 * 若 schema.cmp 非空则在加载后自动排序。
 * 使用 _count / _row / _lookup 访问，用完 _free 释放。
 */
typedef struct egw_ptable_rs egw_ptable_rs_t;

/** @brief 加载业务表全部行数据
 *
 * @param pt      点表句柄
 * @param table   表名
 * @param schema  字段描述
 * @return        行集句柄，NULL 表示失败
 */
egw_ptable_rs_t *egw_ptable_register(egw_ptable_t *pt,
                                      const char *table,
                                      const egw_schema_t *schema);

/** @brief 行数 */
size_t egw_ptable_rs_count(const egw_ptable_rs_t *rs);

/** @brief 取第 idx 行指针（调用方 cast 到对应结构体，可写） */
void *egw_ptable_rs_row(const egw_ptable_rs_t *rs, size_t idx);

/** @brief 按业务键二分查找
 *
 * @param rs   行集
 * @param key  调用方构造的查找键结构体
 * @return     匹配行指针（可写），NULL 表示未找到
 */
void *egw_ptable_rs_lookup(const egw_ptable_rs_t *rs, const void *key);

/** @brief 释放行集 */
void egw_ptable_rs_free(egw_ptable_rs_t *rs);

#endif /* EGW_PTABLE_H */
