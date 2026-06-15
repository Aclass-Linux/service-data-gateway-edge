#include "egw_bus.h"
#include <stdlib.h>
#include <string.h>

#define MAX_SUBS 128

typedef struct {
    uint16_t    device_id;
    uint32_t    sig_id;
    egw_bus_cb  cb;
    void       *data;
} sub_t;

struct egw_bus {
    sub_t subs[MAX_SUBS];
    int   count;
};

egw_bus_t *egw_bus_create(void)
{
    egw_bus_t *bus = calloc(1, sizeof(*bus));
    return bus;
}

void egw_bus_destroy(egw_bus_t *bus)
{
    free(bus);
}

egw_err_t egw_bus_subscribe(egw_bus_t *bus, uint16_t device_id,
                             uint32_t sig_id, egw_bus_cb cb, void *data)
{
    if (!bus || !cb) {
        return EGW_RETURN_CODE(ERR_INVALID_ARG);
    }

    if (bus->count >= MAX_SUBS) {
        return EGW_ERR_BUSY;
    }

    bus->subs[bus->count].device_id = device_id;
    bus->subs[bus->count].sig_id    = sig_id;
    bus->subs[bus->count].cb        = cb;
    bus->subs[bus->count].data      = data;
    bus->count++;

    return EGW_OK;
}

egw_err_t egw_bus_unsubscribe(egw_bus_t *bus, uint16_t device_id,
                               uint32_t sig_id, egw_bus_cb cb)
{
    if (!bus || !cb) {
        return EGW_RETURN_CODE(ERR_INVALID_ARG);
    }

    for (int i = 0; i < bus->count; i++) {
        if (bus->subs[i].device_id == device_id
            && bus->subs[i].sig_id == sig_id
            && bus->subs[i].cb == cb) {
            bus->subs[i] = bus->subs[bus->count - 1];
            bus->count--;
            return EGW_OK;
        }
    }

    return EGW_ERR_NOTFOUND;
}

void egw_bus_publish(egw_bus_t *bus, uint16_t device_id,
                      uint32_t sig_id, egw_value_t value)
{
    if (!bus) {
        return;
    }

    for (int i = 0; i < bus->count; i++) {
        if (bus->subs[i].device_id == device_id
            && bus->subs[i].sig_id == sig_id) {
            bus->subs[i].cb(device_id, sig_id, value, bus->subs[i].data);
        }
    }
}
