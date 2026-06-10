/**
 * @file test_transport.c
 * @brief Transport 通用接口测试
 *
 * 测试 egw_transport_close / egw_transport_write 的参数校验和 vtable 分发。
 */

#include "unity.h"
#include "egw_transport.h"

/* ── vtable mock ──────────────────────────────── */

static egw_err_t mock_open_called;
static egw_err_t mock_close_called;
static egw_err_t mock_write_called;

static egw_err_t mock_open(egw_transport_t *tp) {
    (void)tp;
    mock_open_called = 1;
    return EGW_OK;
}

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

static const struct egw_transport_ops mock_ops = {
    .open  = mock_open,
    .close = mock_close,
    .write = mock_write,
};

/* ── setUp / tearDown ─────────────────────────── */

void setUp(void) {
    mock_open_called  = 0;
    mock_close_called = 0;
    mock_write_called = 0;
}

void tearDown(void) {
}

/* ── 参数校验 ────────────────────────────────── */

static void test_transport_close_null(void) {
    egw_err_t err = egw_transport_close(NULL);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_HANDLER, err);
}

static void test_transport_write_null_tp(void) {
    egw_err_t err = egw_transport_write(NULL, "data", 4);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_HANDLER, err);
}

static void test_transport_write_null_buf(void) {
    egw_transport_t tp = {0};
    tp.ops = &mock_ops;
    egw_err_t err = egw_transport_write(&tp, NULL, 4);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_HANDLER, err);
}

static void test_transport_write_zero_len(void) {
    egw_transport_t tp = {0};
    tp.ops = &mock_ops;
    egw_err_t err = egw_transport_write(&tp, "data", 0);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_HANDLER, err);
}

/* ── vtable 分发 ────────────────────────────────── */

static void test_transport_close_dispatch(void) {
    egw_transport_t tp = {0};
    tp.ops = &mock_ops;
    TEST_ASSERT_EQUAL_INT(0, mock_close_called);
    egw_err_t err = egw_transport_close(&tp);
    TEST_ASSERT_EQUAL_INT(EGW_OK, err);
    TEST_ASSERT_EQUAL_INT(1, mock_close_called);
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

static void test_transport_close_null_ops(void) {
    egw_transport_t tp = {0};
    tp.ops = NULL;
    egw_err_t err = egw_transport_close(&tp);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_HANDLER, err);
}

static void test_transport_write_null_ops(void) {
    egw_transport_t tp = {0};
    tp.ops = NULL;
    egw_err_t err = egw_transport_write(&tp, "data", 4);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_HANDLER, err);
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
    RUN_TEST(test_transport_close_null_ops);
    RUN_TEST(test_transport_write_null_ops);
    return UNITY_END();
}