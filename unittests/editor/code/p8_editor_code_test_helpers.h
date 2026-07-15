#ifndef P8_EDITOR_CODE_TEST_HELPERS_H
#define P8_EDITOR_CODE_TEST_HELPERS_H

#include <assert.h>
#include <cmocka.h>
#include <stdlib.h>

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
    free(actual);
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

/* Move cursor to a given line and column using keypresses */
/* First resets to (0,0) using Ctrl+Up, then moves to target position */
static inline void move_cursor_to(int line, int col)
{   
    /* Reset to (0,0) using Ctrl+Up (SCANCODE_UP with KMOD_CTRL) */
    code_handle_keypress(SCANCODE_UP, 0, KMOD_CTRL);
    
    /* Move down to the target line */
    for (int i = 0; i < line; i++) {
        code_handle_keypress(SCANCODE_DOWN, 0, 0);
    }
    
    /* Move right to the target column */
    for (int i = 0; i < col; i++) {
        code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    }
}

/* Select text from (start_line, start_col) to (end_line, end_col) using keypresses */
/* Uses Shift+Arrow keys to extend selection */
static inline void select_text(int start_line, int start_col, int end_line, int end_col)
{
    /* Move to start position first */
    move_cursor_to(start_line, start_col);

    /* Start selection by holding Shift and moving to end position */
    /* First, move horizontally within the start line */
    if (end_line == start_line) {
        /* Same line - just move right or left */
        int diff = end_col - start_col;
        if (diff > 0) {
            for (int i = 0; i < diff; i++) {
                code_handle_keypress(SCANCODE_RIGHT, 0, KMOD_SHIFT);
            }
        } else if (diff < 0) {
            for (int i = 0; i < -diff; i++) {
                code_handle_keypress(SCANCODE_LEFT, 0, KMOD_SHIFT);
            }
        }
    } else {
        /* Different lines - move down/up first, then horizontally */
        /* Move down to target line */
        int lines_diff = end_line - start_line;
        if (lines_diff > 0) {
            for (int i = 0; i < lines_diff; i++) {
                code_handle_keypress(SCANCODE_DOWN, 0, KMOD_SHIFT);
            }
        } else {
            for (int i = 0; i < -lines_diff; i++) {
                code_handle_keypress(SCANCODE_UP, 0, KMOD_SHIFT);
            }
        }

        /* Now adjust column on the target line */
        /* First get to column 0 of the target line */
        code_handle_keypress(SCANCODE_HOME, 0, KMOD_SHIFT);

        /* Move right to the target column */
        for (int i = 0; i < end_col; i++) {
            code_handle_keypress(SCANCODE_RIGHT, 0, KMOD_SHIFT);
        }
    }
}

/* Verify that all text in lines is accessible (not optimized out) */
static inline void verify_all_lines_accessible(void)
{
    if (lines == NULL || line_count <= 0) {
        return;
    }
    
    /* Read every character from every line */
    for (int i = 0; i < line_count; i++) {
        if (lines[i] != NULL && line_lengths[i] > 0) {
            /* Read each character - this ensures the compiler can't optimize away */
            volatile char sum = 0;
            for (int j = 0; j < line_lengths[i]; j++) {
                sum ^= lines[i][j];
            }
            (void)sum; /* Use volatile variable to prevent optimization */
        }
    }
}

static inline void common_post(void)
{
    if (line_count == 0) {
        assert(lines == NULL);
        assert(line_lengths == NULL);
        assert(line_capacities == NULL);
    } else {
        assert(lines != NULL);
        assert(line_lengths != NULL);
        assert(line_capacities != NULL);
    }
    verify_all_lines_accessible();
}

#endif /* P8_EDITOR_CODE_TEST_HELPERS_H */
