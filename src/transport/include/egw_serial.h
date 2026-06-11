#ifndef EGW_SERIAL_H
#define EGW_SERIAL_H

#include "egw_transport.h"
#include "egw_serial_params.h"
#include <stddef.h>
#include <stdint.h>

typedef struct egw_serial egw_serial_t;

/* 注册 */
egw_err_t egw_serial_register(egw_transport_instance_t *inst,
                               const egw_serial_params_t *params,
                               const egw_transport_cbs_t *cbs,
                               egw_serial_t **tp);

egw_err_t egw_serial_close(egw_serial_t *tp);

egw_err_t egw_serial_write(egw_serial_t *tp, const void *buf, size_t len);

#endif
