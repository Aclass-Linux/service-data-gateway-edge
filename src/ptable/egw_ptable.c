#include "egw_ptable.h"
#include "sqlite3.h"
#include <stdlib.h>
#include <string.h>

struct egw_ptable {
    egw_sb_point_t   *sb_entries;
    uint32_t          sb_count;
    egw_nb_point_t   *nb_entries;
    uint32_t          nb_count;
    egw_route_entry_t *route_entries;
    uint32_t          route_count;
};

/* ── 排序比较函数 ──────────────────────────────────── */

static int cmp_sb(const void *a, const void *b)
{
    const egw_sb_point_t *pa = (const egw_sb_point_t *)a;
    const egw_sb_point_t *pb = (const egw_sb_point_t *)b;

    if (pa->device_id != pb->device_id) {
        return (pa->device_id > pb->device_id) ? 1 : -1;
    }
    if (pa->sig_id != pb->sig_id) {
        return (pa->sig_id > pb->sig_id) ? 1 : -1;
    }
    return 0;
}

static int cmp_nb(const void *a, const void *b)
{
    const egw_nb_point_t *pa = (const egw_nb_point_t *)a;
    const egw_nb_point_t *pb = (const egw_nb_point_t *)b;

    if (pa->device_id != pb->device_id) {
        return (pa->device_id > pb->device_id) ? 1 : -1;
    }
    if (pa->sig_id != pb->sig_id) {
        return (pa->sig_id > pb->sig_id) ? 1 : -1;
    }
    return 0;
}

static int cmp_route(const void *a, const void *b)
{
    const egw_route_entry_t *pa = (const egw_route_entry_t *)a;
    const egw_route_entry_t *pb = (const egw_route_entry_t *)b;

    if (pa->device_id != pb->device_id) {
        return (pa->device_id > pb->device_id) ? 1 : -1;
    }
    if (pa->sig_id != pb->sig_id) {
        return (pa->sig_id > pb->sig_id) ? 1 : -1;
    }
    return 0;
}

/* ── 加载南向表 ────────────────────────────────────── */

static egw_err_t load_southbound(sqlite3 *db, egw_sb_point_t **out,
                                  uint32_t *count)
{
    const char *sql = "SELECT COUNT(*) FROM southbound";
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *out = NULL;
        *count = 0;
        return EGW_OK;
    }

    sqlite3_step(stmt);
    uint32_t n = (uint32_t)sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (n == 0) {
        *out = NULL;
        *count = 0;
        return EGW_OK;
    }

    egw_sb_point_t *arr = calloc(n, sizeof(egw_sb_point_t));
    if (!arr) { return EGW_RET_CODE(ERR_NOMEM); }

    sql = "SELECT device_id, sig_id, func_code, reg_addr, reg_count, "
          "ctype, poll_interval_ms, flags, scale, offset, deadband "
          "FROM southbound ORDER BY device_id, sig_id";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        free(arr);
        return EGW_RET_CODE(ERR_PARSE);
    }

    uint32_t i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < n) {
        arr[i].device_id       = (uint16_t)sqlite3_column_int(stmt, 0);
        arr[i].sig_id          = (uint32_t)sqlite3_column_int(stmt, 1);
        arr[i].func_code       = (uint8_t)sqlite3_column_int(stmt, 2);
        arr[i].reg_addr        = (uint16_t)sqlite3_column_int(stmt, 3);
        arr[i].reg_count       = (uint16_t)sqlite3_column_int(stmt, 4);
        arr[i].ctype           = (uint8_t)sqlite3_column_int(stmt, 5);
        arr[i].poll_interval_ms = (uint32_t)sqlite3_column_int(stmt, 6);
        arr[i].flags           = (uint8_t)sqlite3_column_int(stmt, 7);
        arr[i].scale           = (float)sqlite3_column_double(stmt, 8);
        arr[i].offset          = (float)sqlite3_column_double(stmt, 9);
        arr[i].deadband        = (float)sqlite3_column_double(stmt, 10);
        i++;
    }
    sqlite3_finalize(stmt);

    qsort(arr, n, sizeof(egw_sb_point_t), cmp_sb);

    *out = arr;
    *count = n;
    return EGW_OK;
}

/* ── 加载北向表 ────────────────────────────────────── */

static egw_err_t load_northbound(sqlite3 *db, egw_nb_point_t **out,
                                  uint32_t *count)
{
    const char *sql = "SELECT COUNT(*) FROM northbound";
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *out = NULL;
        *count = 0;
        return EGW_OK;
    }

    sqlite3_step(stmt);
    uint32_t n = (uint32_t)sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (n == 0) {
        *out = NULL;
        *count = 0;
        return EGW_OK;
    }

    egw_nb_point_t *arr = calloc(n, sizeof(egw_nb_point_t));
    if (!arr) { return EGW_RET_CODE(ERR_NOMEM); }

    sql = "SELECT device_id, sig_id, func_code, reg_addr, "
          "ctype, flags, scale, offset, deadband "
          "FROM northbound ORDER BY device_id, sig_id";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        free(arr);
        return EGW_RET_CODE(ERR_PARSE);
    }

    uint32_t i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < n) {
        arr[i].device_id = (uint16_t)sqlite3_column_int(stmt, 0);
        arr[i].sig_id    = (uint32_t)sqlite3_column_int(stmt, 1);
        arr[i].func_code = (uint8_t)sqlite3_column_int(stmt, 2);
        arr[i].reg_addr  = (uint16_t)sqlite3_column_int(stmt, 3);
        arr[i].ctype     = (uint8_t)sqlite3_column_int(stmt, 4);
        arr[i].flags     = (uint8_t)sqlite3_column_int(stmt, 5);
        arr[i].scale     = (float)sqlite3_column_double(stmt, 6);
        arr[i].offset    = (float)sqlite3_column_double(stmt, 7);
        arr[i].deadband  = (float)sqlite3_column_double(stmt, 8);
        i++;
    }
    sqlite3_finalize(stmt);

    qsort(arr, n, sizeof(egw_nb_point_t), cmp_nb);

    *out = arr;
    *count = n;
    return EGW_OK;
}

/* ── 加载路由表 ────────────────────────────────────── */

static egw_err_t load_route(sqlite3 *db, egw_route_entry_t **out,
                             uint32_t *count)
{
    const char *sql = "SELECT COUNT(*) FROM route";
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *out = NULL;
        *count = 0;
        return EGW_OK;
    }

    sqlite3_step(stmt);
    uint32_t n = (uint32_t)sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (n == 0) {
        *out = NULL;
        *count = 0;
        return EGW_OK;
    }

    egw_route_entry_t *arr = calloc(n, sizeof(egw_route_entry_t));
    if (!arr) { return EGW_RET_CODE(ERR_NOMEM); }

    sql = "SELECT device_id, sig_id, ctype "
          "FROM route ORDER BY device_id, sig_id";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        free(arr);
        return EGW_RET_CODE(ERR_PARSE);
    }

    uint32_t i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < n) {
        arr[i].device_id = (uint16_t)sqlite3_column_int(stmt, 0);
        arr[i].sig_id    = (uint32_t)sqlite3_column_int(stmt, 1);
        arr[i].ctype     = (uint8_t)sqlite3_column_int(stmt, 2);
        i++;
    }
    sqlite3_finalize(stmt);

    qsort(arr, n, sizeof(egw_route_entry_t), cmp_route);

    *out = arr;
    *count = n;
    return EGW_OK;
}

/* ── 创建表（如果不存在）───────────────────────────── */

static egw_err_t ensure_schema(sqlite3 *db)
{
    const char *sql =
        "CREATE TABLE IF NOT EXISTS southbound ("
        "  device_id INTEGER NOT NULL,"
        "  sig_id INTEGER NOT NULL,"
        "  func_code INTEGER NOT NULL,"
        "  reg_addr INTEGER NOT NULL,"
        "  reg_count INTEGER NOT NULL,"
        "  ctype INTEGER NOT NULL,"
        "  poll_interval_ms INTEGER NOT NULL DEFAULT 1000,"
        "  flags INTEGER NOT NULL DEFAULT 1,"
        "  scale REAL NOT NULL DEFAULT 1.0,"
        "  offset REAL NOT NULL DEFAULT 0.0,"
        "  deadband REAL NOT NULL DEFAULT 0.0,"
        "  PRIMARY KEY (device_id, sig_id)"
        ");"
        "CREATE TABLE IF NOT EXISTS northbound ("
        "  device_id INTEGER NOT NULL,"
        "  sig_id INTEGER NOT NULL,"
        "  func_code INTEGER NOT NULL,"
        "  reg_addr INTEGER NOT NULL,"
        "  ctype INTEGER NOT NULL,"
        "  flags INTEGER NOT NULL DEFAULT 1,"
        "  scale REAL NOT NULL DEFAULT 1.0,"
        "  offset REAL NOT NULL DEFAULT 0.0,"
        "  deadband REAL NOT NULL DEFAULT 0.0,"
        "  PRIMARY KEY (device_id, sig_id)"
        ");"
        "CREATE TABLE IF NOT EXISTS route ("
        "  device_id INTEGER NOT NULL,"
        "  sig_id INTEGER NOT NULL,"
        "  ctype INTEGER NOT NULL,"
        "  PRIMARY KEY (device_id, sig_id)"
        ");";

    char *errmsg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errmsg);
        return EGW_RET_CODE(ERR_PARSE);
    }

    return EGW_OK;
}

/* ── 生命周期 ───────────────────────────────────────── */

egw_ptable_t *egw_ptable_open(const char *db_path)
{
    if (!db_path) { return NULL; }

    egw_ptable_t *pt = calloc(1, sizeof(*pt));
    if (!pt) { return NULL; }

    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(db_path, &db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                             NULL);
    if (rc != SQLITE_OK) {
        free(pt);
        return NULL;
    }

    sqlite3_busy_timeout(db, 5000);

    egw_err_t err = ensure_schema(db);
    if (err != EGW_OK) {
        sqlite3_close(db);
        free(pt);
        return NULL;
    }

    err = load_southbound(db, &pt->sb_entries, &pt->sb_count);
    if (err != EGW_OK) {
        sqlite3_close(db);
        free(pt);
        return NULL;
    }

    err = load_northbound(db, &pt->nb_entries, &pt->nb_count);
    if (err != EGW_OK) {
        free(pt->sb_entries);
        sqlite3_close(db);
        free(pt);
        return NULL;
    }

    err = load_route(db, &pt->route_entries, &pt->route_count);
    if (err != EGW_OK) {
        free(pt->sb_entries);
        free(pt->nb_entries);
        sqlite3_close(db);
        free(pt);
        return NULL;
    }

    sqlite3_close(db);
    return pt;
}

void egw_ptable_close(egw_ptable_t *pt)
{
    if (!pt) { return; }

    free(pt->sb_entries);
    free(pt->nb_entries);
    free(pt->route_entries);
    free(pt);
}

/* ── 查询 ──────────────────────────────────────────── */

const egw_sb_point_t *egw_ptable_sb_lookup(const egw_ptable_t *pt,
                                            uint16_t device_id,
                                            uint32_t sig_id)
{
    if (!pt || pt->sb_count == 0) { return NULL; }

    egw_sb_point_t key;
    memset(&key, 0, sizeof(key));
    key.device_id = device_id;
    key.sig_id    = sig_id;

    return (const egw_sb_point_t *)bsearch(&key, pt->sb_entries,
                                            pt->sb_count,
                                            sizeof(egw_sb_point_t), cmp_sb);
}

const egw_nb_point_t *egw_ptable_nb_lookup(const egw_ptable_t *pt,
                                            uint16_t device_id,
                                            uint32_t sig_id)
{
    if (!pt || pt->nb_count == 0) { return NULL; }

    egw_nb_point_t key;
    memset(&key, 0, sizeof(key));
    key.device_id = device_id;
    key.sig_id    = sig_id;

    return (const egw_nb_point_t *)bsearch(&key, pt->nb_entries,
                                            pt->nb_count,
                                            sizeof(egw_nb_point_t), cmp_nb);
}

const egw_route_entry_t *egw_ptable_route_lookup(const egw_ptable_t *pt,
                                                  uint16_t device_id,
                                                  uint32_t sig_id)
{
    if (!pt || pt->route_count == 0) { return NULL; }

    egw_route_entry_t key;
    memset(&key, 0, sizeof(key));
    key.device_id = device_id;
    key.sig_id    = sig_id;

    return (const egw_route_entry_t *)bsearch(&key, pt->route_entries,
                                               pt->route_count,
                                               sizeof(egw_route_entry_t),
                                               cmp_route);
}

/* ── 遍历 ──────────────────────────────────────────── */

uint32_t egw_ptable_sb_count(const egw_ptable_t *pt)
{
    return pt ? pt->sb_count : 0;
}

const egw_sb_point_t *egw_ptable_sb_at(const egw_ptable_t *pt, uint32_t index)
{
    if (!pt || index >= pt->sb_count) { return NULL; }
    return &pt->sb_entries[index];
}

uint32_t egw_ptable_nb_count(const egw_ptable_t *pt)
{
    return pt ? pt->nb_count : 0;
}

const egw_nb_point_t *egw_ptable_nb_at(const egw_ptable_t *pt, uint32_t index)
{
    if (!pt || index >= pt->nb_count) { return NULL; }
    return &pt->nb_entries[index];
}
