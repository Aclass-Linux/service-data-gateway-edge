#include "egw_persist.h"
#include "sqlite3.h"
#include <stdlib.h>
#include <stdio.h>

#define EGW_PERSIST_TABLE "runtime_value"

static const char *SQL_CREATE =
    "CREATE TABLE IF NOT EXISTS " EGW_PERSIST_TABLE " ("
    "  table_name TEXT    NOT NULL,"
    "  device_id  INTEGER NOT NULL,"
    "  reg_addr   INTEGER NOT NULL,"
    "  value      INTEGER NOT NULL DEFAULT 0,"
    "  updated_at INTEGER NOT NULL DEFAULT 0,"
    "  PRIMARY KEY (table_name, device_id, reg_addr)"
    ")";

static const char *SQL_PUT =
    "INSERT OR REPLACE INTO " EGW_PERSIST_TABLE
    " (table_name, device_id, reg_addr, value, updated_at)"
    " VALUES (?1, ?2, ?3, ?4, unixepoch('now'))";

static const char *SQL_FULL_DUMP =
    "INSERT OR REPLACE INTO " EGW_PERSIST_TABLE
    " (table_name, device_id, reg_addr, value, updated_at)"
    " SELECT ?1, device_id, reg_addr, 0, unixepoch('now')"
    " FROM %s";

struct egw_persist {
    sqlite3 *db;
};

egw_persist_t *egw_persist_open(const char *db_path)
{
    egw_persist_t *p = calloc(1, sizeof(*p));
    if (!p) { return NULL; }

    if (sqlite3_open(db_path, &p->db) != SQLITE_OK) {
        free(p);
        return NULL;
    }

    sqlite3_exec(p->db, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);

    char *err = NULL;
    if (sqlite3_exec(p->db, SQL_CREATE, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "persist: create table failed: %s\n", err);
        sqlite3_free(err);
        sqlite3_close(p->db);
        free(p);
        return NULL;
    }

    return p;
}

void egw_persist_begin(egw_persist_t *p)
{
    if (!p || !p->db) { return; }
    sqlite3_exec(p->db, "BEGIN", NULL, NULL, NULL);
}

void egw_persist_put(egw_persist_t *p, const char *table,
                      uint16_t device_id, uint16_t reg_addr,
                      uint16_t value)
{
    if (!p || !p->db) { return; }

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(p->db, SQL_PUT, -1, &stmt, NULL) != SQLITE_OK) {
        return;
    }

    sqlite3_bind_text(stmt, 1, table, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt,  2, device_id);
    sqlite3_bind_int(stmt,  3, reg_addr);
    sqlite3_bind_int(stmt,  4, value);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void egw_persist_commit(egw_persist_t *p)
{
    if (!p || !p->db) { return; }
    sqlite3_exec(p->db, "COMMIT", NULL, NULL, NULL);
}

void egw_persist_full_dump(egw_persist_t *p, const char *table)
{
    if (!p || !p->db) { return; }

    char sql[256];
    snprintf(sql, sizeof(sql), SQL_FULL_DUMP, table);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(p->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return;
    }

    sqlite3_bind_text(stmt, 1, table, -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void egw_persist_close(egw_persist_t *p)
{
    if (!p) { return; }
    if (p->db) {
        sqlite3_close(p->db);
        p->db = NULL;
    }
    free(p);
}
