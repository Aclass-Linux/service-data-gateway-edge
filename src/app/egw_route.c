#include "egw_route.h"

/* ── 路由表字段映射 ──────────────────────────────────── */

static const egw_field_t s_route_fields[] = {
    EGW_FIELD(egw_route_entry_t, "device_id", device_id, EGW_CTYPE_U16),
    EGW_FIELD(egw_route_entry_t, "sig_id",    sig_id,    EGW_CTYPE_U32),
    EGW_FIELD(egw_route_entry_t, "ctype",     ctype,     EGW_CTYPE_BOOL),
};

/* ── 加载 ────────────────────────────────────────────── */

static int route_lkp_cmp(const void *key, const void *row)
{
    const egw_route_key_t   *k = key;
    const egw_route_entry_t *r = row;
    if (k->device_id != r->device_id) {
        return (k->device_id > r->device_id)
             - (k->device_id < r->device_id);
    }
    return (k->sig_id > r->sig_id) - (k->sig_id < r->sig_id);
}

egw_err_t egw_route_load(egw_ptable_t *pt)
{
    size_t nf = sizeof(s_route_fields) / sizeof(s_route_fields[0]);

    egw_ptable_rs_t *rs = egw_ptable_register(pt, "route",
        &(egw_schema_t){
            .fields   = s_route_fields,
            .nfields  = nf,
            .row_size = sizeof(egw_route_entry_t),
            .order_by = "device_id, sig_id",
            .lkp      = route_lkp_cmp,
        });
    if (!rs) {
        return EGW_RET_CODE(ERR_NOMEM);
    }

    size_t nrow = egw_ptable_rs_count(rs);
    for (size_t i = 0; i < nrow; i++) {
        const egw_route_entry_t *p = egw_ptable_rs_row(rs, i);
        EGW_LOGI("  reg route dev=%u sig=%u ctype=%u",
                 p->device_id, p->sig_id, p->ctype);
    }
    EGW_LOGI("  register route: loaded %zu row(s)", nrow);
    egw_ptable_rs_free(rs);
    return EGW_OK;
}
