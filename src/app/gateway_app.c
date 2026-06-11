#include "gateway_app.h"
#include "config.h"
#include "egw_transport.h"
#include "egw_serial.h"
#include "egw_protocol.h"
#include <uv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PORTS 32

static egw_transport_instance_t *g_inst;
static egw_serial_params_t       port_params[MAX_PORTS];
static egw_serial_t             *port_tps[MAX_PORTS];
static int                       port_ids[MAX_PORTS];
static int                       port_count;

static void on_sigint(uv_signal_t *handle, int signum) {
    (void)signum;
    printf("\nStopping...\n");
    egw_transport_stop(
        (egw_transport_instance_t *)handle->data);
}

static void on_transport_data(void *tp, const void *buf, size_t len) {
    uint32_t id = egw_transport_get_id(tp);
    egw_protocol_process(id, buf, len);
}

static void on_transport_close(void *tp, egw_err_t err) {
    if (err == EGW_OK) {
        return;
    }

    uint32_t id = egw_transport_get_id(tp);
    egw_transport_instance_t *inst = egw_transport_get_inst(tp);

    int idx = -1;
    for (int i = 0; i < port_count; i++) {
        if ((uint32_t)port_ids[i] == id) {
            idx = i;
            break;
        }
    }

    if (idx < 0) {
        fprintf(stderr, "unknown transport id %u\n", id);
        return;
    }

    printf("retrying port (path=%s)...\n", port_params[idx].path);

    egw_serial_t *new_tp = NULL;
    egw_err_t rerr = egw_serial_register(inst, &port_params[idx],
                                          &(egw_transport_cbs_t){
                                              .on_data  = on_transport_data,
                                              .on_close = on_transport_close,
                                          }, &new_tp);
    if (rerr != EGW_OK) {
        fprintf(stderr, "retry failed: %d\n", rerr);
        return;
    }

    port_tps[idx]  = new_tp;
    port_ids[idx]  = (int)egw_transport_get_id(new_tp);
    printf("  retry ok (new id=%u)\n", egw_transport_get_id(new_tp));
}

static egw_err_t register_ports(egw_transport_instance_t *inst,
                                 egw_conf_t *cfg) {
    int32_t n_ports;
    egw_conf_array_length(cfg, "/modbus/serial_ports", &n_ports, 0);

    for (int32_t p = 0; p < n_ports && p < MAX_PORTS; p++) {
        char base[64];
        snprintf(base, sizeof(base), "/modbus/serial_ports/%d", p);

        char *path = NULL;
        egw_conf_get_string(cfg, base, &path, NULL);
        if (!path) {
            fprintf(stderr, "port[%d]: missing path\n", p);
            continue;
        }

        int32_t baud = 9600;
        egw_conf_get_int(cfg, base, &baud, baud);

        char parity = 'N';
        char *parity_str = NULL;
        egw_conf_get_string(cfg, base, &parity_str, NULL);
        if (parity_str) {
            parity = parity_str[0];
            free(parity_str);
        }

        egw_serial_params_t sp = {
            .path      = path,
            .baud      = baud,
            .parity    = parity,
            .data_bits = 8,
            .stop_bits = 1,
        };

        egw_transport_cbs_t cbs = {
            .on_data  = on_transport_data,
            .on_close = on_transport_close,
        };

        egw_serial_t *tp = NULL;
        egw_err_t err = egw_serial_register(inst, &sp, &cbs, &tp);
        free(path);

        if (err != EGW_OK) {
            fprintf(stderr, "port[%d]: register failed: %d\n", p, err);
            return err;
        }

        port_params[port_count] = sp;
        port_params[port_count].path = strdup(sp.path);
        port_tps[port_count]   = tp;
        port_ids[port_count]   = (int)egw_transport_get_id(tp);
        port_count++;

        printf("  registered port %s (id=%u)\n", sp.path,
               egw_transport_get_id(tp));
    }

    return EGW_OK;
}

int egw_app_run(int argc, char *argv[]) {
    const char *cfg_path = "config.json";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            cfg_path = argv[i + 1];
            i++;
        }
    }

    egw_conf_t *cfg;
    egw_err_t err = egw_conf_load(cfg_path, &cfg);
    if (err != EGW_OK) {
        fprintf(stderr, "Failed to load config: %d\n", err);
        return 1;
    }

    egw_transport_instance_t *inst = egw_transport_create();
    g_inst = inst;
    if (!inst) {
        egw_conf_free(cfg);
        return 1;
    }

    printf("Registering serial ports...\n");
    err = register_ports(inst, cfg);
    egw_conf_free(cfg);
    if (err != EGW_OK) {
        egw_transport_destroy(inst);
        return 1;
    }

    uv_loop_t *loop = (uv_loop_t *)egw_transport_get_loop(inst);

    uv_signal_t sigint;
    uv_signal_init(loop, &sigint);
    sigint.data = inst;
    uv_signal_start(&sigint, on_sigint, SIGINT);

    printf("Running event loop (Ctrl+C to stop)...\n");
    egw_transport_run(inst);

    uv_close((uv_handle_t *)&sigint, NULL);
    uv_run(loop, UV_RUN_NOWAIT);

    egw_transport_destroy(inst);

    for (int i = 0; i < port_count; i++) {
        free((void *)port_params[i].path);
    }

    printf("Goodbye.\n");
    return 0;
}
