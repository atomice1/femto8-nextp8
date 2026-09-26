/**
 * test_undo.c - Tests for undo/redo functionality via keypresses
 */

#include "p8_editor_code_test_common.h"

/* Test setup/teardown */
static int setup(void **state)
{
    code_init();
    return 0;
}

static int teardown(void **state)
{
    code_shutdown();
    return 0;
}

/* Test: Undo/redo single character insertion */
void test_undo_insert_char(void **state)
{
    split_lines("Hello\n");

    /* Insert 'x' at position 0 */
    move_cursor_to(0, 0);
    code_handle_keypress(0, 'x', 0);

    assert_line_equal(0, "xHello");

    /* Undo (Ctrl+Z) */
    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);

    assert_line_equal(0, "Hello");

    /* Redo (Ctrl+Y) */
    code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);

    assert_line_equal(0, "xHello");
}

/* Test: Undo/redo backspace (delete character before cursor) */
void test_undo_backspace(void **state)
{
    split_lines("Hello\n");

    /* Set cursor at position 2 and backspace */
    move_cursor_to(0, 2);
    code_handle_keypress(0, 8, 0);  /* backspace */

    assert_line_equal(0, "Hllo");

    /* Undo (Ctrl+Z) */
    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);

    assert_line_equal(0, "Hello");

    /* Redo (Ctrl+Y) */
    code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);

    assert_line_equal(0, "Hllo");
}

/* Test: Undo/redo delete character (DEL key) */
void test_undo_delete_char(void **state)
{
    split_lines("Hello\n");

    /* Set cursor at position 1 and press DEL */
    move_cursor_to(0, 1);
    code_handle_keypress(SCANCODE_DEL, 0, 0);

    assert_line_equal(0, "Hllo");

    /* Undo (Ctrl+Z) */
    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);

    assert_line_equal(0, "Hello");

    /* Redo (Ctrl+Y) */
    code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);

    assert_line_equal(0, "Hllo");
}

/* Test: Undo/redo newline insertion */
void test_undo_insert_newline(void **state)
{
    split_lines("Hello\n");

    /* Insert newline at position 2 */
    move_cursor_to(0, 2);
    code_handle_keypress(0, '\n', 0);

    assert_int_equal(line_count, 2);
    assert_line_equal(0, "He");
    assert_line_equal(1, "llo");

    /* Undo (Ctrl+Z) */
    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);

    assert_int_equal(line_count, 1);
    assert_line_equal(0, "Hello");
    /* Redo (Ctrl+Y) */
    code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);

    assert_int_equal(line_count, 2);
    assert_line_equal(0, "He");
    assert_line_equal(1, "llo");
}

/* Test: Undo/redo tab indentation */
void test_undo_tab(void **state)
{
    split_lines("Hello\n");

    /* Press tab */
    move_cursor_to(0, 0);
    code_handle_keypress(SCANCODE_TAB, 0, 0);

    assert_line_equal(0, " Hello");

    /* Undo (Ctrl+Z) */
    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);

    assert_line_equal(0, "Hello");

    /* Redo (Ctrl+Y) */
    code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);

    assert_line_equal(0, " Hello");
}

/* Test: Undo/redo paste (Ctrl+V) */
void test_undo_paste(void **state)
{
    split_lines("Hello\n");

    /* Set clipboard and paste */
    p8_clipboard_set("XYZ");
    move_cursor_to(0, 2);
    code_handle_keypress(SCANCODE_V, 0, KMOD_CTRL);

    assert_line_equal(0, "HeXYZllo");

    /* Undo (Ctrl+Z) */
    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);

    assert_line_equal(0, "Hello");

    /* Redo (Ctrl+Y) */
    code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);

    assert_line_equal(0, "HeXYZllo");
}

/* Test: Undo/redo duplicate line (Ctrl+D) */
void test_undo_duplicate_line(void **state)
{
    split_lines("Hello\n");

    /* Duplicate line */
    move_cursor_to(0, 0);
    code_handle_keypress(SCANCODE_D, 0, KMOD_CTRL);

    assert_int_equal(line_count, 2);
    assert_line_equal(0, "Hello");
    assert_line_equal(1, "Hello");

    /* Undo (Ctrl+Z) */
    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);

    assert_int_equal(line_count, 1);
    assert_line_equal(0, "Hello");

    /* Redo (Ctrl+Y) */
    code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);

    assert_int_equal(line_count, 2);
    assert_line_equal(0, "Hello");
    assert_line_equal(1, "Hello");
}

/* Test: Undo/redo comment/uncomment (Ctrl+B) */
void test_undo_comment(void **state)
{
    split_lines("Hello\n");

    /* Comment line */
    move_cursor_to(0, 0);
    code_handle_keypress(SCANCODE_B, 0, KMOD_CTRL);

    assert_line_equal(0, "--Hello");

    /* Undo (Ctrl+Z) */
    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);

    assert_line_equal(0, "Hello");

    /* Redo (Ctrl+Y) */
    code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);

    assert_line_equal(0, "--Hello");
}

/* Test: Undo with empty stack - should be no-op */
void test_undo_empty_stack(void **state)
{
    split_lines("Hello\n");

    /* No edits made, undo stack should be empty */
    int initial_depth = undo_depth;

    /* Try to undo with empty stack - should not crash */
    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);

    /* Stack should still be empty and text unchanged */
    assert_int_equal(undo_depth, initial_depth);
    assert_line_equal(0, "Hello");
}

/* Test: Two undos with only one edit on stack */
void test_undo_twice_with_one_edit(void **state)
{
    split_lines("Hello\n");

    /* Make one edit */
    move_cursor_to(0, 0);
    code_handle_keypress(0, 'x', 0);
    assert_line_equal(0, "xHello");

    /* First undo - should work */
    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);
    assert_line_equal(0, "Hello");

    /* Second undo - should be no-op (redo stack becomes empty) */
    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);
    assert_line_equal(0, "Hello");
}

/* Test: Nine undos after nine edits (stack limit of 8) */
void test_undo_stack_limit(void **state)
{
    split_lines("Hello\n");

    /* Make 9 edits using digits (0-9) which aren't transformed */
    for (int i = 0; i < 9; i++) {
        move_cursor_to(0, line_lengths[0]);  /* Append at end */
        code_handle_keypress(0, '0' + i, 0);
    }
    assert_line_equal(0, "Hello012345678");

    /* Undo 9 times - only 8 should work (stack limit) */
    for (int i = 0; i < 8; i++) {
        code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);
    }

    /* After 8 undos, first edit (append '0') should NOT be undone */
    assert_line_equal(0, "Hello0");

    /* 9th undo should be no-op */
    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);
    assert_line_equal(0, "Hello0");
}

void test_undo_delete_selected_text(void **state)
{
    split_lines("Line 1\nLine 2 ABC\nLine 3 CDECDE\nLine 4 FGH\nLine 5\nLine 6IJKL\n");

    /* Select text using keypresses */
    select_text(1, 4, 4, 2);

    /* Delete selected text */
    code_handle_keypress(0, 8, 0);

    assert_line_equal(0, "Line 1");
    assert_line_equal(1, "Linene 5");
    assert_line_equal(2, "Line 6IJKL");

    /* Undo (Ctrl+Z) */
    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);

    assert_line_equal(0, "Line 1");
    assert_line_equal(1, "Line 2 ABC");
    assert_line_equal(2, "Line 3 CDECDE");
    assert_line_equal(3, "Line 4 FGH");
    assert_line_equal(4, "Line 5");
    assert_line_equal(5, "Line 6IJKL");

    /* Redo (Ctrl+Y) */
    code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);

    assert_line_equal(0, "Line 1");
    assert_line_equal(1, "Linene 5");
    assert_line_equal(2, "Line 6IJKL");
}

/* Test: Redo with empty stack - should be no-op */
void test_redo_empty_stack(void **state)
{
    split_lines("Hello\n");

    /* No edits made, redo stack should be empty */
    int initial_depth = redo_depth;

    /* Try to redo with empty stack - should not crash */
    code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);

    /* Stack should still be empty and text unchanged */
    assert_int_equal(redo_depth, initial_depth);
    assert_line_equal(0, "Hello");
}

/* Test: Two redos with only one edit on stack */
void test_redo_twice_with_one_edit(void **state)
{
    split_lines("Hello\n");

    /* Make one edit */
    move_cursor_to(0, 0);
    code_handle_keypress(0, 'x', 0);
    assert_line_equal(0, "xHello");

    /* Undo to get to initial state */
    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);
    assert_line_equal(0, "Hello");

    /* First redo - should work */
    code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);
    assert_line_equal(0, "xHello");

    /* Second redo - should be no-op (redo stack is now empty) */
    code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);
    assert_line_equal(0, "xHello");
}

/* Test: Nine redos after nine edits (stack limit of 8) */
void test_redo_stack_limit(void **state)
{
    split_lines("Hello\n");

    /* Make 9 edits using digits (0-9) which aren't transformed */
    for (int i = 0; i < 9; i++) {
        move_cursor_to(0, line_lengths[0]);  /* Append at end */
        code_handle_keypress(0, '0' + i, 0);
    }
    assert_line_equal(0, "Hello012345678");

    /* Undo 8 times (stack limit) */
    for (int i = 0; i < 8; i++) {
        code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);
    }

    /* After 8 undos, state after 1 insert should be restored */
    assert_line_equal(0, "Hello0");

    /* 9th undo should be a no-op */
    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);
    assert_line_equal(0, "Hello0");

    /* Redo 8 times */
    for (int i = 0; i < 8; i++) {
        code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);
    }

    /* After 8 redos, last edit (append '8') should be restored */
    assert_line_equal(0, "Hello012345678");

    /* 9th redo should be a no-op */
    code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);
    assert_line_equal(0, "Hello012345678");
}

void test_undo_with_selection(void **state)
{
    split_lines("abcdefghijklmnopqrstuvwxyz");
    assert_line_equal(0, "abcdefghijklmnopqrstuvwxyz");

    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    assert_int_equal(cursor_col, 3);
    assert_int_equal(select_start_col, select_end_col);

    code_handle_keypress(0, ' ', 0);
    assert_line_equal(0, "abc defghijklmnopqrstuvwxyz");
    assert_int_equal(cursor_col, 4);
    assert_int_equal(select_start_col, select_end_col);

    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    assert_int_equal(cursor_col, 7);
    assert_int_equal(select_start_col, select_end_col);

    code_handle_keypress(SCANCODE_RIGHT, 0, KMOD_SHIFT);
    code_handle_keypress(SCANCODE_RIGHT, 0, KMOD_SHIFT);
    code_handle_keypress(SCANCODE_RIGHT, 0, KMOD_SHIFT);
    assert_int_equal(cursor_col, 10);
    assert_int_equal(select_start_col, 7);
    assert_int_equal(select_end_col, 10);

    code_handle_keypress(0, '*', 0);
    assert_line_equal(0, "abc def*jklmnopqrstuvwxyz");
    assert_int_equal(cursor_col, 8);
    assert_int_equal(select_start_col, select_end_col);

    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    assert_int_equal(cursor_col, 11);
    assert_int_equal(select_start_col, select_end_col);

    code_handle_keypress(0, '_', 0);
    assert_line_equal(0, "abc def*jkl_mnopqrstuvwxyz");
    assert_int_equal(cursor_col, 12);
    assert_int_equal(select_start_col, select_end_col);

    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    assert_int_equal(cursor_col, 15);
    assert_int_equal(select_start_col, select_end_col);

    code_handle_keypress(SCANCODE_RIGHT, 0, KMOD_SHIFT);
    assert_int_equal(cursor_col, 16);
    assert_int_equal(select_start_col, 15);
    assert_int_equal(select_end_col, 16);

    code_handle_keypress(0, '%', 0);
    assert_line_equal(0, "abc def*jkl_mno%qrstuvwxyz");
    assert_int_equal(cursor_col, 16);
    assert_int_equal(select_start_col, select_end_col);

    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    assert_int_equal(cursor_col, 19);
    assert_int_equal(select_start_col, select_end_col);

    code_handle_keypress(0, '!', 0);
    assert_line_equal(0, "abc def*jkl_mno%qrs!tuvwxyz");
    assert_int_equal(cursor_col, 20);
    assert_int_equal(select_start_col, select_end_col);

    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    assert_int_equal(cursor_col, 23);

    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);
    assert_line_equal(0, "abc def*jkl_mno%qrstuvwxyz");
    assert_int_equal(cursor_col, 19);
    assert_int_equal(select_start_col, select_end_col);

    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, KMOD_SHIFT);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    assert_int_equal(cursor_col, 22);

    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);
    assert_line_equal(0, "abc def*jkl_mnopqrstuvwxyz");
    assert_int_equal(cursor_col, 16);
    assert_int_equal(select_start_col, 15);
    assert_int_equal(select_end_col, 16);

    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, KMOD_SHIFT);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    assert_int_equal(cursor_col, 19);

    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);
    assert_line_equal(0, "abc def*jklmnopqrstuvwxyz");
    assert_int_equal(cursor_col, 11);
    assert_int_equal(select_start_col, select_end_col);

    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, KMOD_SHIFT);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    assert_int_equal(cursor_col, 14);

    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);
    assert_line_equal(0, "abc defghijklmnopqrstuvwxyz");
    assert_int_equal(cursor_col, 10);
    assert_int_equal(select_start_col, 7);
    assert_int_equal(select_end_col, 10);

    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, KMOD_SHIFT);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    assert_int_equal(cursor_col, 13);

    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);
    assert_line_equal(0, "abcdefghijklmnopqrstuvwxyz");
    assert_int_equal(cursor_col, 3);
    assert_int_equal(select_start_col, select_end_col);

    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, KMOD_SHIFT);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    assert_int_equal(cursor_col, 6);

    code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);
    assert_line_equal(0, "abc defghijklmnopqrstuvwxyz");
    assert_int_equal(cursor_col, 4);
    assert_int_equal(select_start_col, select_end_col);

    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, KMOD_SHIFT);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    assert_int_equal(cursor_col, 7);

    code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);
    assert_line_equal(0, "abc def*jklmnopqrstuvwxyz");
    assert_int_equal(cursor_col, 8);
    assert_int_equal(select_start_col, select_end_col);

    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, KMOD_SHIFT);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    assert_int_equal(cursor_col, 11);

    code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);
    assert_line_equal(0, "abc def*jkl_mnopqrstuvwxyz");
    assert_int_equal(cursor_col, 12);
    assert_int_equal(select_start_col, select_end_col);

    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, KMOD_SHIFT);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    assert_int_equal(cursor_col, 15);

    code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);
    assert_line_equal(0, "abc def*jkl_mno%qrstuvwxyz");
    assert_int_equal(cursor_col, 16);
    assert_int_equal(select_start_col, select_end_col);

    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, KMOD_SHIFT);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    assert_int_equal(cursor_col, 19);

    code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);
    assert_line_equal(0, "abc def*jkl_mno%qrs!tuvwxyz");
    assert_int_equal(cursor_col, 20);
    assert_int_equal(select_start_col, select_end_col);

    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    code_handle_keypress(SCANCODE_RIGHT, 0, KMOD_SHIFT);
    code_handle_keypress(SCANCODE_RIGHT, 0, 0);
    assert_int_equal(cursor_col, 23);
}

void test_undo_redo_random(void **state)
{
    srand(6942);
    
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
                repeat = 0;
                break;
            case 9:
                /* TAB */
                scancode = SCANCODE_TAB;
                keypress = 0;
                keymod = 0;
                repeat = 0;
                break;
            case 10:
                /* Backspace */
                scancode = 0;
                keypress = 8;
                keymod = 0;
                repeat = 0;
                break;
            case 11:
                /* Newline/Return */
                scancode = 0;
                keypress = '\n';
                keymod = 0;
                repeat = 0;
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
                repeat = 0;
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
        int original_line_count = line_count;
        char *original = NULL;
        if ((i % 1) == 0) {
            join_lines(&original);
            assert(original != NULL);
        }
        if (repeat) {
            int num_repeats = 1 + (random() % 10);
            for (int r = 0; r < num_repeats; r++) {
                code_handle_keypress(scancode, keypress, keymod);
            }
        } else {
            code_handle_keypress(scancode, keypress, keymod);
        }
        if ((i % 1) == 0) {
            int edited_line_count = line_count;
            char *edited = NULL;
            join_lines(&edited);
            assert(edited != NULL);
            if (strcmp(original, edited) != 0) {
                code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);
                int after_undo_line_count = line_count;
                char *after_undo = NULL;
                join_lines(&after_undo);
                fflush(stdout);
                assert_int_equal(original_line_count, after_undo_line_count);
                assert_string_equal(original, after_undo);
                free(after_undo);
                code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);
                int after_redo_line_count = line_count;
                char *after_redo = NULL;
                join_lines(&after_redo);
                fflush(stdout);
                assert_int_equal(edited_line_count, after_redo_line_count);
                assert_string_equal(edited, after_redo);
                free(after_redo);
            }
            free(edited);
            free(original);
        }
    }
}