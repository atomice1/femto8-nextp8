/**
 * test_main.c - Test entry point for code editor unit tests
 */

/* Include test headers */
#include "p8_editor_code_test_common.h"

/* Test suite declarations */
extern void test_init(void **state);
extern void test_shutdown(void **state);
extern void test_insert_char(void **state);
extern void test_insert_text_middle(void **state);
extern void test_insert_text_end(void **state);
extern void test_insert_newline(void **state);
extern void test_newline(void **state);
extern void test_backspace(void **state);
extern void test_backspace_join_line(void **state);
extern void test_delete_char(void **state);
extern void test_delete_join_line(void **state);
extern void test_movement_up(void **state);
extern void test_movement_down(void **state);
extern void test_movement_left(void **state);
extern void test_movement_left_across_lines(void **state);
extern void test_movement_right(void **state);
extern void test_movement_right_across_lines(void **state);
extern void test_paste(void **state);
extern void test_cut(void **state);
extern void test_copy(void **state);
extern void test_duplicate_line(void **state);
extern void test_tab(void **state);
extern void test_comment(void **state);
extern void test_long_line(void **state);

/* Undo tests */
extern void test_undo_insert_char(void **state);
extern void test_undo_backspace(void **state);
extern void test_undo_delete_char(void **state);
extern void test_undo_insert_newline(void **state);
extern void test_undo_tab(void **state);
extern void test_undo_paste(void **state);
extern void test_undo_duplicate_line(void **state);
extern void test_undo_comment(void **state);
extern void test_undo_twice_with_one_edit(void **state);
extern void test_undo_delete_selected_text(void **state);
extern void test_undo_stack_limit(void **state);
extern void test_undo_empty_stack(void **state);
extern void test_redo_empty_stack(void **state);
extern void test_redo_twice_with_one_edit(void **state);
extern void test_redo_stack_limit(void **state);

int setup(void **state)
{
    (void) state;
    code_init();
    return 0;
}

int teardown(void **state)
{
    (void) state;
    code_shutdown();
    return 0;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* Run all test suites */
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_init, setup, teardown),
        cmocka_unit_test_setup_teardown(test_shutdown, setup, teardown),
        cmocka_unit_test_setup_teardown(test_insert_char, setup, teardown),
        cmocka_unit_test_setup_teardown(test_insert_text_middle, setup, teardown),
        cmocka_unit_test_setup_teardown(test_insert_text_end, setup, teardown),
        cmocka_unit_test_setup_teardown(test_insert_newline, setup, teardown),
        cmocka_unit_test_setup_teardown(test_newline, setup, teardown),
        cmocka_unit_test_setup_teardown(test_backspace, setup, teardown),
        cmocka_unit_test_setup_teardown(test_backspace_join_line, setup, teardown),
        cmocka_unit_test_setup_teardown(test_delete_char, setup, teardown),
        cmocka_unit_test_setup_teardown(test_delete_join_line, setup, teardown),
        cmocka_unit_test_setup_teardown(test_movement_up, setup, teardown),
        cmocka_unit_test_setup_teardown(test_movement_down, setup, teardown),
        cmocka_unit_test_setup_teardown(test_movement_left, setup, teardown),
        cmocka_unit_test_setup_teardown(test_movement_left_across_lines, setup, teardown),
        cmocka_unit_test_setup_teardown(test_movement_right, setup, teardown),
        cmocka_unit_test_setup_teardown(test_movement_right_across_lines, setup, teardown),
        cmocka_unit_test_setup_teardown(test_tab, setup, teardown),
        cmocka_unit_test_setup_teardown(test_paste, setup, teardown),
        cmocka_unit_test_setup_teardown(test_cut, setup, teardown),
        cmocka_unit_test_setup_teardown(test_copy, setup, teardown),
        cmocka_unit_test_setup_teardown(test_duplicate_line, setup, teardown),
        cmocka_unit_test_setup_teardown(test_comment, setup, teardown),
        cmocka_unit_test_setup_teardown(test_long_line, setup, teardown),
        cmocka_unit_test_setup_teardown(test_undo_insert_char, setup, teardown),
        cmocka_unit_test_setup_teardown(test_undo_backspace, setup, teardown),
        cmocka_unit_test_setup_teardown(test_undo_delete_char, setup, teardown),
        cmocka_unit_test_setup_teardown(test_undo_insert_newline, setup, teardown),
        cmocka_unit_test_setup_teardown(test_undo_tab, setup, teardown),
        cmocka_unit_test_setup_teardown(test_undo_paste, setup, teardown),
        cmocka_unit_test_setup_teardown(test_undo_duplicate_line, setup, teardown),
        cmocka_unit_test_setup_teardown(test_undo_comment, setup, teardown),
        cmocka_unit_test_setup_teardown(test_undo_delete_selected_text, setup, teardown),
        cmocka_unit_test_setup_teardown(test_undo_empty_stack, setup, teardown),
        cmocka_unit_test_setup_teardown(test_undo_twice_with_one_edit, setup, teardown),
        cmocka_unit_test_setup_teardown(test_undo_stack_limit, setup, teardown),
        cmocka_unit_test_setup_teardown(test_redo_empty_stack, setup, teardown),
        cmocka_unit_test_setup_teardown(test_redo_twice_with_one_edit, setup, teardown),
        cmocka_unit_test_setup_teardown(test_redo_stack_limit, setup, teardown),
    };

    int result = cmocka_run_group_tests(tests, NULL, NULL);

    return result;
}
