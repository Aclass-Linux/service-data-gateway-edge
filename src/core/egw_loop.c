#include "egw_loop.h"
#include <stdlib.h>
#include <string.h>

/* ── 内部回调转发 ───────────────────────────────────── */

static void poll_cb_wrapper(uv_poll_t *handle, int status, int events)
{
    egw_poll_t *poll = (egw_poll_t *)handle;

    if (poll->cb) {
        poll->cb(poll, status, events, poll->data);
    }
}

static void timer_cb_wrapper(uv_timer_t *handle)
{
    egw_timer_t *timer = (egw_timer_t *)handle;

    if (timer->cb) {
        timer->cb(timer, timer->data);
    }
}

static void signal_cb_wrapper(uv_signal_t *handle, int signum)
{
    egw_signal_t *sig = (egw_signal_t *)handle;

    if (sig->cb) {
        sig->cb(sig, signum, sig->data);
    }
}

/* ── 事件循环生命周期实现 ──────────────────────────── */

egw_loop_t *egw_loop_create(void)
{
    egw_loop_t *loop = calloc(1, sizeof(*loop));
    if (!loop) {
        return NULL;
    }

    int ret = uv_loop_init(&loop->uv_loop);
    if (ret != 0) {
        free(loop);
        return NULL;
    }

    loop->should_stop = false;
    return loop;
}

egw_err_t egw_loop_run(egw_loop_t *loop)
{
    if (!loop) {
        return EGW_RETURN_CODE(ERR_INVALID_ARG);
    }

    int ret = uv_run(&loop->uv_loop, UV_RUN_DEFAULT);

    if (ret != 0 && !loop->should_stop) {
        return EGW_ERR_LOOP_RUN;
    }

    return EGW_OK;
}

void egw_loop_stop(egw_loop_t *loop)
{
    if (!loop) {
        return;
    }

    loop->should_stop = true;
    uv_stop(&loop->uv_loop);
}

static void close_all_cb(uv_handle_t *handle, void *arg)
{
    (void)arg;
    if (!uv_is_closing(handle)) {
        uv_close(handle, NULL);
    }
}

void egw_loop_destroy(egw_loop_t *loop)
{
    if (!loop) {
        return;
    }

    uv_walk(&loop->uv_loop, close_all_cb, NULL);
    uv_run(&loop->uv_loop, UV_RUN_DEFAULT);

    int ret = uv_loop_close(&loop->uv_loop);
    if (ret == UV_EBUSY) {
        uv_run(&loop->uv_loop, UV_RUN_DEFAULT);
        uv_loop_close(&loop->uv_loop);
    }

    free(loop);
}

/* ── fd 就绪通知（poll）实现 ────────────────────────── */

egw_err_t egw_poll_init(egw_loop_t *loop, egw_poll_t *poll, int fd)
{
    if (!loop || !poll || fd < 0) {
        return EGW_RETURN_CODE(ERR_INVALID_ARG);
    }

    memset(poll, 0, sizeof(*poll));

    int ret = uv_poll_init(&loop->uv_loop, &poll->handle, fd);
    if (ret != 0) {
        return EGW_ERR_NOMEM;
    }

    return EGW_OK;
}

egw_err_t egw_poll_start(egw_poll_t *poll, int events, egw_poll_cb cb, void *data)
{
    if (!poll || !cb) {
        return EGW_RETURN_CODE(ERR_INVALID_ARG);
    }

    poll->cb = cb;
    poll->data = data;

    int uv_events = 0;
    if (events & EGW_POLLIN)  { uv_events |= UV_READABLE; }
    if (events & EGW_POLLOUT) { uv_events |= UV_WRITABLE; }

    int ret = uv_poll_start(&poll->handle, uv_events, poll_cb_wrapper);
    if (ret != 0) {
        return EGW_ERR_LOOP_RUN;
    }

    return EGW_OK;
}

egw_err_t egw_poll_stop(egw_poll_t *poll)
{
    if (!poll) {
        return EGW_RETURN_CODE(ERR_INVALID_ARG);
    }

    uv_poll_stop(&poll->handle);
    return EGW_OK;
}

void egw_poll_close(egw_poll_t *poll)
{
    if (!poll) {
        return;
    }

    uv_close((uv_handle_t *)&poll->handle, NULL);
}

/* ── 定时器实现 ────────────────────────────────────── */

egw_err_t egw_timer_init(egw_loop_t *loop, egw_timer_t *timer)
{
    if (!loop || !timer) {
        return EGW_RETURN_CODE(ERR_INVALID_ARG);
    }

    memset(timer, 0, sizeof(*timer));

    int ret = uv_timer_init(&loop->uv_loop, &timer->handle);
    if (ret != 0) {
        return EGW_ERR_NOMEM;
    }

    return EGW_OK;
}

egw_err_t egw_timer_start(egw_timer_t *timer, uint64_t timeout_ms,
                           uint64_t repeat_ms, egw_timer_cb cb, void *data)
{
    if (!timer || !cb) {
        return EGW_RETURN_CODE(ERR_INVALID_ARG);
    }

    timer->cb = cb;
    timer->data = data;

    int ret = uv_timer_start(&timer->handle, timer_cb_wrapper,
                              timeout_ms, repeat_ms);
    if (ret != 0) {
        return EGW_ERR_LOOP_RUN;
    }

    return EGW_OK;
}

egw_err_t egw_timer_stop(egw_timer_t *timer)
{
    if (!timer) {
        return EGW_RETURN_CODE(ERR_INVALID_ARG);
    }

    uv_timer_stop(&timer->handle);
    return EGW_OK;
}

void egw_timer_close(egw_timer_t *timer)
{
    if (!timer) {
        return;
    }

    uv_close((uv_handle_t *)&timer->handle, NULL);
}

/* ── 信号实现 ──────────────────────────────────────── */

egw_err_t egw_signal_init(egw_loop_t *loop, egw_signal_t *sig,
                           int signum, egw_signal_cb cb, void *data)
{
    if (!loop || !sig || !cb) {
        return EGW_RETURN_CODE(ERR_INVALID_ARG);
    }

    memset(sig, 0, sizeof(*sig));

    int ret = uv_signal_init(&loop->uv_loop, &sig->handle);
    if (ret != 0) {
        return EGW_ERR_NOMEM;
    }

    sig->cb = cb;
    sig->data = data;

    ret = uv_signal_start(&sig->handle, signal_cb_wrapper, signum);
    if (ret != 0) {
        return EGW_ERR_LOOP_RUN;
    }

    return EGW_OK;
}

void egw_signal_close(egw_signal_t *sig)
{
    if (!sig) {
        return;
    }

    uv_close((uv_handle_t *)&sig->handle, NULL);
}

/* ── 内部访问接口 ──────────────────────────────────── */

void *egw_loop_get_uv_loop(egw_loop_t *loop)
{
    return loop ? &loop->uv_loop : NULL;
}
