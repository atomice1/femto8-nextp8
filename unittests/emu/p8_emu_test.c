/**
 * p8_emu_test.c - Unit test entry point for p8_emu tests
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

/* Test function declarations */
extern int wrap_text(const char *text, int max_lines, int max_chars_per_line, char *output_lines);

/* Test suite declarations */
extern void test_wrap_text_no_wrap(void **state);
extern void test_wrap_text_exact_limit(void **state);
extern void test_wrap_text_force_break(void **state);
extern void test_wrap_text_break_at_space(void **state);
extern void test_wrap_text_max_lines(void **state);
extern void test_wrap_text_empty(void **state);
extern void test_wrap_text_spaces(void **state);
extern void test_wrap_text_fits_first_line(void **state);
extern void test_wrap_text_truncation(void **state);
extern void test_wrap_text_last_space(void **state);

extern void test_make_full_path_basic(void **state);
extern void test_make_full_path_empty_dir(void **state);
extern void test_make_full_path_current_dir(void **state);
extern void test_make_full_path_dot_dot_from_current(void **state);
extern void test_make_full_path_dot_dot_from_simple_dir(void **state);
extern void test_make_full_path_dot_dot_with_trailing_slash(void **state);
extern void test_make_full_path_dot_dot_from_root(void **state);
extern void test_make_full_path_dot_dot_windows_drive_root(void **state);
extern void test_make_full_path_dot_dot_windows_at_drive_root(void **state);
extern void test_make_full_path_dot_dot_windows_subdir(void **state);
extern void test_make_full_path_dot_dot_nested(void **state);
extern void test_make_full_path_dot_dot_ending_with_dot_dot(void **state);
extern void test_make_full_path_dot_dot_from_empty(void **state);
extern void test_make_full_path_dot_dot_is_dot_dot(void **state);
extern void test_make_full_path_dot_dot_from_dot_dot(void **state);
extern void test_make_full_path_null_ret(void **state);
extern void test_make_full_path_null_dir(void **state);
extern void test_make_full_path_null_file(void **state);
extern void test_make_full_path_buffer_too_small(void **state);
extern void test_make_full_path_buffer_too_small_for_result(void **state);
extern void test_make_full_path_forward_slash(void **state);

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* Register all tests */
    const struct CMUnitTest tests[] = {
        /* wrap_text tests */
        cmocka_unit_test(test_wrap_text_no_wrap),
        cmocka_unit_test(test_wrap_text_exact_limit),
        cmocka_unit_test(test_wrap_text_force_break),
        cmocka_unit_test(test_wrap_text_break_at_space),
        cmocka_unit_test(test_wrap_text_max_lines),
        cmocka_unit_test(test_wrap_text_empty),
        cmocka_unit_test(test_wrap_text_spaces),
        cmocka_unit_test(test_wrap_text_fits_first_line),
        cmocka_unit_test(test_wrap_text_truncation),
        cmocka_unit_test(test_wrap_text_last_space),
        /* p8_make_full_path tests */
        cmocka_unit_test(test_make_full_path_basic),
        cmocka_unit_test(test_make_full_path_empty_dir),
        cmocka_unit_test(test_make_full_path_current_dir),
        cmocka_unit_test(test_make_full_path_dot_dot_from_current),
        cmocka_unit_test(test_make_full_path_dot_dot_from_simple_dir),
        cmocka_unit_test(test_make_full_path_dot_dot_with_trailing_slash),
        cmocka_unit_test(test_make_full_path_dot_dot_from_root),
        cmocka_unit_test(test_make_full_path_dot_dot_windows_drive_root),
        cmocka_unit_test(test_make_full_path_dot_dot_windows_at_drive_root),
        cmocka_unit_test(test_make_full_path_dot_dot_windows_subdir),
        cmocka_unit_test(test_make_full_path_dot_dot_nested),
        cmocka_unit_test(test_make_full_path_dot_dot_ending_with_dot_dot),
        cmocka_unit_test(test_make_full_path_dot_dot_from_empty),
        cmocka_unit_test(test_make_full_path_dot_dot_is_dot_dot),
        cmocka_unit_test(test_make_full_path_dot_dot_from_dot_dot),
        cmocka_unit_test(test_make_full_path_null_ret),
        cmocka_unit_test(test_make_full_path_null_dir),
        cmocka_unit_test(test_make_full_path_null_file),
        cmocka_unit_test(test_make_full_path_buffer_too_small),
        cmocka_unit_test(test_make_full_path_buffer_too_small_for_result),
        cmocka_unit_test(test_make_full_path_forward_slash),
    };

    /* Run the tests */
    return cmocka_run_group_tests(tests, NULL, NULL);
}
