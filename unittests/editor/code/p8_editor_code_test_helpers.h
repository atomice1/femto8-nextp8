#ifndef P8_EDITOR_CODE_TEST_HELPERS_H
#define P8_EDITOR_CODE_TEST_HELPERS_H

#include <cmocka.h>

#include "p8_editor_code_priv.h"
#include "strtcpy.h"

/**
 * p8_editor_test_helpers.h - Inline helper function implementations for tests
 * This file is included after p8_editor_test.h to provide inline function bodies.
 * It temporarily disables the -Dstatic= macro for proper inline function behavior.
 */

/* Assertion helpers */
static inline void assert_line_equal(int line_idx, const char *expected)
{
    int len = line_lengths[line_idx];
    char *actual = malloc(len + 1);
    memcpy(actual, lines[line_idx], len);
    actual[len] = '\0';
    assert_string_equal(actual, expected);
}

static inline void assert_cursor_position(int expected_line, int expected_col)
{
    assert_int_equal(cursor_line, expected_line);
    assert_int_equal(cursor_col, expected_col);
}

static inline void assert_line_count(int expected)
{
    assert_int_equal(line_count, expected);
}

#endif /* P8_EDITOR_CODE_TEST_HELPERS_H */
