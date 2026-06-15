#include "egw_runtime.h"
#include <stdlib.h>

static egw_runtime_t *g_runtime;

struct egw_runtime {
    egw_loop_t *loop;
    egw_bus_t  *bus;
};

egw_runtime_t *egw_runtime_create(egw_loop_t *loop, egw_bus_t *bus)
{
    egw_runtime_t *rt = calloc(1, sizeof(*rt));
    if (!rt) {
        return NULL;
    }

    rt->loop = loop;
    rt->bus  = bus;
    g_runtime = rt;
    return rt;
}

void egw_runtime_destroy(egw_runtime_t *rt)
{
    g_runtime = NULL;
    free(rt);
}

egw_runtime_t *egw_runtime_current(void)
{
    return g_runtime;
}

egw_loop_t *egw_runtime_loop(egw_runtime_t *rt)
{
    return rt ? rt->loop : NULL;
}

egw_bus_t *egw_runtime_bus(egw_runtime_t *rt)
{
    return rt ? rt->bus : NULL;
}
