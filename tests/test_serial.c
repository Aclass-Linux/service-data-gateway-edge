/**
 * @file test_serial.c
 * @brief 串口 Transport 参数校验测试
 */

#include "unity.h"
#include "egw_serial.h"

/* ── setUp / tearDown ─────────────────────────── */

void setUp(void) {
}

void tearDown(void) {
}

/* ── 参数校验 ────────────────────────────────── */

static void test_serial_open_null_tp(void) {
    egw_serial_params_t params = {"/dev/null", 9600, 'N', 8, 1};
    egw_transport_cbs_t cbs = {NULL, NULL, NULL, NULL, NULL};
    egw_err_t err = egw_serial_open(NULL, &params, &cbs);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_HANDLER, err);
}

static void test_serial_open_null_params(void) {
    egw_transport_t *tp = NULL;
    egw_transport_cbs_t cbs = {NULL, NULL, NULL, NULL, NULL};
    egw_err_t err = egw_serial_open(&tp, NULL, &cbs);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_HANDLER, err);
}

static void test_serial_open_null_cbs(void) {
    egw_transport_t *tp = NULL;
    egw_serial_params_t params = {"/dev/null", 9600, 'N', 8, 1};
    egw_err_t err = egw_serial_open(&tp, &params, NULL);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_HANDLER, err);
}

static void test_serial_open_null_path(void) {
    egw_transport_t *tp = NULL;
    egw_serial_params_t params = {NULL, 9600, 'N', 8, 1};
    egw_transport_cbs_t cbs = {NULL, NULL, NULL, NULL, NULL};
    egw_err_t err = egw_serial_open(&tp, &params, &cbs);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_HANDLER, err);
}

/* ── main ──────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_serial_open_null_tp);
    RUN_TEST(test_serial_open_null_params);
    RUN_TEST(test_serial_open_null_cbs);
    RUN_TEST(test_serial_open_null_path);
    return UNITY_END();
}