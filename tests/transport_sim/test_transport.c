#include "unity.h"
#include "egw_serial.h"

void setUp(void) { }
void tearDown(void) { }

static void test_serial_write_null(void)
{
    egw_err_t err = egw_serial_write(NULL, "data", 4);
    TEST_ASSERT_EQUAL_INT(EGW_RET_CODE(ERR_INVALID_ARG), err);
}

static void test_serial_close_null(void)
{
    egw_serial_close(NULL);
    TEST_ASSERT_TRUE(1);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_serial_write_null);
    RUN_TEST(test_serial_close_null);
    return UNITY_END();
}
