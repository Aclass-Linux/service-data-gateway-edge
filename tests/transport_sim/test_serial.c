#include "unity.h"
#include "egw_serial.h"
#include "egw_transport.h"

static egw_transport_instance_t *test_inst;

void setUp(void) {
    test_inst = egw_transport_create();
}

void tearDown(void) {
    if (test_inst) {
        egw_transport_destroy(test_inst);
    }
    test_inst = NULL;
}

static void test_register_null_inst(void) {
    egw_serial_params_t sp = { .path = "/dev/null", .baud = 9600 };
    egw_serial_t *tp = NULL;
    egw_err_t err = egw_serial_register(NULL, &sp, NULL, &tp);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_INVAL, err);
}

static void test_register_null_params(void) {
    egw_serial_t *tp = NULL;
    egw_err_t err = egw_serial_register(test_inst, NULL, NULL, &tp);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_INVAL, err);
}

static void test_register_null_path(void) {
    egw_serial_params_t sp = { .path = NULL, .baud = 9600 };
    egw_serial_t *tp = NULL;
    egw_err_t err = egw_serial_register(test_inst, &sp, NULL, &tp);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_INVAL, err);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_register_null_inst);
    RUN_TEST(test_register_null_params);
    RUN_TEST(test_register_null_path);
    return UNITY_END();
}
