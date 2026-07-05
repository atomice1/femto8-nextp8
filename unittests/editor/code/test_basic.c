/**
 * test_basic.c - Basic editor state management tests
 */

#include "p8_editor_code_test_common.h"

/* Test: State initialization */
void test_init(void **state)
{
    assert_null(lines);
    assert_null(line_lengths);
    assert_int_equal(line_count, 0);
    assert_int_equal(cursor_line, 0);
    assert_int_equal(cursor_col, 0);
    assert_int_equal(undo_depth, 0);
    assert_int_equal(redo_depth, 0);
}

/* Test: State shutdown and re-init is clean */
void test_shutdown(void **state)
{
    split_lines("Hello\nWorld\n");
    cursor_line = 1;
    cursor_col = 2;
    insert_text_at_cursor("Test");

    code_shutdown();
    code_init();

    assert_null(lines);
    assert_null(line_lengths);
    assert_int_equal(line_count, 0);
    assert_int_equal(cursor_line, 0);
    assert_int_equal(cursor_col, 0);
    assert_int_equal(undo_depth, 0);
    assert_int_equal(redo_depth, 0);
}
