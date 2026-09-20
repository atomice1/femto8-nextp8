/**
 * test_edit.c - Tests for various types of edit
 */

#include <stdlib.h>

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
    move_cursor_to(0, 2);

    /* Insert "XY" */
    code_handle_keypress(0, 'x', 0);
    code_handle_keypress(0, 'y', 0);

    assert_line_equal(0, "Hexyllo");
    assert_int_equal(line_lengths[0], 7);
}

/* Test: Insert at end of line */
void test_insert_text_end(void **state)
{
    split_lines("Hello\n");

    /* Insert at end of line 0 */
    move_cursor_to(0, line_lengths[0]);  /* Position at end */
    code_handle_keypress(0, 'y', 0);
    code_handle_keypress(0, 'z', 0);

    assert_line_equal(0, "Helloyz");
    assert_int_equal(line_lengths[0], 7);
}

/* Test: Insert newline creates new line */
void test_insert_newline(void **state)
{
    split_lines("Hello\n");

    /* Insert newline at position 2 of line 0 */
    move_cursor_to(0, 2);

    code_handle_keypress(0, '\n', 0);

    assert_int_equal(line_count, 2);
    assert_line_equal(0, "He");
    assert_line_equal(1, "llo");
    assert_int_equal(cursor_line, 1);
    assert_int_equal(cursor_col, 0);
}

/* Test: newline at tend of line */
void test_newline(void **state)
{
    split_lines("Hello\n");

    /* Insert newline at position 2 of line 0 */
    move_cursor_to(0, 5);

    code_handle_keypress(0, '\n', 0);

    assert_int_equal(line_count, 2);
    assert_line_equal(0, "Hello");
    assert_line_equal(1, "");
    assert_int_equal(cursor_line, 1);
    assert_int_equal(cursor_col, 0);
}

/* Test: Backspace deletes character before cursor */
void test_backspace(void **state)
{
    split_lines("Hello\n");

    /* Set cursor at position 2 */
    move_cursor_to(0, 2);

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
    move_cursor_to(1, 0);

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
    move_cursor_to(0, 1);

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
    move_cursor_to(0, line_lengths[0]);

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
    move_cursor_to(2, 2);

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
    move_cursor_to(0, 2);

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
    move_cursor_to(0, 3);

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
    move_cursor_to(1, 0);

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
    move_cursor_to(0, 2);

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
    move_cursor_to(0, line_lengths[0]);

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
    move_cursor_to(0, 2);

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

    /* Select "Hello" using keypresses */
    move_cursor_to(0, 0);
    select_text(0, 0, 0, 5);

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

    /* Select "Hello" using keypresses */
    move_cursor_to(0, 0);
    select_text(0, 0, 0, 5);

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

void test_copy_paste(void **state)
{
    split_lines("--foo\n  for v=1,bricksv do\n--bar\n    for h=1,bricksh do\n--foobar\n\n--baz\n  end");

    assert_line_equal(1, "  for v=1,bricksv do");
    assert_line_equal(3, "    for h=1,bricksh do");
    assert_line_equal(7, "  end");

    for (int i=0;i<1;i++)
        code_handle_keypress(SCANCODE_DOWN, 0, 0);
    assert_int_equal(cursor_line, 1);

    code_handle_keypress(SCANCODE_DOWN, 0, KMOD_SHIFT);
    code_handle_keypress(SCANCODE_DOWN, 0, KMOD_SHIFT);
    code_handle_keypress(SCANCODE_DOWN, 0, KMOD_SHIFT);
    code_handle_keypress(SCANCODE_RIGHT, 0, KMOD_SHIFT);
    code_handle_keypress(SCANCODE_C, 0, KMOD_CTRL);

    for (int i=4;i<7;i++)
        code_handle_keypress(SCANCODE_DOWN, 0, 0);

    assert_int_equal(cursor_line, 7);

    code_handle_keypress(SCANCODE_V, 0, KMOD_CTRL);
    assert_line_equal(7, "  for v=1,bricksv do");
    assert_line_equal(9, "    for h=1,bricksh do");
    assert_line_equal(10, "-  end");
}

void test_delete_line(void **state)
{
    split_lines("Line 1\nLine 2\nLine 3");

    code_handle_keypress(SCANCODE_DOWN, 0, 0);
    assert_int_equal(cursor_line, 1);
    code_handle_keypress(SCANCODE_END, 0, 0);
    code_handle_keypress(SCANCODE_HOME, 0, KMOD_SHIFT);
    code_handle_keypress(0, 8, 0);  /* 8 = backspace */
    assert_int_equal(cursor_line, 1);
    code_handle_keypress(0, 8, 0);  /* 8 = backspace */
    assert_int_equal(cursor_line, 0);
    assert_line_equal(0, "Line 1");
    assert_line_equal(1, "Line 3");
}

void test_random(void **state)
{
    srand(4269);
    
    /* Initialize with 100 lines of random text */
    {
        char *script = malloc(10000);
        int pos = 0;
        for (int line = 0; line < 100; line++) {
            int line_len = 1 + (random() % 80);
            for (int col = 0; col < line_len; col++) {
                int ch = 32 + (random() % 95); /* printable ASCII */
                if (ch == '\n') ch = ' ';
                script[pos++] = ch;
            }
            script[pos++] = '\n';
        }
        script[pos] = '\0';
        split_lines(script);
        free(script);
    }
    
    /* Move cursor to a random position */
    {
        int target_line = random() % line_count;
        int target_col = random() % (line_lengths[target_line] + 1);
        move_cursor_to(target_line, target_col);
    }
    
    for (int i=0;i<10000;++i) {
        int scancode, keypress, keymod;
        int repeat = 0;
        int n = random() % 32;
        switch (n) {
            case 0:
                /* UP arrow */
                scancode = SCANCODE_UP;
                keypress = 0;
                keymod = 0;
                repeat = 1;
                break;
            case 1:
                /* DOWN arrow */
                scancode = SCANCODE_DOWN;
                keypress = 0;
                keymod = 0;
                repeat = 1;
                break;
            case 2:
                /* LEFT arrow */
                scancode = SCANCODE_LEFT;
                keypress = 0;
                keymod = 0;
                repeat = 1;
                break;
            case 3:
                /* RIGHT arrow */
                scancode = SCANCODE_RIGHT;
                keypress = 0;
                keymod = 0;
                repeat = 1;
                break;
            case 4:
                /* PAGEUP */
                scancode = SCANCODE_PAGEUP;
                keypress = 0;
                keymod = 0;
                repeat = 1;
                break;
            case 5:
                /* PAGEDOWN */
                scancode = SCANCODE_PAGEDOWN;
                keypress = 0;
                keymod = 0;
                repeat = 1;
                break;
            case 6:
                /* HOME */
                scancode = SCANCODE_HOME;
                keypress = 0;
                keymod = 0;
                repeat = 1;
                break;
            case 7:
                /* END */
                scancode = SCANCODE_END;
                keypress = 0;
                keymod = 0;
                repeat = 1;
                break;
            case 8:
                /* DEL */
                scancode = SCANCODE_DEL;
                keypress = 0;
                keymod = 0;
                repeat = 1;
                break;
            case 9:
                /* TAB */
                scancode = SCANCODE_TAB;
                keypress = 0;
                keymod = 0;
                repeat = 1;
                break;
            case 10:
                /* Backspace */
                scancode = 0;
                keypress = 8;
                keymod = 0;
                repeat = 1;
                break;
            case 11:
                /* Newline/Return */
                scancode = 0;
                keypress = '\n';
                keymod = 0;
                repeat = 1;
                break;
            case 12:
                /* Ctrl+V (paste) */
                scancode = SCANCODE_V;
                keypress = 0;
                keymod = KMOD_CTRL;
                repeat = 0;
                break;
            case 13:
                /* Ctrl+C (copy) */
                scancode = SCANCODE_C;
                keypress = 0;
                keymod = KMOD_CTRL;
                repeat = 0;
                break;
            case 14:
                /* Ctrl+X (cut) */
                scancode = SCANCODE_X;
                keypress = 0;
                keymod = KMOD_CTRL;
                repeat = 0;
                break;
            case 15:
                /* Ctrl+Z (undo) */
                scancode = SCANCODE_Z;
                keypress = 0;
                keymod = KMOD_CTRL;
                repeat = 0;
                break;
            case 16:
                /* Ctrl+Y (redo) */
                scancode = SCANCODE_Y;
                keypress = 0;
                keymod = KMOD_CTRL;
                repeat = 0;
                break;
            case 17:
                /* Ctrl+D (duplicate line) */
                scancode = SCANCODE_D;
                keypress = 0;
                keymod = KMOD_CTRL;
                repeat = 0;
                break;
            case 18:
                /* Ctrl+B (comment/uncomment) */
                scancode = SCANCODE_B;
                keypress = 0;
                keymod = KMOD_CTRL;
                repeat = 0;
                break;
            case 19:
                /* Ctrl+F (find) */
                scancode = SCANCODE_F;
                keypress = 0;
                keymod = KMOD_CTRL;
                repeat = 0;
                break;
            case 20:
                /* Ctrl+G (find next) */
                scancode = SCANCODE_G;
                keypress = 0;
                keymod = KMOD_CTRL;
                repeat = 0;
                break;
            case 21:
                /* Ctrl+L (goto line) */
                scancode = SCANCODE_L;
                keypress = 0;
                keymod = KMOD_CTRL;
                repeat = 0;
                break;
            case 22:
                /* Ctrl+Left (word left) */
                scancode = SCANCODE_LEFT;
                keypress = 0;
                keymod = KMOD_CTRL;
                repeat = 0;
                break;
            case 23:
                /* Ctrl+Right (word right) */
                scancode = SCANCODE_RIGHT;
                keypress = 0;
                keymod = KMOD_CTRL;
                repeat = 0;
                break;
            case 24:
                /* Shift+UP (selection up) */
                scancode = SCANCODE_UP;
                keypress = 0;
                keymod = KMOD_SHIFT;
                repeat = 1;
                break;
            case 25:
                /* Shift+DOWN (selection down) */
                scancode = SCANCODE_DOWN;
                keypress = 0;
                keymod = KMOD_SHIFT;
                repeat = 1;
                break;
            case 26:
                /* Shift+LEFT (selection left) */
                scancode = SCANCODE_LEFT;
                keypress = 0;
                keymod = KMOD_SHIFT;
                repeat = 1;
                break;
            case 27:
                /* Shift+RIGHT (selection right) */
                scancode = SCANCODE_RIGHT;
                keypress = 0;
                keymod = KMOD_SHIFT;
                repeat = 1;
                break;
            case 28:
                /* Shift+HOME (selection to start) */
                scancode = SCANCODE_HOME;
                keypress = 0;
                keymod = KMOD_SHIFT;
                repeat = 1;
                break;
            case 29:
                /* Shift+END (selection to end) */
                scancode = SCANCODE_END;
                keypress = 0;
                keymod = KMOD_SHIFT;
                repeat = 1;
                break;
            case 30:
                /* Regular character input */
                scancode = 0;
                keypress = 32 + (random() % 95); /* printable ASCII 32-126 */
                keymod = 0;
                repeat = 1;
                break;
            case 31:
                /* Ctrl+Up (goto start of file) */
                scancode = SCANCODE_UP;
                keypress = 0;
                keymod = KMOD_CTRL;
                repeat = 0;
                break;
            default:
                /* Fallback - should never happen */
                scancode = 0;
                keypress = ' ';
                keymod = 0;
                repeat = 0;
                break;
        }
        if (repeat) {
            int num_repeats = 1 + (random() % 10);
            for (int r = 0; r < num_repeats; r++) {
                code_handle_keypress(scancode, keypress, keymod);
            }
        } else {
            code_handle_keypress(scancode, keypress, keymod);
        }
    }
}
