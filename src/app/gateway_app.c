#include "gateway_app.h"
#include "egw_ptable.h"

int egw_app_run(int argc, char *argv[])
{
    const char *db_path = "config.db";
    if (argc > 2) {
        db_path = argv[1];
    }

    EGW_LOGI("=== open: %s ===", db_path);

    egw_ptable_t *pt = egw_ptable_open(db_path);
    if (!pt) {
        EGW_LOGE("open returned NULL (run: python tools/init_db.py %s)", db_path);
        return 1;
    }

    /* 打印发现的表 */
    uint32_t tcount = egw_ptable_table_count(pt);
    EGW_LOGI("discovered %u table(s):", tcount);
    for (uint32_t i = 0; i < tcount; i++) {
        const egw_ptable_tbl_t *t = egw_ptable_table_get(pt, i);
        EGW_LOGI("  [%u] %s  (%s)", i, t->name, t->protocol);
    }

    egw_ptable_head_t head;
    if (egw_ptable_head_get(pt, &head) == EGW_OK) {
        EGW_LOGI("schema_version: %u", head.schema_version);
    } else {
        EGW_LOGW("no schema_version found in egw_meta");
    }

    EGW_LOGI("done");
    egw_ptable_close(pt);
    return 0;
}
