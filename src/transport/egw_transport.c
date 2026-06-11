#include "egw_transport.h"
#include "egw_transport_internal.h"
#include <stdlib.h>
#include <string.h>

static egw_err_t ptr_array_push(void ***arr, size_t *cnt, size_t *cap,
                                 void *ptr) {
    if (*cnt >= *cap) {
        size_t new_cap = (*cap == 0) ? 4 : *cap * 2;
        void **tmp = realloc(*arr, new_cap * sizeof(tmp[0]));
        if (!tmp) {
            return EGW_ERR_NOMEM;
        }
        *arr = (void **)tmp;
        *cap = new_cap;
    }
    (*arr)[*cnt] = ptr;
    (*cnt)++;
    return EGW_OK;
}

egw_transport_instance_t *egw_transport_create(void) {
    egw_transport_instance_t *inst = calloc(1, sizeof(*inst));
    if (!inst) {
        return NULL;
    }

    if (uv_loop_init(&inst->loop) != 0) {
        free(inst);
        return NULL;
    }

    return inst;
}

void egw_transport_destroy(egw_transport_instance_t *inst) {
    if (!inst) {
        return;
    }

    for (size_t i = 0; i < inst->transport_cnt; i++) {
        struct egw_transport_header *hdr = inst->transports[i];
        if (hdr->opened) {
            hdr->close_fn(hdr);
        } else {
            free(hdr);
        }
    }

    uv_run(&inst->loop, UV_RUN_DEFAULT);

    free(inst->transports);
    uv_loop_close(&inst->loop);
    free(inst);
}

egw_err_t egw_transport_add(egw_transport_instance_t *inst,
                             struct egw_transport_header *hdr) {
    if (!inst || !hdr) {
        return EGW_ERR_INVAL;
    }

    hdr->inst = inst;
    hdr->id   = inst->next_id++;

    return ptr_array_push((void ***)&inst->transports, &inst->transport_cnt,
                           &inst->transport_cap, hdr);
}

void egw_transport_remove(egw_transport_instance_t *inst,
                           struct egw_transport_header *hdr) {
    if (!inst || !hdr) {
        return;
    }

    for (size_t i = 0; i < inst->transport_cnt; i++) {
        if (inst->transports[i] == hdr) {
            inst->transports[i] = inst->transports[--inst->transport_cnt];
            return;
        }
    }
}

void egw_transport_do_open(struct egw_transport_header *hdr) {
    if (!hdr->opened && hdr->open_fn) {
        egw_err_t err = hdr->open_fn(hdr);
        hdr->opened = (err == EGW_OK);
    }
}

void egw_transport_run(egw_transport_instance_t *inst) {
    if (!inst) {
        return;
    }

    for (size_t i = 0; i < inst->transport_cnt; i++) {
        egw_transport_do_open(inst->transports[i]);
    }

    uv_run(&inst->loop, UV_RUN_DEFAULT);
}

void egw_transport_stop(egw_transport_instance_t *inst) {
    if (!inst) {
        return;
    }
    uv_stop(&inst->loop);
}

uint32_t egw_transport_get_id(const void *tp) {
    const struct egw_transport_header *hdr = (const struct egw_transport_header *)tp;
    return hdr ? hdr->id : 0;
}

egw_transport_instance_t *egw_transport_get_inst(const void *tp) {
    const struct egw_transport_header *hdr = (const struct egw_transport_header *)tp;
    return hdr ? hdr->inst : NULL;
}

void *egw_transport_get_loop(egw_transport_instance_t *inst) {
    return inst ? (void *)&inst->loop : NULL;
}
