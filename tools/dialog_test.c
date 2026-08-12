/**
 * dialog_test.c
 *
 * Interactive test for p8_dialog cursor_pos support
 *
 * This tool creates a dialog with an input box and allows you to test:
 * - Typing characters (insert at cursor position)
 * - Backspace (delete before cursor)
 * - Left/right arrow keys (move cursor)
 * - Return (move to next control)
 *
 * Controls:
 * - Type characters: inserts at cursor position
 * - Backspace: deletes character before cursor
 * - Left/Right arrows: moves cursor
 * - Return/Enter: moves to next control
 * - Escape: cancels dialog
 * - Button 1 (O)/Space: accepts dialog
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "p8_emu.h"
#include "p8_input.h"
#include "p8_dialog.h"
#include "p8_overlay_helper.h"

char *femto8_version = "dialog_test";

/* Dialog state */
static char test_buffer[32] = "hello";

/* Dialog controls */
static p8_dialog_control_t dialog_controls[] = {
    DIALOG_LABEL("Input box test:"),
    DIALOG_INPUTBOX("text:", test_buffer, sizeof(test_buffer)),
    DIALOG_SPACING(),
    DIALOG_BUTTONBAR(),
};

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    p8_dialog_t dialog;
    p8_dialog_action_t result;

    /* Initialize femto8 system (includes SDL) */
    if (p8_init() != 0) {
        fprintf(stderr, "p8_init failed\n");
        return 1;
    }

    /* Initialize dialog */
    p8_dialog_init(&dialog, "Test Dialog", dialog_controls,
                   sizeof(dialog_controls) / sizeof(dialog_controls[0]), 0);

    printf("=== Dialog Test ===\n");
    printf("Buffer: '%s'\n", test_buffer);
    printf("\nControls:\n");
    printf("  - Type characters: inserts at cursor\n");
    printf("  - Backspace: deletes before cursor\n");
    printf("  - Left/Right arrows: moves cursor\n");
    printf("  - Return: moves to next control\n");
    printf("  - Escape: cancels\n");
    printf("  - Button 1 (O)/Space: accepts\n\n");

    result = p8_dialog_run(&dialog);

    if (result.type == DIALOG_RESULT_ACCEPTED) {
        printf("\nDialog accepted!\n");
        printf("Final buffer: '%s'\n", test_buffer);
    } else if (result.type == DIALOG_RESULT_CANCELLED) {
        printf("\nDialog cancelled!\n");
    } else if (result.type == DIALOG_RESULT_BUTTON) {
        printf("\nButton pressed: %d\n", result.action_id);
    }

    /* Cleanup */
    p8_dialog_cleanup(&dialog);
    p8_shutdown();

    return (result.type == DIALOG_RESULT_ACCEPTED) ? 0 : 1;
}
