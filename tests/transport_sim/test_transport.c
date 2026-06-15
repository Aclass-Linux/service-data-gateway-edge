#include "unity.h"

void setUp(void) { }
void tearDown(void) { }

static void test_write_null_tp(void) {
    egw_err_t err = egw_write((egw_serial_t *)NULL, "data", 4);
    TEST_ASSERT_EQUAL_INT(EGW_RETURN_CODE(ERR_INVALID_ARG), err);
}

static void test_close_null(void) {
    egw_err_t err = egw_close((egw_serial_t *)NULL);
    TEST_ASSERT_EQUAL_INT(EGW_RETURN_CODE(ERR_INVALID_ARG), err);
}

int main(void) {
    UNITY_BEGIN();
    return UNITY_END();
}
