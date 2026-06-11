#include "unity.h"
#include "egw_transport.h"
#include "egw_serial.h"

void setUp(void) { }
void tearDown(void) { }

static void test_write_null_tp(void) {
    egw_err_t err = egw_write((egw_serial_t *)NULL, "data", 4);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_INVAL, err);
}

static void test_close_null(void) {
    egw_err_t err = egw_close((egw_serial_t *)NULL);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_INVAL, err);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_write_null_tp);
    RUN_TEST(test_close_null);
    return UNITY_END();
}
