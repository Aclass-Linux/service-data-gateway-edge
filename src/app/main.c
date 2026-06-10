#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

static void print_config(egw_conf_t *cfg) {
    char *sval;
    int32_t intval;

    printf("[MQTT]\n");
    egw_conf_get_string(cfg, "/mqtt/broker", &sval, "(null)");
    printf("  broker = %s\n", sval);
    free(sval);
    egw_conf_get_int(cfg, "/mqtt/port", &intval, 0);
    printf("  port = %d\n", intval);
    egw_conf_get_string(cfg, "/mqtt/topic_prefix", &sval, "(null)");
    printf("  topic_prefix = %s\n", sval);
    free(sval);

    printf("\n[MODBUS]\n");
    printf("  serial_ports:\n");

    int32_t n_ports;
    egw_conf_array_length(cfg, "/modbus/serial_ports", &n_ports, 0);
    if (n_ports <= 0) {
        return;
    }
    for (int32_t p = 0; p < n_ports; p++) {
        char port_path[64];
        snprintf(port_path, sizeof(port_path), "/modbus/serial_ports/%d", p);

        egw_conf_enter(cfg, port_path);
        egw_conf_get_string(cfg, "/path", &sval, "?");
        printf("    [%s]\n", sval);
        free(sval);
        egw_conf_get_int(cfg, "/baud", &intval, 9600);
        printf("      baud = %d\n", intval);
        egw_conf_get_string(cfg, "/parity", &sval, "N");
        printf("      parity = %s\n", sval);
        free(sval);

        printf("      devices:\n");

        int32_t n_devs;
        egw_conf_array_length(cfg, "/devices", &n_devs, 0);
        for (int32_t d = 0; d < n_devs; d++) {
            char dev_path[128];
            snprintf(dev_path, sizeof(dev_path), "/modbus/serial_ports/%d/devices/%d", p, d);

            egw_conf_enter(cfg, dev_path);
            egw_conf_get_int(cfg, "/slave_id", &intval, 0);
            printf("        [ID=%d]\n", intval);
            egw_conf_get_int(cfg, "/poll_interval", &intval, 5);
            printf("          poll_interval = %d\n", intval);
            egw_conf_enter(cfg, "");
        }

        egw_conf_enter(cfg, "");
    }
}

int main(int argc, char *argv[]) {
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

    print_config(cfg);
    egw_conf_free(cfg);
    return 0;
}
