#ifndef EGW_PROTOCOL_H
#define EGW_PROTOCOL_H

#include "egw_defs.h"
#include <stddef.h>
#include <stdint.h>

egw_err_t egw_protocol_process(uint32_t port_id, const void *data, size_t len);

#endif /* EGW_PROTOCOL_H */
