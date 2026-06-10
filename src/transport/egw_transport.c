#include "egw_transport.h"
#include "egw_transport_internal.h"
#include <stdlib.h>
#include <string.h>

/* ── 辅助：数组追加 ───────────────────────────────── */

static egw_err_t array_push(struct egw_transport ***arr, size_t *cnt, size_t *cap,
                             struct egw_transport *tp) {
    if (*cnt >= *cap) {
        size_t new_cap = (*cap == 0) ? 4 : *cap * 2;
        struct egw_transport **tmp = realloc(*arr, new_cap * sizeof(tmp[0]));
        if (!tmp) {
            return EGW_ERR_NOMEM;
        }
        *arr = tmp;
        *cap = new_cap;
    }
    (*arr)[(*cnt)++] = tp;
    return EGW_OK;
}

/* ── uv_async 回调：处理跨线程关闭队列 ───────────── */

static void on_close_async(uv_async_t *handle) {
    egw_transport_instance_t *inst = (egw_transport_instance_t *)handle->data;

    pthread_mutex_lock(&inst->close_lock);
    size_t cnt = inst->close_cnt;
    struct egw_transport **entries = inst->close_queue;
    inst->close_cnt = 0;
    inst->close_cap = 0;
    inst->close_queue = NULL;
    pthread_mutex_unlock(&inst->close_lock);

    for (size_t i = 0; i < cnt; i++) {
        entries[i]->ops->close(entries[i]);
    }
    free(entries);
}

/* ── 实例生命周期 ──────────────────────────────── */

egw_transport_instance_t *egw_transport_create(void) {
    egw_transport_instance_t *inst = calloc(1, sizeof(*inst));
    if (!inst) {
        return NULL;
    }

    if (uv_loop_init(&inst->loop) != 0) {
        free(inst);
        return NULL;
    }

    inst->close_async.data = inst;
    if (uv_async_init(&inst->loop, &inst->close_async, on_close_async) != 0) {
        uv_loop_close(&inst->loop);
        free(inst);
        return NULL;
    }

    pthread_mutex_init(&inst->close_lock, NULL);
    /* 其余字段 calloc 已置零 */
    return inst;
}

egw_err_t egw_transport_register(egw_transport_instance_t *inst,
                                  const egw_transport_cfg_t *cfg,
                                  egw_transport_t **tp) {
    if (!inst || !cfg || !tp) {
        return EGW_ERR_INVAL;
    }

    struct egw_transport *new_tp = NULL;
    egw_err_t err;

    switch (cfg->type) {
        case EGW_TRANSPORT_SERIAL:
            err = egw_serial_create(&new_tp, &cfg->serial, &cfg->cbs);
            break;
        default:
            return EGW_ERR_INVAL;
    }

    if (err != EGW_OK) {
        return err;
    }

    new_tp->inst = inst;

    err = array_push(&inst->transports, &inst->transport_cnt,
                     &inst->transport_cap, new_tp);
    if (err != EGW_OK) {
        new_tp->ops->destroy(new_tp);
        return err;
    }

    *tp = new_tp;
    return EGW_OK;
}

void egw_transport_destroy(egw_transport_instance_t *inst) {
    if (!inst) {
        return;
    }

    /* 关闭所有仍存活的 transport */
    for (size_t i = 0; i < inst->transport_cnt; i++) {
        struct egw_transport *tp = inst->transports[i];
        if (tp->opened) {
            tp->ops->close(tp);
        } else {
            tp->ops->destroy(tp);
        }
    }
    free(inst->transports);
    inst->transports = NULL;
    inst->transport_cnt = 0;
    inst->transport_cap = 0;

    /* 清理 async handle → 关 loop */
    uv_close((uv_handle_t *)&inst->close_async, NULL);

    /* 跑完还剩的 close 回调 */
    uv_run(&inst->loop, UV_RUN_NOWAIT);

    uv_loop_close(&inst->loop);
    pthread_mutex_destroy(&inst->close_lock);

    /* 清理 close_queue 残留 */
    free(inst->close_queue);
    free(inst);
}

/* ── 线程控制 ──────────────────────────────────── */

void egw_transport_run(egw_transport_instance_t *inst) {
    if (!inst) {
        return;
    }

    inst->running = true;

    /* 打开所有已注册的 transport */
    for (size_t i = 0; i < inst->transport_cnt; i++) {
        struct egw_transport *tp = inst->transports[i];
        egw_err_t err = tp->ops->open(tp);
        tp->opened = (err == EGW_OK);
        /* on_open 回调已在 ops->open 内部调用 */
    }

    uv_run(&inst->loop, UV_RUN_DEFAULT);

    inst->running = false;
}

void egw_transport_stop(egw_transport_instance_t *inst) {
    if (!inst) {
        return;
    }
    uv_stop(&inst->loop);
    uv_async_send(&inst->close_async);
}

/* ── 连接操作 ──────────────────────────────────── */

egw_err_t egw_transport_close(egw_transport_t *tp) {
    if (!tp || !tp->inst) {
        return EGW_ERR_INVAL;
    }

    egw_transport_instance_t *inst = tp->inst;

    pthread_mutex_lock(&inst->close_lock);
    egw_err_t err = array_push(&inst->close_queue, &inst->close_cnt,
                                &inst->close_cap, tp);
    pthread_mutex_unlock(&inst->close_lock);

    if (err != EGW_OK) {
        return err;
    }

    uv_async_send(&inst->close_async);
    return EGW_OK;
}

egw_err_t egw_transport_write(egw_transport_t *tp, const void *buf, size_t len) {
    if (!tp || !buf || len == 0) {
        return EGW_ERR_INVAL;
    }
    if (!tp->ops || !tp->ops->write) {
        return EGW_ERR_INVAL;
    }
    return tp->ops->write(tp, buf, len);
}
