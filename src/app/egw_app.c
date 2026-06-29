#include "egw_app.h"
#include "egw_ptable.h"
#include "egw_modbus_master.h"
#include "egw_modbus_slave.h"
#include "egw_route.h"
#include <string.h>

static void on_protocol_node(egw_ptable_t *pt, egw_node_t *n,
                               egw_ptable_rs_t **out_master,
                               egw_ptable_rs_t **out_slave)
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

        if (strcmp(t->name, "southbound") == 0) {
            *out_master = egw_modbus_master_load(pt);
        } else if (strcmp(t->name, "northbound") == 0) {
            *out_slave = egw_modbus_slave_load(pt);
        } else if (strcmp(t->name, "route") == 0) {
            egw_route_load(pt);
        }
    }

    egw_manifest_free(mh);
}

static void on_port_node(egw_node_t *n)
{
    const char *path = n->desc[0] ? n->desc : "(empty)";
    EGW_LOGI("  port = %s", path);
}

/* ── Modbus RTU 本地回环 ───────────────────────────── */

static void run_modbus_loopback(egw_ptable_rs_t *master_rs,
                                  egw_ptable_rs_t *slave_rs)
{
    EGW_LOGI("=== Modbus RTU local loopback (uv_poll) ===");
    EGW_LOGI("  (requires: ./tools/virtual_serial.sh start)");

    egw_lb_ctx_t ctx = {0};
    uv_loop_t    loop;
    uv_timer_t   timer;

    if (uv_loop_init(&loop) != 0) {
        EGW_LOGE("  uv_loop_init failed");
        return;
    }
    ctx.loop = &loop;
    ctx.phase = EGW_LB_PHASE_SERVER_RECV;
    ctx.seg_pending = false;

    if (egw_lb_master_transport_open(&ctx, "/tmp/ttyV0") != EGW_OK
        || egw_lb_slave_transport_open(&ctx, "/tmp/ttyV1") != EGW_OK) {
        EGW_LOGE("  open failed — run ./tools/virtual_serial.sh start");
        goto cleanup;
    }
    EGW_LOGI("  serial: /tmp/ttyV0 (client) + /tmp/ttyV1 (server) opened");

    if (egw_lb_slave_init(&ctx, slave_rs) != EGW_OK) {
        EGW_LOGE("  slave_init failed");
        goto cleanup;
    }
    if (egw_lb_master_init(&ctx, master_rs) != EGW_OK) {
        EGW_LOGE("  master_init failed");
        goto cleanup;
    }

    if (egw_lb_master_send(&ctx) != EGW_OK) {
        EGW_LOGE("  client write failed");
        goto cleanup;
    }

    egw_lb_master_poll_init(&ctx);
    egw_lb_slave_poll_start(&ctx);

    uv_timer_init(&loop, &timer);
    timer.data = &ctx;
    uv_timer_start(&timer, egw_lb_on_timeout, 2000, 0);

    EGW_LOGI("  uv_run start...");
    uv_run(&loop, UV_RUN_DEFAULT);
    EGW_LOGI("  uv_run done (phase=%d)", ctx.phase);

cleanup:
    if (ctx.phase != EGW_LB_PHASE_DONE) {
        EGW_LOGE("  loopback did not complete cleanly");
    }

    uv_close((uv_handle_t *)&ctx.srv_poll, NULL);
    uv_close((uv_handle_t *)&ctx.cli_poll, NULL);
    uv_close((uv_handle_t *)&timer, NULL);
    uv_run(&loop, UV_RUN_DEFAULT);
    uv_loop_close(&loop);

    egw_lb_master_cleanup(&ctx);
    egw_lb_slave_cleanup(&ctx);
    EGW_LOGI("=== loopback done ===");
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

    egw_ptable_rs_t *master_rs = NULL;
    egw_ptable_rs_t *slave_rs  = NULL;

    EGW_LOGI("head.desc = %s", head->desc);
    for (egw_thread_t *th = head->threads; th; th = th->next) {
        EGW_LOGI("thread[%d] desc=%s", th->thread_id, th->desc);
        for (egw_node_t *n = th->nodes; n; n = n->next) {
            switch (n->type) {
            case EGW_THREAD_NODE_PROTOCOL:
                on_protocol_node(pt, n, &master_rs, &slave_rs);
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

    if (master_rs && slave_rs) {
        run_modbus_loopback(master_rs, slave_rs);
    }

    egw_ptable_rs_free(master_rs);
    egw_ptable_rs_free(slave_rs);
    egw_ptable_close(pt);
    egw_ptable_head_free(head);

    EGW_LOGI("done");
    return 0;
}
