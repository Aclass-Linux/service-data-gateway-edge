#include <stdio.h>
#include <string.h>
#include "config.h"

#pragma GCC diagnostic ignored "-Wformat-truncation"

static void print_config(egw_conf_t *cfg) {
    printf("[MQTT]\n");
    printf("  broker = %s\n", EGW_CONF_STR(cfg, "mqtt.broker", "(null)"));
    printf("  port = %d\n",   EGW_CONF_INT(cfg, "mqtt.port", 0));
    printf("  topic_prefix = %s\n", EGW_CONF_STR(cfg, "mqtt.topic_prefix", "(null)"));

    printf("\n[MODBUS]\n");
    printf("  serial_ports:\n");

    int n_ports = egw_conf_array_length(cfg, "modbus.serial_ports");
    for (int p = 0; p < n_ports; p++) {
        char base[64];
        snprintf(base, sizeof(base), "modbus.serial_ports[%d]", p);
        char field[128];

        snprintf(field, sizeof(field), "%s.path", base);
        printf("    [%s]\n", EGW_CONF_STR(cfg, field, "?"));

        snprintf(field, sizeof(field), "%s.baud", base);
        printf("      baud = %d\n", EGW_CONF_INT(cfg, field, 9600));

        snprintf(field, sizeof(field), "%s.parity", base);
        printf("      parity = %s\n", EGW_CONF_STR(cfg, field, "N"));

        printf("      devices:\n");

        char devices_base[256];
        snprintf(devices_base, sizeof(devices_base), "%s.devices", base);

        int n_devs = egw_conf_array_length(cfg, devices_base);
        for (int d = 0; d < n_devs; d++) {
            char dev_path[512];
            snprintf(dev_path, sizeof(dev_path), "%s[%d]", devices_base, d);
            char df[512];

            snprintf(df, sizeof(df), "%s.slave_id", dev_path);
            printf("        [ID=%d]\n", EGW_CONF_INT(cfg, df, 0));

            snprintf(df, sizeof(df), "%s.poll_interval", dev_path);
            printf("          poll_interval = %d\n", EGW_CONF_INT(cfg, df, 5));
        }
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

    egw_conf_t *cfg = egw_conf_load(cfg_path);
    if (!cfg) {
        fprintf(stderr, "Failed to load config\n");
        return 1;
    }

    print_config(cfg);
    egw_conf_free(cfg);
    return 0;
}
