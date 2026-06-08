#include <stdio.h>
#include <string.h>
#include "config.h"

#pragma GCC diagnostic ignored "-Wformat-truncation"

static void print_config(egw_conf_t *cfg) {
    printf("[MQTT]\n");
    printf("  broker = %s\n", EGW_CONF_STR(cfg, "/mqtt/broker", "(null)"));
    printf("  port = %d\n",   EGW_CONF_INT(cfg, "/mqtt/port", 0));
    printf("  topic_prefix = %s\n", EGW_CONF_STR(cfg, "/mqtt/topic_prefix", "(null)"));

    printf("\n[MODBUS]\n");
    printf("  serial_ports:\n");

    int n_ports;
    if (EGW_CONF_ARR_LEN(cfg, "/modbus/serial_ports", &n_ports) != EGW_OK) {
        return;
    }
    for (int p = 0; p < n_ports; p++) {
        char port_path[64];
        snprintf(port_path, sizeof(port_path), "/modbus/serial_ports/%d", p);

        egw_conf_enter(cfg, port_path);
        printf("    [%s]\n", EGW_CONF_STR(cfg, "/path", "?"));
        printf("      baud = %d\n", EGW_CONF_INT(cfg, "/baud", 9600));
        printf("      parity = %s\n", EGW_CONF_STR(cfg, "/parity", "N"));

        printf("      devices:\n");

        int n_devs;
        if (EGW_CONF_ARR_LEN(cfg, "/devices", &n_devs) != EGW_OK) {
            n_devs = 0;
        }
        for (int d = 0; d < n_devs; d++) {
            char dev_path[128];
            snprintf(dev_path, sizeof(dev_path), "/modbus/serial_ports/%d/devices/%d", p, d);

            egw_conf_enter(cfg, dev_path);
            printf("        [ID=%d]\n", EGW_CONF_INT(cfg, "/slave_id", 0));
            printf("          poll_interval = %d\n", EGW_CONF_INT(cfg, "/poll_interval", 5));
            egw_conf_leave(cfg);
        }

        egw_conf_leave(cfg);
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
