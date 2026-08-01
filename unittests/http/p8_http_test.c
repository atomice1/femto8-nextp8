/**
 * p8_http_test.c - Test entry point for HTTP client unit tests
 */

/* Include test headers */
#include "p8_http_test_common.h"

/* Test suite declarations */
extern void test_parse_url_http_basic(void **state);
extern void test_parse_url_http_with_port(void **state);
extern void test_parse_url_http_with_path(void **state);
extern void test_parse_url_http_with_path_and_port(void **state);
extern void test_parse_url_https_basic(void **state);
extern void test_parse_url_https_with_port(void **state);
extern void test_parse_url_https_with_path(void **state);
extern void test_parse_url_unsupported_protocol(void **state);
extern void test_parse_url_no_protocol(void **state);
extern void test_parse_url_invalid_port(void **state);
extern void test_parse_url_invalid_port_too_high(void **state);
extern void test_parse_url_invalid_port_non_numeric(void **state);
extern void test_parse_url_empty_host(void **state);
extern void test_parse_url_host_too_long(void **state);
extern void test_parse_url_path_too_long(void **state);
extern void test_parse_url_localhost(void **state);
extern void test_parse_url_ip_address(void **state);
extern void test_parse_url_path_only_root(void **state);
extern void test_parse_url_no_path_defaults_to_root(void **state);
extern void test_parse_url_errno_set_on_invalid_url(void **state);

int setup(void **state)
{
    (void)state;
    return 0;
}

int teardown(void **state)
{
    (void)state;
    return 0;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* Run all test suites */
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_parse_url_http_basic, setup, teardown),
        cmocka_unit_test_setup_teardown(test_parse_url_http_with_port, setup, teardown),
        cmocka_unit_test_setup_teardown(test_parse_url_http_with_path, setup, teardown),
        cmocka_unit_test_setup_teardown(test_parse_url_http_with_path_and_port, setup, teardown),
        cmocka_unit_test_setup_teardown(test_parse_url_https_basic, setup, teardown),
        cmocka_unit_test_setup_teardown(test_parse_url_https_with_port, setup, teardown),
        cmocka_unit_test_setup_teardown(test_parse_url_https_with_path, setup, teardown),
        cmocka_unit_test_setup_teardown(test_parse_url_unsupported_protocol, setup, teardown),
        cmocka_unit_test_setup_teardown(test_parse_url_no_protocol, setup, teardown),
        cmocka_unit_test_setup_teardown(test_parse_url_invalid_port, setup, teardown),
        cmocka_unit_test_setup_teardown(test_parse_url_invalid_port_too_high, setup, teardown),
        cmocka_unit_test_setup_teardown(test_parse_url_invalid_port_non_numeric, setup, teardown),
        cmocka_unit_test_setup_teardown(test_parse_url_empty_host, setup, teardown),
        cmocka_unit_test_setup_teardown(test_parse_url_host_too_long, setup, teardown),
        cmocka_unit_test_setup_teardown(test_parse_url_path_too_long, setup, teardown),
        cmocka_unit_test_setup_teardown(test_parse_url_localhost, setup, teardown),
        cmocka_unit_test_setup_teardown(test_parse_url_ip_address, setup, teardown),
        cmocka_unit_test_setup_teardown(test_parse_url_path_only_root, setup, teardown),
        cmocka_unit_test_setup_teardown(test_parse_url_no_path_defaults_to_root, setup, teardown),
        cmocka_unit_test_setup_teardown(test_parse_url_errno_set_on_invalid_url, setup, teardown),
    };

    int result = cmocka_run_group_tests(tests, NULL, NULL);

    return result;
}
