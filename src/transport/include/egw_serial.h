#ifndef EGW_SERIAL_H
#define EGW_SERIAL_H

#include "egw_transport_type.h"
#include <stdint.h>

typedef struct egw_serial_params {
    const char *path;
    int32_t     baud;
    char        parity;
    int32_t     data_bits;
    int32_t     stop_bits;
} egw_serial_params_t;

const struct egw_transport *egw_serial_vtable(void);

#endif /* EGW_SERIAL_H */
