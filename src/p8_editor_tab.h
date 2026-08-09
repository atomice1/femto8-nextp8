/**
 * p8_editor_tab.h – Interface shared by all PICO-8 sub-editors.
 *
 * The editor shell (p8_editor.c) owns the main loop, the title bar, and the
 * tab-switching logic.  Each sub-editor registers a p8_editor_tab_t struct
 * that provides a draw callback and a keypress handler.
 */
#ifndef P8_EDITOR_TAB_H
#define P8_EDITOR_TAB_H

#include <stdbool.h>
#include <stdint.h>
#include "p8_dialog.h"

/* ── Action codes produced by handle_keypress ─────────────────────────────── */

#define EDITOR_ACTION_NONE         0
#define EDITOR_ACTION_QUIT         1   /* Switch screens */
#define EDITOR_ACTION_SAVE         2
#define EDITOR_ACTION_SAVE_AS      3
#define EDITOR_ACTION_RUN          4
#define EDITOR_ACTION_NEW          5
/* Switch to a different sub-editor tab (F1-F5).
 * Use EDITOR_ACTION_SWITCH_TAB(P8_TAB_SPRITE) etc. */
#define EDITOR_ACTION_SWITCH_TAB(n)     (10 + (n))
#define EDITOR_ACTION_IS_SWITCH_TAB(a)  ((a) >= 10 && (a) < 10 + (int)P8_TAB_COUNT)
#define EDITOR_ACTION_GET_TAB(a)        ((p8_tab_t)((a) - 10))
/* Leave the editor and go directly to the REPL. */
#define EDITOR_ACTION_GOTO_REPL         (10 + P8_TAB_COUNT)

/* ── Sub-editor descriptor ────────────────────────────────────────────────── */

typedef struct {
    /**
     * Short label (3 ASCII chars) shown in the tab strip at the top of the
     * screen.  e.g. "cod", "spr", "map", "sfx", "mus".
     */
    const char *label;

    /**
     * Initialize any state needed by this sub-editor. Called when the editor is first entered. May be NULL if no initialization is needed.
     */
    void (*init)(void);

    /**
     * Shutdown the sub-editor. Called when the editor is exited or the program exits. May be NULL if no shutdown is needed.
     */
    void (*shutdown)(void);

    /**
     * Show the sub-editor. Called when switching to this sub-editor's tab. May be NULL if no special handling is needed.
     */
    void (*show)(void);

    /**
     * Hide the sub-editor. Called when switching away from this sub-editor's tab. May be NULL if no special handling is needed.
     */
    void (*hide)(void);

    /**
     * Synchronize the sub-editor with the current cart state. Called when the cart is saved or when the sub-editor is shown. May be NULL if no synchronization is needed.
     */
    void (*sync)(void);

    /**
     * Invalidate any cached data from the current cart. Called when a new cart is loaded. May be NULL if no cached data needs invalidating.
     */
    void (*invalidate)(void);

    /**
     * Draw the sub-editor's content area.  Signature matches
     * DIALOG_CUSTOM_CONTROL so it can be used directly as a callback.
     */
    void (*draw)(const p8_dialog_t *dialog, void *user_data,
                 int x, int y, int w, int h);

    /** Update the sub-editor state. Called every frame after draw(). */
    void (*update)(void);

    /**
     * Handle pressed buttons.
     * Return true if the input was handled and should not be processed further, or false to allow default processing.
     */
    bool (*handle_buttons)(int button_mask, int buttonp_mask);

    /**
     * Handle a raw key event.
     */
    void (*handle_keypress)(int scancode, int keypress, int keymod);

    /**
     * Handle mouse movement. Called every frame with mouse position and relative movement.
     * mx, my        current mouse position in screen (overlay) coordinates
     * mx_rel, my_rel  relative movement since last frame
     * buttons       current button state: bit0=LMB, bit1=RMB, bit2=MMB
     * keymod        current key modifiers (KMOD_* bitmask)
     * May be NULL if this sub-editor does not handle mouse movement.
     */
    void (*handle_mouse_move)(int mx, int my, int mx_rel, int my_rel, int buttons, int keymod);

    /**
     * Handle mouse button presses/releases.
     * mx, my        current mouse position in screen (overlay) coordinates
     * buttons       current button state: bit0=LMB, bit1=RMB, bit2=MMB
     * buttonsp      buttons pressed this frame: bit0=LMB, bit1=RMB, bit2=MMB
     * keymod        current key modifiers (KMOD_* bitmask)
     * May be NULL if this sub-editor does not handle mouse buttons.
     */
    void (*handle_mouse_buttons)(int mx, int my, int buttons, int buttonsp, int keymod);

    /**
     * Handle mouse wheel movement.
     * mx, my        current mouse position in screen (overlay) coordinates
     * wheel         wheel delta (positive = up/right, negative = down/left)
     * buttons       current button state: bit0=LMB, bit1=RMB, bit2=MMB
     * keymod        current key modifiers (KMOD_* bitmask)
     * May be NULL if this sub-editor does not handle mouse wheel.
     */
    void (*handle_mouse_wheel)(int mx, int my, int wheel, int buttons, int keymod);
} p8_editor_tab_t;

#endif /* P8_EDITOR_TAB_H */
