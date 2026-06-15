#include "egw_fsm.h"
#include <stddef.h>

void egw_fsm_init(egw_fsm_t *fsm, egw_state_fn initial)
{
    if (!fsm || !initial) {
        return;
    }

    fsm->current = initial;

    egw_event_t entry_ev = { .sig = EGW_ENTRY_SIG, .data = NULL };
    initial(fsm, &entry_ev);
}

void egw_fsm_dispatch(egw_fsm_t *fsm, egw_event_t *ev)
{
    if (!fsm || !fsm->current) {
        return;
    }

    egw_state_fn source = fsm->current;
    egw_state_t  ret    = source(fsm, ev);

    if (ret == EGW_RET_TRAN) {
        egw_state_fn target = fsm->temp;

        if (target && target != source) {
            egw_event_t exit_ev  = { .sig = EGW_EXIT_SIG, .data = NULL };
            source(fsm, &exit_ev);

            fsm->current = target;

            egw_event_t entry_ev = { .sig = EGW_ENTRY_SIG, .data = NULL };
            target(fsm, &entry_ev);
        }
    }
}
