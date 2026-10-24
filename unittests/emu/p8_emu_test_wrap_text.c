/**
 * p8_emu_test_wrap_text.c - Unit tests for wrap_text function
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

/* Test function declaration - wrap_text is defined in p8_emu.c */
int wrap_text(const char *text, int max_lines, int max_chars_per_line, char *output_lines);

/* Test helper to get line from output buffer */
static char *get_line(char *output_lines, int max_chars_per_line, int line_num)
{
    return output_lines + (line_num * (max_chars_per_line + 1));
}

/* Test 1: Simple text that doesn't need wrapping */
void test_wrap_text_no_wrap(void **state)
{
    (void)state;

    char output[2][29] = {0};
    int result = wrap_text("hello", 2, 28, output[0]);

    assert_int_equal(result, 1);
    assert_string_equal(get_line(output[0], 28, 0), "hello");
}

/* Test 2: Text exactly at limit */

void test_wrap_text_exact_limit(void **state)
{
    (void)state;

    char output[2][29];
    /* 28 characters */
    char text[29] = "1234567890123456789012345678";
    text[28] = '\0';

    int result = wrap_text(text, 2, 28, output[0]);

    assert_int_equal(result, 1);
    assert_string_equal(get_line(output[0], 28, 0), text);
}

/* Test 3: Text just over limit (no space to break) */

void test_wrap_text_force_break(void **state)
{
    (void)state;

    char output[2][29] = {0};
    /* 29 characters, no spaces */
    char text[30] = "12345678901234567890123456789";
    text[29] = '\0';

    int result = wrap_text(text, 2, 28, output[0]);

    assert_int_equal(result, 2);
    assert_string_equal(get_line(output[0], 28, 0), "1234567890123456789012345678");
    assert_string_equal(get_line(output[0], 28, 1), "9");
}

/* Test 4: Text breaks at space */

void test_wrap_text_break_at_space(void **state)
{
    (void)state;

    char output[2][29] = {0};
    /* Text that's 29 chars, needs wrapping */
    char text[] = "hello world this is a test!!!";
    /* len=29, one over limit - should wrap at last space */

    int result = wrap_text(text, 2, 28, output[0]);

    assert_int_equal(result, 2);
    /* Should break at last space before position 28 */
    assert_string_equal(get_line(output[0], 28, 0), "hello world this is a");
    assert_string_equal(get_line(output[0], 28, 1), "test!!!");
}

/* Test 5: Text with multiple lines available */

void test_wrap_text_max_lines(void **state)
{
    (void)state;

    char output[4][29] = {0};
    char text[] = "line1 line2 line3 line4 line5";

    int result = wrap_text(text, 4, 28, output[0]);

    assert_int_equal(result, 2);  /* Only 2 lines needed */
}

/* Test 6: Empty string */

void test_wrap_text_empty(void **state)
{
    (void)state;

    char output[2][29];
    int result = wrap_text("", 2, 28, output[0]);

    assert_int_equal(result, 1);
    assert_string_equal(get_line(output[0], 28, 0), "");
}

/* Test 7: Text with only spaces over limit */

void test_wrap_text_spaces(void **state)
{
    (void)state;

    char output[2][29] = {0};
    char text[] = "                             ";  /* 29 spaces */

    int result = wrap_text(text, 2, 28, output[0]);

    assert_int_equal(result, 2);
    assert_int_equal(get_line(output[0], 28, 0)[0], ' ');
    assert_int_equal(get_line(output[0], 28, 1)[0], ' ');
}

/* Test 8: Text that fits in first line of multiple */

void test_wrap_text_fits_first_line(void **state)
{
    (void)state;

    char output[4][29];
    char text[] = "short text";

    int result = wrap_text(text, 4, 28, output[0]);

    assert_int_equal(result, 1);
    assert_string_equal(get_line(output[0], 28, 0), "short text");
}

/* Test 9: Truncation when more lines needed than max_lines */

void test_wrap_text_truncation(void **state)
{
    (void)state;

    char output[2][29] = {0};
    /* Create text that would need 3 lines but we only allow 2 */
    char text[60];
    memset(text, 'x', 59);
    text[59] = '\0';

    int result = wrap_text(text, 2, 28, output[0]);

    assert_int_equal(result, 2);
    assert_int_equal(strlen(get_line(output[0], 28, 0)), 28);
    assert_int_equal(strlen(get_line(output[0], 28, 1)), 28);
}

/* Test 10: Break at last space before limit */

void test_wrap_text_last_space(void **state)
{
    (void)state;

    char output[2][29] = {0};
    /* Text where last space is at position 27 */
    char text[] = "this is a text with last space";
    /* Positions: 0123456789012345678901234567 */
    /*            0         1         2      */
    /* Last space before 28 is at position 27 (before "space") */

    int result = wrap_text(text, 2, 28, output[0]);

    assert_int_equal(result, 2);
    /* Should break at position 27, which is after "last" */
    assert_string_equal(get_line(output[0], 28, 0), "this is a text with last");
    assert_string_equal(get_line(output[0], 28, 1), "space");
}
