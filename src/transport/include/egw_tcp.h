#ifndef EGW_TCP_H
#define EGW_TCP_H

#include "egw_transport_type.h"
#include <stdint.h>

typedef struct {
    const char *host;
    uint16_t    port;
    uint32_t    connect_timeout_ms;
} egw_tcp_params_t;

const struct egw_transport *egw_tcp_vtable(void);

#endif /* EGW_TCP_H */
