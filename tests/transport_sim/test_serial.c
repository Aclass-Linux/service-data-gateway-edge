#include "unity.h"
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
    egw_transport_cfg_t cfg = {
        .type = EGW_TRANSPORT_SERIAL,
        .cbs = {NULL, NULL, NULL, NULL, NULL},
        .serial = { .path = "/dev/null", .baud = 9600 },
    };
    egw_transport_t *tp = NULL;
    egw_err_t err = egw_transport_register(NULL, &cfg, &tp);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_INVAL, err);
}

static void test_register_null_cfg(void) {
    egw_transport_t *tp = NULL;
    egw_err_t err = egw_transport_register(test_inst, NULL, &tp);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_INVAL, err);
}

static void test_register_null_path(void) {
    egw_transport_t *tp = NULL;
    egw_transport_cfg_t cfg = {
        .type = EGW_TRANSPORT_SERIAL,
        .cbs = {NULL, NULL, NULL, NULL, NULL},
        .serial = { .path = NULL, .baud = 9600 },
    };
    egw_err_t err = egw_transport_register(test_inst, &cfg, &tp);
    TEST_ASSERT_EQUAL_INT(EGW_ERR_INVAL, err);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_register_null_inst);
    RUN_TEST(test_register_null_cfg);
    RUN_TEST(test_register_null_path);
    return UNITY_END();
}
