#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "config.h"
#include "egw_transport.h"

/* ── 从 config 注册所有串口 Transport ────────────── */

static egw_err_t register_ports(egw_transport_instance_t *inst,
                                 egw_conf_t *cfg) {
    int32_t n_ports;
    egw_conf_array_length(cfg, "/modbus/serial_ports", &n_ports, 0);

    for (int32_t p = 0; p < n_ports; p++) {
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
            .on_open  = NULL,
            .on_data  = NULL,
            .on_write = NULL,
            .on_close = NULL,
        };

        egw_transport_cfg_t tcfg = {
            .type   = EGW_TRANSPORT_SERIAL,
            .cbs    = cbs,
            .serial = sp,
        };

        egw_transport_t *tp = NULL;
        egw_err_t err = egw_transport_register(inst, &tcfg, &tp);
        free(path);

        if (err != EGW_OK) {
            fprintf(stderr, "port[%d]: register failed: %d\n", p, err);
            return err;
        }

        printf("  registered port %s\n", sp.path);
    }

    return EGW_OK;
}

/* ── I/O 线程 ───────────────────────────────────── */

struct thread_arg {
    egw_transport_instance_t *inst;
};

static void *io_thread(void *arg) {
    struct thread_arg *ta = (struct thread_arg *)arg;
    printf("[I/O] starting event loop\n");
    egw_transport_run(ta->inst);
    printf("[I/O] event loop exited\n");
    return NULL;
}

/* ── main ────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    const char *cfg_path = "config.json";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            cfg_path = argv[i + 1];
            i++;
        }
    }

    /* 1. 加载配置 */
    egw_conf_t *cfg;
    egw_err_t err = egw_conf_load(cfg_path, &cfg);
    if (err != EGW_OK) {
        fprintf(stderr, "Failed to load config: %d\n", err);
        return 1;
    }

    /* 2. 创建 transport 实例 */
    egw_transport_instance_t *inst = egw_transport_create();
    if (!inst) {
        fprintf(stderr, "Failed to create transport instance\n");
        egw_conf_free(cfg);
        return 1;
    }

    /* 3. 逐个注册 transport */
    printf("Registering serial ports...\n");
    err = register_ports(inst, cfg);
    egw_conf_free(cfg);
    if (err != EGW_OK) {
        egw_transport_destroy(inst);
        return 1;
    }

    /* 4. 启动 I/O 线程 */
    pthread_t th;
    struct thread_arg ta = { .inst = inst };
    pthread_create(&th, NULL, io_thread, &ta);

    /* 5. 等待退出信号（暂用 stdin EOF） */
    printf("Press Enter to stop...\n");
    getchar();

    /* 6. 停止并清理 */
    printf("Stopping...\n");
    egw_transport_stop(inst);
    pthread_join(th, NULL);
    egw_transport_destroy(inst);

    printf("Goodbye.\n");
    return 0;
}
