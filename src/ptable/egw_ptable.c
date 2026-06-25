#include "egw_ptable.h"
#include "sqlite3.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct egw_ptable {
    sqlite3 *db;
};

struct egw_manifest {
    egw_ptable_tbl_t *tables;
    uint32_t          count;
    uint32_t          cap;
};

/* ── SQL 查询常量 ───────────────────────────────────── */

#define SQL_HEAD_ROOT           "SELECT desc FROM egw_head WHERE id=1 AND type='HEAD'"
#define SQL_HEAD_VERSION        "SELECT desc FROM egw_head WHERE id=2 AND type='version'"
#define SQL_THREAD_LIST         "SELECT id, desc FROM egw_head" \
                                " WHERE parent_id=1 AND type='thread' ORDER BY id"
#define SQL_NODE_LIST           "SELECT type, desc FROM egw_head" \
                                " WHERE parent_id=?1 ORDER BY id"

#define SQL_MANIFEST_VALID      "SELECT m.key, m.value FROM %s m" \
                                " WHERE EXISTS (" \
                                "  SELECT 1 FROM sqlite_master" \
                                "  WHERE type='table' AND name = m.key" \
                                ")"
#define SQL_MANIFEST_WARN       "SELECT key FROM %s WHERE key NOT IN (" \
                                "  SELECT name FROM sqlite_master WHERE type='table'" \
                                ")"
#define SQL_MANIFEST_DEBUG      "SELECT name FROM sqlite_master WHERE type='table'" \
                                " AND name NOT IN ('egw_head', '%s')" \
                                " AND name NOT IN (SELECT key FROM %s)"

#define SQL_COUNT_ROWS          "SELECT COUNT(*) FROM %s"
#define SQL_SELECT_ALL          "SELECT * FROM %s"

/* ── 节点类型映射 ────────────────────────────────────── */

static const struct {
    const char     *name;
    egw_node_type_t type;
} node_type_map[] = {
    {"protocol", EGW_THREAD_NODE_PROTOCOL},
    {"port",     EGW_THREAD_NODE_PORT},
    {"sqlite",   EGW_THREAD_NODE_SQLITE},
};

/* ── 发现业务表的 exec 回调 ─────────────────────────── */

static int valid_cb(void *arg, int ncol, char **vals, char **names)
{
    (void)names;
    egw_manifest_t *mh = (egw_manifest_t *)arg;

    if (ncol < 2 || !vals[0]) { return 0; }

    if (mh->count >= mh->cap) {
        uint32_t new_cap = mh->cap ? mh->cap * 2 : 4;
        egw_ptable_tbl_t *p = realloc(mh->tables,
                                      new_cap * sizeof(egw_ptable_tbl_t));
        if (!p) { return 0; }
        mh->tables = p;
        mh->cap    = new_cap;
    }

    uint32_t i = mh->count;
    strncpy(mh->tables[i].name, vals[0], sizeof(mh->tables[i].name) - 1);
    mh->tables[i].name[sizeof(mh->tables[i].name) - 1] = '\0';
    if (vals[1]) {
        strncpy(mh->tables[i].protocol, vals[1],
                sizeof(mh->tables[i].protocol) - 1);
        mh->tables[i].protocol[sizeof(mh->tables[i].protocol) - 1] = '\0';
    }
    mh->count++;
    return 0;
}

static int warn_cb(void *arg, int ncol, char **vals, char **names)
{
    (void)arg;
    (void)ncol;
    (void)names;
    if (!vals[0]) { return 0; }
    EGW_LOGW("manifest registered '%s' but table not found", vals[0]);
    return 0;
}

static int debug_cb(void *arg, int ncol, char **vals, char **names)
{
    (void)arg;
    (void)ncol;
    (void)names;
    if (!vals[0]) { return 0; }
    EGW_LOGD("table '%s' exists in db but not registered in manifest", vals[0]);
    return 0;
}

/* ── 点表生命周期 ──────────────────────────────────── */

egw_ptable_t *egw_ptable_open(const char *db_path, int head_version)
{
    if (!db_path) { return NULL; }

    egw_ptable_t *pt = calloc(1, sizeof(*pt));
    if (!pt) { return NULL; }

    int rc = sqlite3_open_v2(db_path, &pt->db,
                              SQLITE_OPEN_READWRITE, NULL);
    if (rc != SQLITE_OK) {
        free(pt);
        return NULL;
    }

    sqlite3_busy_timeout(pt->db, 5000);

    /* 校验版本是否匹配 */
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(pt->db, SQL_HEAD_VERSION, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(pt->db);
        free(pt);
        return NULL;
    }

    int db_ver = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        db_ver = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (db_ver != head_version) {
        sqlite3_close(pt->db);
        free(pt);
        return NULL;
    }

    return pt;
}

void egw_ptable_close(egw_ptable_t *pt)
{
    if (!pt) { return; }

    sqlite3_close(pt->db);
    free(pt);
}

/* ── 节点类型查询 ────────────────────────────────────── */

static egw_node_type_t name_to_type(const char *name)
{
    for (size_t i = 0; i < sizeof(node_type_map)/sizeof(node_type_map[0]); i++) {
        if (strcmp(node_type_map[i].name, name) == 0) {
            return node_type_map[i].type;
        }
    }
    EGW_LOGW("unknown head node type: %s", name);
    return (egw_node_type_t)0;
}

/* ── 节点链表助手 ────────────────────────────────────── */

static void free_node_list(egw_node_t *head)
{
    while (head) {
        egw_node_t *next = head->next;
        free(head);
        head = next;
    }
}

static egw_node_t *parse_node_row(sqlite3_stmt *s2)
{
    const char *type = (const char *)sqlite3_column_text(s2, 0);
    const char *desc = (const char *)sqlite3_column_text(s2, 1);
    if (!type) { return NULL; }

    egw_node_type_t nt = name_to_type(type);
    if (!nt) { return NULL; }

    egw_node_t *n = calloc(1, sizeof(*n));
    if (!n) { return NULL; }

    n->type = nt;
    if (desc) { snprintf(n->desc, sizeof(n->desc), "%s", desc); }
    return n;
}

static egw_node_t *load_thread_nodes(sqlite3 *db, int thread_id)
{
    sqlite3_stmt *s2 = NULL;
    if (sqlite3_prepare_v2(db, SQL_NODE_LIST, -1, &s2, NULL) != SQLITE_OK) {
        return NULL;
    }
    sqlite3_bind_int(s2, 1, thread_id);

    egw_node_t *head = NULL;
    egw_node_t **tail = &head;

    while (sqlite3_step(s2) == SQLITE_ROW) {
        egw_node_t *n = parse_node_row(s2);
        if (!n) {
            free_node_list(head);
            sqlite3_finalize(s2);
            return NULL;
        }
        *tail = n;
        tail = &n->next;
    }
    sqlite3_finalize(s2);
    return head;
}

/* ── head 树生命周期 ────────────────────────────────── */

egw_head_t *egw_ptable_head_load(const char *db_path)
{
    if (!db_path) { return NULL; }

    sqlite3 *db = NULL;
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        return NULL;
    }
    sqlite3_busy_timeout(db, 5000);

    /* 验证根节点 */
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, SQL_HEAD_ROOT, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        return NULL;
    }

    egw_head_t *head = calloc(1, sizeof(*head));
    if (!head) { goto fail; }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *d = (const char *)sqlite3_column_text(stmt, 0);
        if (d) { snprintf(head->desc, sizeof(head->desc), "%s", d); }
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    /* 读取版本 */
    rc = sqlite3_prepare_v2(db, SQL_HEAD_VERSION, -1, &stmt, NULL);
    if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
        head->version = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    /* 解析线程链表 */
    rc = sqlite3_prepare_v2(db, SQL_THREAD_LIST, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        egw_thread_t **t_tail = &head->threads;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int         tid = sqlite3_column_int(stmt, 0);
            const char *td  = (const char *)sqlite3_column_text(stmt, 1);

            egw_thread_t *th = calloc(1, sizeof(*th));
            if (!th) { sqlite3_finalize(stmt); goto fail; }
            th->thread_id = tid;
            if (td) { snprintf(th->desc, sizeof(th->desc), "%s", td); }

            th->nodes = load_thread_nodes(db, tid);
            if (!th->nodes) {
                free(th);
                sqlite3_finalize(stmt);
                goto fail;
            }

            *t_tail = th;
            t_tail  = &th->next;
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return head;

fail:
    sqlite3_close(db);
    egw_ptable_head_free(head);
    return NULL;
}

void egw_ptable_head_free(egw_head_t *head)
{
    if (!head) { return; }

    egw_thread_t *t = head->threads;
    while (t) {
        free_node_list(t->nodes);
        egw_thread_t *next = t->next;
        free(t);
        t = next;
    }

    free(head);
}

/* ── 发现业务表 ─────────────────────────────────────── */

egw_manifest_t *egw_ptable_discover(egw_ptable_t *pt, const char *manifest)
{
    if (!pt || !manifest) { return NULL; }

    egw_manifest_t *mh = calloc(1, sizeof(*mh));
    if (!mh) { return NULL; }

    char sql[384];
    snprintf(sql, sizeof(sql), SQL_MANIFEST_VALID, manifest);

    char *errmsg = NULL;
    int rc = sqlite3_exec(pt->db, sql, valid_cb, mh, &errmsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errmsg);
        free(mh->tables);
        free(mh);
        return NULL;
    }

    snprintf(sql, sizeof(sql), SQL_MANIFEST_WARN, manifest);
    rc = sqlite3_exec(pt->db, sql, warn_cb, NULL, &errmsg);
    if (rc != SQLITE_OK) { sqlite3_free(errmsg); }

    snprintf(sql, sizeof(sql), SQL_MANIFEST_DEBUG, manifest, manifest);
    rc = sqlite3_exec(pt->db, sql, debug_cb, NULL, &errmsg);
    if (rc != SQLITE_OK) { sqlite3_free(errmsg); }

    return mh;
}

uint32_t egw_manifest_count(const egw_manifest_t *mh)
{
    return mh ? mh->count : 0;
}

const egw_ptable_tbl_t *egw_manifest_get(const egw_manifest_t *mh,
                                           uint32_t index)
{
    if (!mh || index >= mh->count) { return NULL; }
    return &mh->tables[index];
}

void egw_manifest_free(egw_manifest_t *mh)
{
    if (!mh) { return; }
    free(mh->tables);
    free(mh);
}

/* ── 行数据注册 ─────────────────────────────────────── */

/**
 * @brief 实现：SELECT * → 逐行 read_column → realloc 收集
 *
 * @param pt     DB 句柄
 * @param table  表名
 * @param fields 字段数组（buf.data + buf.len 编码）
 * @return       egw_buf_t，.data == NULL 表示失败
 */

static void read_column(void *buf, size_t offset,
                         egw_ctype_t ctype,
                         sqlite3_stmt *stmt, int col)
{
    void *dest = (uint8_t *)buf + offset;

    switch (ctype) {
    case EGW_CTYPE_F32:  *(float *)dest   = (float)sqlite3_column_double(stmt, col);          return;
    case EGW_CTYPE_F64:  *(double *)dest  = sqlite3_column_double(stmt, col);                  return;
    case EGW_CTYPE_BOOL: *(uint8_t *)dest  = (uint8_t)sqlite3_column_int64(stmt, col);  return;
    case EGW_CTYPE_U16:  *(uint16_t *)dest = (uint16_t)sqlite3_column_int64(stmt, col); return;
    case EGW_CTYPE_U32:  *(uint32_t *)dest = (uint32_t)sqlite3_column_int64(stmt, col); return;
    case EGW_CTYPE_U64:  *(uint64_t *)dest = (uint64_t)sqlite3_column_int64(stmt, col); return;
    case EGW_CTYPE_I16:  *(int16_t *)dest  = (int16_t)sqlite3_column_int64(stmt, col);  return;
    case EGW_CTYPE_I32:  *(int32_t *)dest  = (int32_t)sqlite3_column_int64(stmt, col);  return;
    case EGW_CTYPE_I64:  *(int64_t *)dest  = sqlite3_column_int64(stmt, col);            return;
    default:             return;
    }
}

/** @brief 首行解析：列名 → 列号，并校验类型。返回缺失字段数。 */
static int resolve_cols(sqlite3_stmt *stmt,
                          const egw_field_t *flds, int nfield,
                          const char *table, int *col_idx)
{
    int ncol = sqlite3_column_count(stmt);
    int missing = 0;

    for (int f = 0; f < nfield; f++) {
        col_idx[f] = -1;
        for (int c = 0; c < ncol; c++) {
            if (strcmp(flds[f].name, sqlite3_column_name(stmt, c)) == 0) {
                col_idx[f] = c;
                break;
            }
        }
        if (col_idx[f] < 0) {
            EGW_LOGE("'%s': column '%s' not found", table, flds[f].name);
            missing++;
            continue;
        }

        int expect = SQLITE_INTEGER;
        if (flds[f].ctype == EGW_CTYPE_F32 ||
            flds[f].ctype == EGW_CTYPE_F64) {
            expect = SQLITE_FLOAT;
        } else if (flds[f].ctype == EGW_CTYPE_STR) {
            expect = SQLITE_TEXT;
        }
        int actual = sqlite3_column_type(stmt, col_idx[f]);
        if (actual != expect) {
            EGW_LOGW("'%s'.%s: sqlite type %d, expected %d",
                     table, flds[f].name, actual, expect);
        }
    }

    return missing;
}

egw_buf_t egw_ptable_register(egw_ptable_t *pt,
                                const char *table,
                                egw_buf_t fields,
                                size_t row_size)
{
    egw_buf_t result = { NULL, 0 };

    if (!pt || !table || !fields.data || row_size == 0) { return result; }

    const egw_field_t *flds = (const egw_field_t *)fields.data;
    int nfield = fields.len / sizeof(egw_field_t);

    /* 先数行数，一次分配 */
    char sql[128];
    snprintf(sql, sizeof(sql), SQL_COUNT_ROWS, table);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(pt->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return result;
    }

    uint32_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = (uint32_t)sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    void *buf = calloc(count, row_size);
    if (!buf) { return result; }

    /* 再查数据 */
    snprintf(sql, sizeof(sql), SQL_SELECT_ALL, table);
    if (sqlite3_prepare_v2(pt->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        EGW_LOGE("register '%s' prepare failed", table);
        free(buf);
        return result;
    }

    /* 列名解析 + 逐行填充 */
    int col_idx[nfield];
    uint8_t *row = (uint8_t *)buf;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        if (resolve_cols(stmt, flds, nfield, table, col_idx) > 0) {
            free(buf);
            sqlite3_finalize(stmt);
            return result;
        }

        do {
            for (int f = 0; f < nfield; f++) {
                if (col_idx[f] < 0) { continue; }
                read_column(row, flds[f].offset,
                            flds[f].ctype, stmt, col_idx[f]);
            }
            row += row_size;
        } while (sqlite3_step(stmt) == SQLITE_ROW);
    }

    sqlite3_finalize(stmt);

    /* 返回结果 */
    result.data = buf;
    result.len  = count * row_size;
    return result;
}

/* ── 路由表字段表（协议无关） ────────────────────────── */

static const egw_field_t s_route_fields[] = {
    EGW_FIELD(egw_route_entry_t, "device_id", device_id, EGW_CTYPE_U16),
    EGW_FIELD(egw_route_entry_t, "sig_id",    sig_id,    EGW_CTYPE_U32),
    EGW_FIELD(egw_route_entry_t, "ctype",     ctype,     EGW_CTYPE_BOOL),
};

const egw_field_t *egw_ptable_route_fields(size_t *count)
{
    if (count) {
        *count = sizeof(s_route_fields) / sizeof(s_route_fields[0]);
    }
    return s_route_fields;
}
