/**
 * Copyright (C) 2026 Chris January
 */
#ifndef P8_SCREEN_H
#define P8_SCREEN_H

#include <stdbool.h>

typedef struct {
    /**
     * Initialize the screen. Called once when the screen is first shown. May be NULL if no initialization is needed.
     */
    void (*init)(void);

    /**
     * Shutdown the screen. Called once when the screen is hidden or the program exits. May be NULL if no shutdown is needed.
     */
    void (*shutdown)(void);

    /**
     * Show the screen. Called when switching to this screen. May be NULL if no special handling is needed.
     */
    void (*show)(void);

    /**
     * Hide the screen. Called when switching away from this screen. May be NULL if no special handling is needed.
     */
    void (*hide)(void);

    /**
     * Draw the screen. Called every frame.
     */
    void (*draw)(int x, int y, int w, int h);

    /**
     * Update the screen state. Called every frame after draw(). May return a dialog result if this screen is showing a dialog.
     */
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
     * Handle mouse movement.
     */
    void (*handle_mouse_move)(int mx, int my, int mx_rel, int my_rel, int buttons, int keymod);

    /**
     * Handle mouse button presses/releases.
     */
    void (*handle_mouse_buttons)(int mx, int my, int buttons, int buttonsp, int keymod);

    /**
     * Handle mouse wheel movement.
     */
    void (*handle_mouse_wheel)(int mx, int my, int wheel, int buttons, int keymod);
} p8_screen_t;

#endif /* P8_SCREEN_H */
