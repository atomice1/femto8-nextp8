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
    free(clipboard);
    clipboard = strdup("XYZ");
    move_cursor_to(0, 2);
    code_handle_keypress(SCANCODE_V, 0, KMOD_CTRL);

    assert_line_equal(0, "HeXYZllo");

    /* Undo (Ctrl+Z) */
    code_handle_keypress(SCANCODE_Z, 0, KMOD_CTRL);

    assert_line_equal(0, "Hello");

    /* Redo (Ctrl+Y) */
    code_handle_keypress(SCANCODE_Y, 0, KMOD_CTRL);

    assert_line_equal(0, "HeXYZllo");

    free(clipboard);
    clipboard = NULL;
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
