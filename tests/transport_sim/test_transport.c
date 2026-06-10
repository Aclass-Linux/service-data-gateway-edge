#include "unity.h"
#include "egw_transport.h"
#include "egw_transport_internal.h"

/* ── vtable mock ──────────────────────────────── */

static int mock_close_called;
static int mock_write_called;

static egw_err_t mock_close(egw_transport_t *tp) {
    (void)tp;
    mock_close_called = 1;
    return EGW_OK;
}

static egw_err_t mock_write(egw_transport_t *tp, const void *buf, size_t len) {
    (void)tp;
    (void)buf;
    (void)len;
    mock_write_called = 1;
    return EGW_OK;
}

static void mock_destroy(egw_transport_t *tp) {
    (void)tp;
}

static const struct egw_transport_ops mock_ops = {
    .close   = mock_close,
    .write   = mock_write,
    .destroy = mock_destroy,
};

/* ── setUp / tearDown ─────────────────────────── */

void setUp(void) {
    mock_close_called = 0;
    mock_write_called = 0;
}

void tearDown(void) {
}

/* ── 参数校验 ────────────────────────────────── */

static void test_transport_close_null(void) {
    egw_err_t err = egw_transport_close(NULL);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_INVAL, err);
}

static void test_transport_write_null_tp(void) {
    egw_err_t err = egw_transport_write(NULL, "data", 4);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_INVAL, err);
}

static void test_transport_write_null_buf(void) {
    egw_transport_t tp = {0};
    tp.ops = &mock_ops;
    egw_err_t err = egw_transport_write(&tp, NULL, 4);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_INVAL, err);
}

static void test_transport_write_zero_len(void) {
    egw_transport_t tp = {0};
    tp.ops = &mock_ops;
    egw_err_t err = egw_transport_write(&tp, "data", 0);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_INVAL, err);
}

/* ── vtable 分发 ────────────────────────────────── */

static void test_transport_close_dispatch(void) {
    egw_transport_instance_t *inst = egw_transport_create();
    TEST_ASSERT_NOT_NULL(inst);

    egw_transport_t tp = {0};
    tp.ops  = &mock_ops;
    tp.inst = inst;

    TEST_ASSERT_EQUAL_INT(0, mock_close_called);
    egw_err_t err = egw_transport_close(&tp);
    TEST_ASSERT_EQUAL_INT(EGW_OK, err);

    /* 跑一次 loop 处理 async 回调 */
    uv_run(&inst->loop, UV_RUN_ONCE);
    TEST_ASSERT_EQUAL_INT(1, mock_close_called);

    egw_transport_destroy(inst);
}

static void test_transport_write_dispatch(void) {
    egw_transport_t tp = {0};
    tp.ops = &mock_ops;
    TEST_ASSERT_EQUAL_INT(0, mock_write_called);
    egw_err_t err = egw_transport_write(&tp, "hello", 5);
    TEST_ASSERT_EQUAL_INT(EGW_OK, err);
    TEST_ASSERT_EQUAL_INT(1, mock_write_called);
}

/* ── null ops ────────────────────────────────────── */

static void test_transport_write_null_ops(void) {
    egw_transport_t tp = {0};
    tp.ops = NULL;
    egw_err_t err = egw_transport_write(&tp, "data", 4);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_INVAL, err);
}

/* ── main ──────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_transport_close_null);
    RUN_TEST(test_transport_write_null_tp);
    RUN_TEST(test_transport_write_null_buf);
    RUN_TEST(test_transport_write_zero_len);
    RUN_TEST(test_transport_close_dispatch);
    RUN_TEST(test_transport_write_dispatch);
    RUN_TEST(test_transport_write_null_ops);
    return UNITY_END();
}
