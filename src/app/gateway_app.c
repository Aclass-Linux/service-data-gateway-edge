#include "gateway_app.h"
#include "egw_ptable.h"
#include "egw_modbus.h"
#include <string.h>
#include <stdlib.h>

static egw_field_t master_fields[] = {
    {"device_id",  EGW_CTYPE_U16, offsetof(egw_modbus_master_t, device_id),  sizeof(uint16_t), .def_val=0},
    {"sig_id",     EGW_CTYPE_U32, offsetof(egw_modbus_master_t, sig_id),     sizeof(uint32_t), .def_val=0},
    {"func_code",  EGW_CTYPE_BOOL,offsetof(egw_modbus_master_t, func_code),  sizeof(uint8_t),  .def_val=3},
    {"reg_addr",   EGW_CTYPE_U16, offsetof(egw_modbus_master_t, reg_addr),   sizeof(uint16_t), .def_val=0},
    {"reg_count",  EGW_CTYPE_U16, offsetof(egw_modbus_master_t, reg_count),  sizeof(uint16_t), .def_val=1},
    {"ctype",      EGW_CTYPE_BOOL,offsetof(egw_modbus_master_t, ctype),      sizeof(uint8_t),  .def_val=3},
    {"poll_interval_ms", EGW_CTYPE_U32, offsetof(egw_modbus_master_t, poll_interval_ms), sizeof(uint32_t), .def_val=1000},
    {"flags",      EGW_CTYPE_BOOL,offsetof(egw_modbus_master_t, flags),      sizeof(uint8_t),  .def_val=1},
    {"scale",      EGW_CTYPE_F32, offsetof(egw_modbus_master_t, scale),      sizeof(float),    .def_val=0},
    {"offset",     EGW_CTYPE_F32, offsetof(egw_modbus_master_t, offset),     sizeof(float),    .def_val=0},
    {"deadband",   EGW_CTYPE_F32, offsetof(egw_modbus_master_t, deadband),   sizeof(float),    .def_val=0},
};

static egw_field_t slave_fields[] = {
    {"device_id",  EGW_CTYPE_U16, offsetof(egw_modbus_slave_t, device_id),  sizeof(uint16_t), .def_val=0},
    {"sig_id",     EGW_CTYPE_U32, offsetof(egw_modbus_slave_t, sig_id),     sizeof(uint32_t), .def_val=0},
    {"func_code",  EGW_CTYPE_BOOL,offsetof(egw_modbus_slave_t, func_code),  sizeof(uint8_t),  .def_val=3},
    {"reg_addr",   EGW_CTYPE_U16, offsetof(egw_modbus_slave_t, reg_addr),   sizeof(uint16_t), .def_val=0},
    {"ctype",      EGW_CTYPE_BOOL,offsetof(egw_modbus_slave_t, ctype),      sizeof(uint8_t),  .def_val=3},
    {"flags",      EGW_CTYPE_BOOL,offsetof(egw_modbus_slave_t, flags),      sizeof(uint8_t),  .def_val=1},
    {"scale",      EGW_CTYPE_F32, offsetof(egw_modbus_slave_t, scale),      sizeof(float),    .def_val=0},
    {"offset",     EGW_CTYPE_F32, offsetof(egw_modbus_slave_t, offset),     sizeof(float),    .def_val=0},
    {"deadband",   EGW_CTYPE_F32, offsetof(egw_modbus_slave_t, deadband),   sizeof(float),    .def_val=0},
};

static egw_field_t route_fields[] = {
    {"device_id",  EGW_CTYPE_U16, offsetof(egw_route_entry_t, device_id),  sizeof(uint16_t), .def_val=0},
    {"sig_id",     EGW_CTYPE_U32, offsetof(egw_route_entry_t, sig_id),     sizeof(uint32_t), .def_val=0},
    {"ctype",      EGW_CTYPE_BOOL,offsetof(egw_route_entry_t, ctype),      sizeof(uint8_t),  .def_val=3},
};

static void register_table(egw_ptable_t *pt, const char *name)
{
    const egw_field_t *flds = NULL;
    int nf = 0;
    size_t row_sz = 0;

    if (strcmp(name, "southbound") == 0) {
        flds = master_fields;
        nf   = sizeof(master_fields) / sizeof(master_fields[0]);
        row_sz = sizeof(egw_modbus_master_t);
    } else if (strcmp(name, "northbound") == 0) {
        flds = slave_fields;
        nf   = sizeof(slave_fields) / sizeof(slave_fields[0]);
        row_sz = sizeof(egw_modbus_slave_t);
    } else if (strcmp(name, "route") == 0) {
        flds = route_fields;
        nf   = sizeof(route_fields) / sizeof(route_fields[0]);
        row_sz = sizeof(egw_route_entry_t);
    } else { return; }

    egw_buf_t r = egw_ptable_register(pt, name,
                    (egw_buf_t){ .data = (void *)flds, .len = nf * sizeof(egw_field_t) },
                    row_sz);
    if (!r.data) { return; }

    int nrow = r.len / row_sz;
    for (int i = 0; i < nrow; i++) {
        uint8_t *row = (uint8_t *)r.data + i * row_sz;

        if (strcmp(name, "southbound") == 0) {
            egw_modbus_master_t *p = (egw_modbus_master_t *)row;
            EGW_LOGI("  reg south dev=%u sig=%u fc=%u addr=%u count=%u ctype=%u",
                     p->device_id, p->sig_id, p->func_code,
                     p->reg_addr, p->reg_count, p->ctype);
        } else if (strcmp(name, "northbound") == 0) {
            egw_modbus_slave_t *p = (egw_modbus_slave_t *)row;
            EGW_LOGI("  reg north dev=%u sig=%u fc=%u addr=%u ctype=%u",
                     p->device_id, p->sig_id, p->func_code,
                     p->reg_addr, p->ctype);
        } else if (strcmp(name, "route") == 0) {
            egw_route_entry_t *p = (egw_route_entry_t *)row;
            EGW_LOGI("  reg route dev=%u sig=%u ctype=%u",
                     p->device_id, p->sig_id, p->ctype);
        }
    }
    EGW_LOGI("  register %s: loaded %d row(s)", name, nrow);
    free(r.data);
}

static void on_protocol_node(egw_ptable_t *pt, egw_node_t *n)
{
    const char *manifest = n->desc;
    EGW_LOGI("  protocol = %s", manifest);
    if (!manifest || !manifest[0]) { return; }

    egw_manifest_t *mh = egw_ptable_discover(pt, manifest);
    if (!mh) { return; }

    uint32_t nt = egw_manifest_count(mh);
    EGW_LOGI("  discovered %u table(s) from %s:", nt, manifest);
    for (uint32_t i = 0; i < nt; i++) {
        const egw_ptable_tbl_t *t = egw_manifest_get(mh, i);
        EGW_LOGI("    [%u] %s  (%s)", i, t->name, t->protocol);
        register_table(pt, t->name);
    }

    egw_manifest_free(mh);
}

int egw_app_run(int argc, char *argv[])
{
    const char *db_path = "config.db";
    if (argc > 2) {
        db_path = argv[1];
    }

    egw_head_t *head = egw_ptable_head_load(db_path);
    if (!head) {
        EGW_LOGE("head_load failed (run: python tools/init_db.py %s)", db_path);
        return 1;
    }

    egw_ptable_t *pt = egw_ptable_open(db_path, head->version);
    if (!pt) {
        EGW_LOGE("ptable_open failed");
        egw_ptable_head_free(head);
        return 1;
    }

    EGW_LOGI("head.desc = %s", head->desc);
    for (egw_thread_t *th = head->threads; th; th = th->next) {
        EGW_LOGI("thread[%d] desc=%s", th->thread_id, th->desc);
        for (egw_node_t *n = th->nodes; n; n = n->next) {
            switch (n->type) {
            case EGW_THREAD_NODE_PROTOCOL:
                on_protocol_node(pt, n);
                break;
            case EGW_THREAD_NODE_PORT:
                EGW_LOGI("  port = %s", n->desc[0] ? n->desc : "(empty)");
                break;
            case EGW_THREAD_NODE_SQLITE:
                EGW_LOGI("  sqlite = %s", n->desc[0] ? n->desc : "(empty)");
                break;
            }
        }
    }

    egw_ptable_close(pt);
    egw_ptable_head_free(head);
    EGW_LOGI("done");
    return 0;
}
