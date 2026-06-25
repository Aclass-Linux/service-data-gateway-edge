#include "egw_transport_internal.h"
#include <stdlib.h>

egw_err_t egw_transport_read(egw_transport_handle_t *h, void *buf,
                              size_t *out_len, size_t cap)
{
    if (!h) { return EGW_RET_CODE(ERR_INVALID_ARG); }
    return h->read(h, buf, out_len, cap);
}

egw_err_t egw_transport_write(egw_transport_handle_t *h, const void *data,
                               size_t *out_written, size_t len)
{
    if (!h) { return EGW_RET_CODE(ERR_INVALID_ARG); }
    return h->write(h, data, out_written, len);
}

void egw_transport_close(egw_transport_handle_t *h)
{
    if (!h) { return; }
    h->close(h);
    free(h);
}

int egw_transport_get_fd(const egw_transport_handle_t *h)
{
    if (!h) { return -1; }
    return h->fd;
}
