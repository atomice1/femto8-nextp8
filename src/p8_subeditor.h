/**
 * p8_subeditor.h – Interface shared by all PICO-8 sub-editors.
 *
 * The editor shell (p8_editor.c) owns the main loop, the title bar, and the
 * tab-switching logic.  Each sub-editor registers a p8_subeditor_t struct
 * that provides a draw callback and a keypress handler.
 */
#ifndef P8_SUBEDITOR_H
#define P8_SUBEDITOR_H

#include <stdbool.h>
#include <stdint.h>
#include "p8_dialog.h"

/* ── Tab identifiers ──────────────────────────────────────────────────────── */

typedef enum {
    P8_TAB_CODE   = 0,  /* F1 */
    P8_TAB_SPRITE = 1,  /* F2 */
    P8_TAB_MAP    = 2,  /* F3 */
    P8_TAB_SFX    = 3,  /* F4 */
    P8_TAB_MUSIC  = 4,  /* F5 */
    P8_TAB_COUNT
} p8_tab_t;

/* ── Action codes produced by handle_keypress ─────────────────────────────── */

#define SUBEDITOR_ACTION_NONE         0
#define SUBEDITOR_ACTION_QUIT         1   /* Escape – leave editor (hub → browser) */
#define SUBEDITOR_ACTION_SAVE         2
#define SUBEDITOR_ACTION_SAVE_AS      3
#define SUBEDITOR_ACTION_RUN          4
/* Switch to a different sub-editor tab (F1-F5).
 * Use SUBEDITOR_ACTION_SWITCH_TAB(P8_TAB_SPRITE) etc. */
#define SUBEDITOR_ACTION_SWITCH_TAB(n)     (10 + (n))
#define SUBEDITOR_ACTION_IS_SWITCH_TAB(a)  ((a) >= 10 && (a) < 10 + (int)P8_TAB_COUNT)
#define SUBEDITOR_ACTION_GET_TAB(a)        ((p8_tab_t)((a) - 10))
/* Leave the editor and go directly to the REPL. */
#define SUBEDITOR_ACTION_GOTO_REPL         (10 + P8_TAB_COUNT)

/* ── Sub-editor descriptor ────────────────────────────────────────────────── */

typedef struct {
    /**
     * Short label (≤4 ASCII chars) shown in the tab strip at the top of the
     * screen.  e.g. "COD", "SPR", "MAP", "SFX", "MUS".
     */
    const char *label;

    /**
     * Draw the sub-editor's content area.  Signature matches
     * DIALOG_CUSTOM_CONTROL so it can be used directly as a callback.
     */
    void (*draw)(const p8_dialog_t *dialog, void *user_data,
                 int x, int y, int w, int h);

    /**
     * Handle a raw key event.  Write one of the SUBEDITOR_ACTION_* constants
     * into *action (default SUBEDITOR_ACTION_NONE).
     */
    void (*handle_keypress)(int scancode, int keypress, int keymod,
                            int *action);

    /**
     * Return true if this sub-editor has unsaved changes.  May be NULL (then
     * the shell assumes unmodified).
     */
    bool (*is_modified)(void);

    /**
     * Handle mouse input for this sub-editor.  Called every frame after
     * p8_flip() with updated mouse state.
     * mx, my        current mouse position in screen (overlay) coordinates
     * buttons       current button state: bit0=LMB, bit1=RMB, bit2=MMB
     * prev_buttons  button state from the previous frame
     * action        write a SUBEDITOR_ACTION_* code here if needed
     * May be NULL if this sub-editor does not use mouse input.
     */
    void (*handle_mouse)(int mx, int my, int buttons, int prev_buttons,
                         int *action);
} p8_subeditor_t;

#endif /* P8_SUBEDITOR_H */
