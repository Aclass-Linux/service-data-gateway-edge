#ifndef EGW_TRANSPORT_INTERNAL_H
#define EGW_TRANSPORT_INTERNAL_H

#include "egw_transport.h"
#include <uv.h>
#include <stdint.h>

struct egw_transport_header {
    struct egw_transport_instance *inst;
    uint32_t                       id;
    egw_transport_cbs_t            cbs;
    bool                           opened;
    egw_err_t                    (*open_fn)(struct egw_transport_header *hdr);
    void                          (*close_fn)(struct egw_transport_header *hdr);
};

struct egw_transport_instance {
    uv_loop_t          loop;
    uint32_t           next_id;

    struct egw_transport_header **transports;
    size_t             transport_cnt;
    size_t             transport_cap;
};

egw_err_t egw_transport_add(struct egw_transport_instance *inst,
                             struct egw_transport_header *hdr);

void egw_transport_remove(struct egw_transport_instance *inst,
                           struct egw_transport_header *hdr);

void egw_transport_do_open(struct egw_transport_header *hdr);

#endif /* EGW_TRANSPORT_INTERNAL_H */
