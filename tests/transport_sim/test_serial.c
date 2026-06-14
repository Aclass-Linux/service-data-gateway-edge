#include "unity.h"
#include "egw_serial.h"

static void test_open_null_params(void)
{
    egw_serial_t *tp = NULL;
    egw_err_t err = egw_serial_open(NULL, &tp);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_INVAL, err);
}

static void test_open_null_path(void)
{
    egw_serial_params_t sp = { .path = NULL, .baud = 9600 };
    egw_serial_t *tp = NULL;
    egw_err_t err = egw_serial_open(&sp, &tp);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_OPEN, err);
}

static void test_close_null(void)
{
    egw_serial_close(NULL);
    /* should not crash */
    TEST_ASSERT_TRUE(1);
}

static void test_get_fd_null(void)
{
    int fd = egw_serial_get_fd(NULL);
    TEST_ASSERT_EQUAL_INT(-1, fd);
}

static void test_read_null(void)
{
    char buf[16];
    size_t len;
    egw_err_t err = egw_serial_read(NULL, buf, &len, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(EGW_ERR_INVAL, err);
}

static void test_write_null(void)
{
    egw_err_t err = egw_serial_write(NULL, "data", 4);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_INVAL, err);
}

static void test_flush_null(void)
{
    egw_err_t err = egw_serial_flush(NULL);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_INVAL, err);
}

static void test_has_pending_null(void)
{
    bool pending = egw_serial_has_pending(NULL);
    TEST_ASSERT_FALSE(pending);
}

void setUp(void) { }
void tearDown(void) { }

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_open_null_params);
    RUN_TEST(test_open_null_path);
    RUN_TEST(test_close_null);
    RUN_TEST(test_get_fd_null);
    RUN_TEST(test_read_null);
    RUN_TEST(test_write_null);
    RUN_TEST(test_flush_null);
    RUN_TEST(test_has_pending_null);
    return UNITY_END();
}
