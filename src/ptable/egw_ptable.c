#include "egw_ptable.h"
#include "sqlite3.h"
#include <stdlib.h>
#include <string.h>

struct egw_ptable {
    sqlite3          *db;
    egw_ptable_tbl_t *tables;
    uint32_t          table_count;
    uint32_t          table_cap;
};

/* ── 发现业务表的 exec 回调 ─────────────────────────── */

static int valid_cb(void *arg, int ncol, char **vals, char **names)
{
    (void)names;
    egw_ptable_t *pt = (egw_ptable_t *)arg;

    if (ncol < 2 || !vals[0]) { return 0; }

    if (pt->table_count >= pt->table_cap) {
        uint32_t new_cap = pt->table_cap ? pt->table_cap * 2 : 4;
        egw_ptable_tbl_t *p = realloc(pt->tables, new_cap * sizeof(egw_ptable_tbl_t));
        if (!p) { return 0; }
        pt->tables   = p;
        pt->table_cap = new_cap;
    }

    uint32_t i = pt->table_count;
    strncpy(pt->tables[i].name, vals[0], sizeof(pt->tables[i].name) - 1);
    pt->tables[i].name[sizeof(pt->tables[i].name) - 1] = '\0';
    if (vals[1]) {
        strncpy(pt->tables[i].protocol, vals[1],
                sizeof(pt->tables[i].protocol) - 1);
        pt->tables[i].protocol[sizeof(pt->tables[i].protocol) - 1] = '\0';
    }
    pt->table_count++;
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

/* ── 生命周期 ───────────────────────────────────────── */

egw_ptable_t *egw_ptable_open(const char *db_path)
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

    /* 验证 egw_head 存在 */
    sqlite3_stmt *stmt = NULL;
    const char *chk = "SELECT 1 FROM egw_head LIMIT 1";
    rc = sqlite3_prepare_v2(pt->db, chk, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(pt->db);
        free(pt);
        return NULL;
    }
    sqlite3_finalize(stmt);

    /* 发现已注册的业务表（两边都存在的） */
    char *errmsg = NULL;
    const char *sql = "SELECT m.key, m.value FROM " EGW_MANIFEST_TABLE " m "
                      "WHERE EXISTS ("
                      "  SELECT 1 FROM sqlite_master "
                      "  WHERE type='table' AND name = m.key"
                      ")";
    rc = sqlite3_exec(pt->db, sql, valid_cb, pt, &errmsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errmsg);
        sqlite3_close(pt->db);
        free(pt);
        return NULL;
    }

    /* 检查：manifest 注册了但表不存在 */
    sql = "SELECT key FROM " EGW_MANIFEST_TABLE " WHERE key NOT IN ("
          "  SELECT name FROM sqlite_master WHERE type='table'"
          ")";
    rc = sqlite3_exec(pt->db, sql, warn_cb, NULL, &errmsg);
    if (rc != SQLITE_OK) { sqlite3_free(errmsg); }

    /* 检查：表存在但 manifest 没注册 */
    sql = "SELECT name FROM sqlite_master WHERE type='table'"
          " AND name NOT IN ('egw_head', '" EGW_MANIFEST_TABLE "')"
          " AND name NOT IN (SELECT key FROM " EGW_MANIFEST_TABLE ")";
    rc = sqlite3_exec(pt->db, sql, debug_cb, NULL, &errmsg);
    if (rc != SQLITE_OK) { sqlite3_free(errmsg); }

    return pt;
}

void egw_ptable_close(egw_ptable_t *pt)
{
    if (!pt) { return; }

    sqlite3_close(pt->db);
    free(pt->tables);
    free(pt);
}

/* ── 文件头信息 ─────────────────────────────────────── */

egw_err_t egw_ptable_head_get(const egw_ptable_t *pt, egw_ptable_head_t *head)
{
    if (!pt || !head) { return EGW_RET_CODE(ERR_INVALID_ARG); }

    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT schema_version FROM egw_head";
    if (sqlite3_prepare_v2(pt->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        memset(head, 0, sizeof(*head));
        return EGW_RET_CODE(ERR_PARSE);
    }

    egw_err_t err = EGW_OK;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        head->schema_version = (uint32_t)sqlite3_column_int(stmt, 0);
    } else {
        memset(head, 0, sizeof(*head));
        err = EGW_RET_CODE(ERR_NOTFOUND);
    }

    sqlite3_finalize(stmt);
    return err;
}

/* ── 发现的表 ───────────────────────────────────────── */

uint32_t egw_ptable_table_count(const egw_ptable_t *pt)
{
    return pt ? pt->table_count : 0;
}

const egw_ptable_tbl_t *egw_ptable_table_get(const egw_ptable_t *pt,
                                               uint32_t index)
{
    if (!pt || index >= pt->table_count) { return NULL; }
    return &pt->tables[index];
}
