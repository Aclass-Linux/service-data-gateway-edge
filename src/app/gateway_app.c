#include "gateway_app.h"
#include "egw_ptable.h"
#include "egw_modbus.h"
#include "egw_serial.h"
#include <string.h>
#include <stdlib.h>

static egw_field_t master_fields[] = {
    EGW_FIELD(egw_modbus_master_t, "device_id",        device_id,        EGW_CTYPE_U16),
    EGW_FIELD(egw_modbus_master_t, "sig_id",           sig_id,           EGW_CTYPE_U32),
    EGW_FIELD(egw_modbus_master_t, "func_code",        func_code,        EGW_CTYPE_BOOL),
    EGW_FIELD(egw_modbus_master_t, "reg_addr",         reg_addr,         EGW_CTYPE_U16),
    EGW_FIELD(egw_modbus_master_t, "reg_count",        reg_count,        EGW_CTYPE_U16),
    EGW_FIELD(egw_modbus_master_t, "ctype",            ctype,            EGW_CTYPE_BOOL),
    EGW_FIELD(egw_modbus_master_t, "poll_interval_ms", poll_interval_ms, EGW_CTYPE_U32),
    EGW_FIELD(egw_modbus_master_t, "flags",            flags,            EGW_CTYPE_BOOL),
    EGW_FIELD(egw_modbus_master_t, "scale",            scale,            EGW_CTYPE_F32),
    EGW_FIELD(egw_modbus_master_t, "offset",           offset,           EGW_CTYPE_F32),
    EGW_FIELD(egw_modbus_master_t, "deadband",         deadband,         EGW_CTYPE_F32),
};

static egw_field_t slave_fields[] = {
    EGW_FIELD(egw_modbus_slave_t, "device_id", device_id, EGW_CTYPE_U16),
    EGW_FIELD(egw_modbus_slave_t, "sig_id",    sig_id,    EGW_CTYPE_U32),
    EGW_FIELD(egw_modbus_slave_t, "func_code", func_code, EGW_CTYPE_BOOL),
    EGW_FIELD(egw_modbus_slave_t, "reg_addr",  reg_addr,  EGW_CTYPE_U16),
    EGW_FIELD(egw_modbus_slave_t, "ctype",     ctype,     EGW_CTYPE_BOOL),
    EGW_FIELD(egw_modbus_slave_t, "flags",     flags,     EGW_CTYPE_BOOL),
    EGW_FIELD(egw_modbus_slave_t, "scale",     scale,     EGW_CTYPE_F32),
    EGW_FIELD(egw_modbus_slave_t, "offset",    offset,    EGW_CTYPE_F32),
    EGW_FIELD(egw_modbus_slave_t, "deadband",  deadband,  EGW_CTYPE_F32),
};

static egw_field_t route_fields[] = {
    EGW_FIELD(egw_route_entry_t, "device_id", device_id, EGW_CTYPE_U16),
    EGW_FIELD(egw_route_entry_t, "sig_id",    sig_id,    EGW_CTYPE_U32),
    EGW_FIELD(egw_route_entry_t, "ctype",     ctype,     EGW_CTYPE_BOOL),
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

static void on_port_node(egw_node_t *n)
{
    const char *path = n->desc[0] ? n->desc : "(empty)";
    EGW_LOGI("  port = %s", path);
    if (!n->desc[0]) { return; }

    const struct egw_transport *vt = egw_serial_vtable();
    egw_serial_params_t sp = {
        .path      = n->desc,
        .baud      = 9600,
        .parity    = 'N',
        .data_bits = 8,
        .stop_bits = 1,
    };
    int fd = -1;
    egw_err_t err = vt->open(&sp, &fd);
    if (err != EGW_OK || fd < 0) {
        EGW_LOGE("    open failed: err=%d", (int)err);
        return;
    }

    EGW_LOGI("    opened fd=%d", fd);
    uint8_t buf[32];
    size_t rlen = 0;
    err = vt->read(fd, buf, &rlen, sizeof(buf));
    EGW_LOGI("    read -> err=%d len=%zu", (int)err, rlen);
    vt->close(fd);
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
                on_port_node(n);
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
