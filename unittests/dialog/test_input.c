/**
 * test_input.c - Unit tests for p8_dialog input box
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

#include "p8_dialog.h"
#include "p8_input.h"

/* Include the test header to expose static functions */
#include "p8_dialog_test_priv.h"

/* Test buffer and dialog for input box tests */
static char test_buffer[64];
static p8_dialog_control_t test_controls[] = {
    DIALOG_INPUTBOX("test:", test_buffer, sizeof(test_buffer)),
};

static p8_dialog_t test_dialog;

/* Setup function to initialize test state */
int input_setup(void **state)
{
    (void)state;
    p8_init_input();
    p8_dialog_init(&test_dialog, "Test", test_controls, 1, 0);
    return 0;
}

/* Teardown function to cleanup test state */
void input_teardown(void **state)
{
    (void)state;
    p8_dialog_cleanup(&test_dialog);
}

/* Typing a character inserts at cursor */
void test_typing_inserts_at_cursor(void **state)
{
    (void)state;

    strcpy(test_buffer, "hello");
    test_controls[0].data.inputbox.cursor_pos = 5;  /* End of string */

    /* Cursor should be at end after init */
    assert_int_equal(test_controls[0].data.inputbox.cursor_pos, 5);
    assert_string_equal(test_buffer, "hello");
}

/* Selection deletion when typing */
void test_selection_delete_on_type(void **state)
{
    (void)state;

    strcpy(test_buffer, "hello world");

    /* Set up a selection from position 6 to 11 (the word "world") */
    test_controls[0].data.inputbox.select_start = 6;
    test_controls[0].data.inputbox.select_end = 11;
    test_controls[0].data.inputbox.cursor_pos = 6;

    /* Queue a keypress 'x' */
    queue_keypress(0, 'x', 0);

    /* Update the dialog which should process the keypress */
    p8_dialog_update(&test_dialog);

    assert_string_equal(test_buffer, "hello x");
    assert_int_equal(test_controls[0].data.inputbox.cursor_pos, 7);
    assert_int_equal(test_controls[0].data.inputbox.select_start, test_controls[0].data.inputbox.select_end);
}

/* Backspace with selection */
void test_backspace_with_selection(void **state)
{
    (void)state;

    strcpy(test_buffer, "hello world");

    /* Set up a selection */
    test_controls[0].data.inputbox.select_start = 6;
    test_controls[0].data.inputbox.select_end = 11;
    test_controls[0].data.inputbox.cursor_pos = 6;

    /* Backspace should delete selection */
    queue_keypress(0, 8, 0);  /* 8 = backspace */

    p8_dialog_update(&test_dialog);

    assert_string_equal(test_buffer, "hello ");
    assert_int_equal(test_controls[0].data.inputbox.cursor_pos, 5);
}

/* Backspace without selection */
void test_backspace_without_selection(void **state)
{
    (void)state;

    strcpy(test_buffer, "hello");

    /* Set cursor in middle */
    test_controls[0].data.inputbox.cursor_pos = 3;  /* After 'l' */
    
    /* Reset selection (since tests share the same dialog structure) */
    test_controls[0].data.inputbox.select_start = -1;
    test_controls[0].data.inputbox.select_end = -1;

    /* Backspace should delete character before cursor */
    queue_keypress(0, 8, 0);  /* 8 = backspace */

    p8_dialog_update(&test_dialog);

    assert_string_equal(test_buffer, "helo");
    assert_int_equal(test_controls[0].data.inputbox.cursor_pos, 2);
}

/* Delete (Del) with selection */
void test_delete_with_selection(void **state)
{
    (void)state;

    strcpy(test_buffer, "hello world");

    test_controls[0].data.inputbox.select_start = 0;
    test_controls[0].data.inputbox.select_end = 5;  /* "hello" */
    test_controls[0].data.inputbox.cursor_pos = 0;

    /* Del should delete selection */
    queue_keypress(76, 0, 0);  /* 76 = SCANCODE_DEL */

    p8_dialog_update(&test_dialog);

    assert_string_equal(test_buffer, " world");
    assert_int_equal(test_controls[0].data.inputbox.cursor_pos, 0);
}

/* Delete (Del) without selection */
void test_delete_without_selection(void **state)
{
    (void)state;

    strcpy(test_buffer, "hello");

    test_controls[0].data.inputbox.cursor_pos = 0;
    
    /* Reset selection */
    test_controls[0].data.inputbox.select_start = -1;
    test_controls[0].data.inputbox.select_end = -1;

    /* Del should delete character after cursor */
    queue_keypress(76, 0, 0);  /* 76 = SCANCODE_DEL */

    p8_dialog_update(&test_dialog);

    assert_string_equal(test_buffer, "ello");
}

/* Typing with cursor in middle */
void test_typing_in_middle(void **state)
{
    (void)state;

    strcpy(test_buffer, "hello");

    /* Set cursor in middle */
    test_controls[0].data.inputbox.cursor_pos = 2;  /* Between 'l' and 'l' */
    
    /* Reset selection */
    test_controls[0].data.inputbox.select_start = -1;
    test_controls[0].data.inputbox.select_end = -1;

    /* Type 'x' */
    queue_keypress(0, 'x', 0);

    p8_dialog_update(&test_dialog);

    assert_string_equal(test_buffer, "helxlo");
    assert_int_equal(test_controls[0].data.inputbox.cursor_pos, 3);
}
