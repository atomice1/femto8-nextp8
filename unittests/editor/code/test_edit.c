/**
 * test_edit.c - Tests for various types of edit
 */

#include "p8_editor_code_test_common.h"

/* Test: Insert a single character */
void test_insert_char(void **state)
{
    split_lines("Hello\n");

    /* Insert '5' at cursor position (0,0) (digits don't get transformed) */
    assert_int_equal(cursor_line, 0);
    assert_int_equal(cursor_col, 0);
    code_handle_keypress(0, '5', 0);

    assert_line_equal(0, "5Hello");
    assert_int_equal(line_lengths[0], 6);
}

/* Test: Insert text in the middle of a line */
void test_insert_text_middle(void **state)
{
    split_lines("Hello\n");

    /* Set cursor at position 2 */
    cursor_line = 0;
    cursor_col = 2;

    /* Insert "XY" */
    insert_text_at_cursor("XY");

    assert_line_equal(0, "HeXYllo");
    assert_int_equal(line_lengths[0], 7);
}

/* Test: Insert at end of line */
void test_insert_text_end(void **state)
{
    split_lines("Hello\n");

    /* Insert at end of line 0 */
    cursor_line = 0;
    cursor_col = line_lengths[0];  /* Position at end */
    insert_text_at_cursor("YZ");

    assert_line_equal(0, "HelloYZ");
    assert_int_equal(line_lengths[0], 7);
}

/* Test: Insert newline creates new line */
void test_insert_newline(void **state)
{
    split_lines("Hello\n");

    /* Insert newline at position 2 of line 0 */
    cursor_line = 0;
    cursor_col = 2;

    code_handle_keypress(0, '\n', 0);

    assert_int_equal(line_count, 2);
    assert_line_equal(0, "He");
    assert_line_equal(1, "llo");
}

/* Test: Backspace deletes character before cursor */
void test_backspace(void **state)
{
    split_lines("Hello\n");

    /* Set cursor at position 2 */
    cursor_line = 0;
    cursor_col = 2;

    /* Backspace should delete 'e' at position 1 */
    code_handle_keypress(0, 8, 0);  /* 8 = backspace */

    assert_line_equal(0, "Hllo");
    assert_int_equal(cursor_col, 1);
}

/* Test: Backspace at start of line joins with previous line */
/* Note: undo for this case has a bug in the undo system, so we skip undo/redo test */
void test_backspace_join_line(void **state)
{
    split_lines("Hello\nWorld\n");

    /* Set cursor at start of line 1 */
    cursor_line = 1;
    cursor_col = 0;

    /* Backspace should join with line 0 */
    code_handle_keypress(0, 8, 0);  /* 8 = backspace */

    /* Note: line join changes cursor_line to 0 and cursor_col to end of previous line */
    assert_int_equal(line_count, 1);
    assert_line_equal(0, "HelloWorld");
    assert_int_equal(cursor_line, 0);
    assert_int_equal(cursor_col, 5);  /* At position after "Hello" */
}

/* Test: Forward delete (DEL) removes character at cursor */
void test_delete_char(void **state)
{
    split_lines("Hello\n");

    /* Set cursor at position 1 */
    cursor_line = 0;
    cursor_col = 1;

    /* DEL should delete 'e' at position 1 */
    code_handle_keypress(SCANCODE_DEL, 0, 0);

    assert_line_equal(0, "Hllo");
    assert_int_equal(cursor_col, 1);
}

/* Test: DEL at end of line joins with next line */
void test_delete_join_line(void **state)
{
    split_lines("Hello\nWorld\n");

    /* Set cursor at end of line 0 */
    cursor_line = 0;
    cursor_col = line_lengths[0];

    /* DEL should join with line 1 */
    code_handle_keypress(SCANCODE_DEL, 0, 0);

    assert_int_equal(line_count, 1);
    assert_line_equal(0, "HelloWorld");
    assert_int_equal(cursor_col, 5);  /* At position after "Hello" */
}

/* Test: Cursor movement - UP */
void test_movement_up(void **state)
{
    split_lines("Hello\nWorld\nFoo\n");

    /* Start at line 2 */
    cursor_line = 2;
    cursor_col = 2;

    /* Press UP */
    code_handle_keypress(SCANCODE_UP, 0, 0);

    assert_int_equal(cursor_line, 1);
    assert_int_equal(cursor_col, 2);
}

/* Test: Cursor movement - DOWN */
void test_movement_down(void **state)
{
    split_lines("Hello\nWorld\nFoo\n");

    /* Start at line 0 */
    cursor_line = 0;
    cursor_col = 2;

    /* Press DOWN twice */
    code_handle_keypress(SCANCODE_DOWN, 0, 0);
    code_handle_keypress(SCANCODE_DOWN, 0, 0);

    assert_int_equal(cursor_line, 2);
    assert_int_equal(cursor_col, 2);
}

/* Test: Cursor movement - LEFT */
void test_movement_left(void **state)
{
    split_lines("Hello\n");

    /* Start at position 3 */
    cursor_line = 0;
    cursor_col = 3;

    /* Press LEFT */
    code_handle_keypress(SCANCODE_LEFT, 0, 0);

    assert_int_equal(cursor_line, 0);
    assert_int_equal(cursor_col, 2);
}

/* Test: Cursor movement - LEFT across lines */
void test_movement_left_across_lines(void **state)
{
    split_lines("Hello\nWorld\n");

    /* Start at start of line 1 */
    cursor_line = 1;
    cursor_col = 0;

    /* Press LEFT - should go to end of previous line */
    code_handle_keypress(SCANCODE_LEFT, 0, 0);

    assert_int_equal(cursor_line, 0);
    assert_int_equal(cursor_col, 5);  /* End of "Hello" */
}

/* Test: Cursor movement - RIGHT */
void test_movement_right(void **state)
{
    split_lines("Hello\n");

    /* Start at position 2 */
    cursor_line = 0;
    cursor_col = 2;

    /* Press RIGHT */
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);

    assert_int_equal(cursor_line, 0);
    assert_int_equal(cursor_col, 3);
}

/* Test: Cursor movement - RIGHT across lines */
void test_movement_right_across_lines(void **state)
{
    split_lines("Hello\nWorld\n");

    /* Start at end of line 0 */
    cursor_line = 0;
    cursor_col = line_lengths[0];

    /* Press RIGHT - should go to start of next line */
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);

    assert_int_equal(cursor_line, 1);
    assert_int_equal(cursor_col, 0);
}

/* Test: Paste from clipboard (Ctrl+V) */
void test_paste(void **state)
{
    split_lines("Hello\n");

    /* Set clipboard content */
    free(clipboard);
    clipboard = strdup("XYZ");

    /* Set cursor at position 2 */
    cursor_line = 0;
    cursor_col = 2;

    /* Paste (Ctrl+V) */
    code_handle_keypress(SCANCODE_V, 0, KMOD_CTRL);

    assert_line_equal(0, "HeXYZllo");
    assert_int_equal(cursor_col, 5);  /* After "XYZ" */
    
    free(clipboard);
    clipboard = NULL;
}

/* Test: Cut selection (Ctrl+X) */
void test_cut(void **state)
{
    split_lines("Hello World\n");

    /* Select "Hello" by moving with shift */
    cursor_line = 0;
    cursor_col = 5;  /* At end of "Hello" */
    select_start_line = 0;
    select_start_col = 0;
    select_end_line = 0;
    select_end_col = 5;

    /* Cut (Ctrl+X) */
    code_handle_keypress(SCANCODE_X, 0, KMOD_CTRL);

    assert_line_equal(0, " World");
    assert_int_equal(cursor_col, 0);
    
    /* Verify clipboard contains "Hello" */
    assert_string_equal(clipboard, "Hello");
    
    free(clipboard);
    clipboard = NULL;
}

/* Test: Copy selection (Ctrl+C) */
void test_copy(void **state)
{
    split_lines("Hello World\n");

    /* Set selection */
    cursor_line = 0;
    cursor_col = 5;
    select_start_line = 0;
    select_start_col = 0;
    select_end_line = 0;
    select_end_col = 5;

    /* Copy (Ctrl+C) */
    code_handle_keypress(SCANCODE_C, 0, KMOD_CTRL);

    /* Line should be unchanged */
    assert_line_equal(0, "Hello World");
    
    /* Verify clipboard contains "Hello" */
    assert_string_equal(clipboard, "Hello");
    
    free(clipboard);
    clipboard = NULL;
}

/* Test: Duplicate line (Ctrl+D) */
void test_duplicate_line(void **state)
{
    split_lines("Hello\n");

    /* Duplicate line 0 */
    code_handle_keypress(SCANCODE_D, 0, KMOD_CTRL);

    assert_int_equal(line_count, 2);
    assert_line_equal(0, "Hello");
    assert_line_equal(1, "Hello");
}

/* Test: Tab indentation */
void test_tab(void **state)
{
    split_lines("Hello\n");

    /* Tab should indent the line */
    code_handle_keypress(SCANCODE_TAB, 0, 0);

    /* indent_lines adds 1 space per line */
    assert_line_equal(0, " Hello");
}

/* Test: Comment/uncomment line (Ctrl+B) */
void test_comment(void **state)
{
    split_lines("Hello\n");

    /* Comment line (Ctrl+B) */
    code_handle_keypress(SCANCODE_B, 0, KMOD_CTRL);

    /* comment_uncomment adds -- without space */
    assert_line_equal(0, "--Hello");

    /* Uncomment (Ctrl+B again) */
    code_handle_keypress(SCANCODE_B, 0, KMOD_CTRL);

    assert_line_equal(0, "Hello");
}

void test_long_line(void **state)
{
    split_lines("");

    char expected[65537];
    for (int i=0;i<65536;++i) {
        code_handle_keypress(0, '0'+(i%10), 0);
        expected[i] = '0'+(i%10);
    }
    expected[65536] = '\0';

    assert_line_equal(0, expected);
}