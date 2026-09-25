/**
 * p8_dialog_test.c - Unit tests for p8_dialog functions
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "p8_dialog.h"
#include "p8_input.h"

extern void input_setup(void **state);
extern void input_teardown(void **state);
extern void test_typing_inserts_at_cursor(void **state);
extern void test_selection_delete_on_type(void **state);
extern void test_backspace_with_selection(void **state);
extern void test_backspace_without_selection(void **state);
extern void test_delete_with_selection(void **state);
extern void test_delete_without_selection(void **state);
extern void test_typing_in_middle(void **state);

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* Register all tests with setup function */
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_typing_inserts_at_cursor, input_setup, input_teardown),
        cmocka_unit_test_setup_teardown(test_selection_delete_on_type, input_setup, input_teardown),
        cmocka_unit_test_setup_teardown(test_backspace_with_selection, input_setup, input_teardown),
        cmocka_unit_test_setup_teardown(test_backspace_without_selection, input_setup, input_teardown),
        cmocka_unit_test_setup_teardown(test_delete_with_selection, input_setup, input_teardown),
        cmocka_unit_test_setup_teardown(test_delete_without_selection, input_setup, input_teardown),
        cmocka_unit_test_setup_teardown(test_typing_in_middle, input_setup, input_teardown)
    };

    /* Run the tests */
    return cmocka_run_group_tests(tests, NULL, NULL);
}

