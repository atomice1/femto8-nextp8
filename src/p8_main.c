/**
 * Copyright (C) 2026 Chris January
 */

#include "p8_browse.h"
#include "p8_editor.h"
#include "p8_emu.h"
#include "p8_input.h"
#include "p8_main.h"

/* =========================================================================
 * SCREEN MANAGEMENT
 * ========================================================================= */

static p8_screen_t *screens[] = {
    &p8_screen_browse,  /* P8_SCREEN_BROWSE */
    &p8_screen_edit,    /* P8_SCREEN_EDIT */
};
#define NUM_SCREENS (sizeof(screens) / sizeof(screens[0]))

static p8_screen_index_t current_screen = P8_SCREEN_BROWSE;
static p8_screen_index_t pending_screen = P8_SCREEN_BROWSE;
static bool screen_switch_requested = false;

void p8_set_active_screen(p8_screen_index_t screen)
{
    pending_screen = screen;
    screen_switch_requested = true;
}

/* =========================================================================
 * MAIN LOOP
 * ========================================================================= */

void p8_main(void)
{
    p8_clear_reboot_requested();
    p8_clear_quit_requested();

    p8_new_cart();

    p8_set_active_screen(P8_SCREEN_BROWSE);

    /* Initialize all screens */
    for (int i = 0; i < NUM_SCREENS; i++) {
        if (screens[i]->init)
            screens[i]->init();
    }

    /* Show initial screen */
    if (screens[current_screen]->show)
        screens[current_screen]->show();

    while (!p8_is_quit_requested()) {
        /* Handle screen switch */
        if (screen_switch_requested) {
            screen_switch_requested = false;
            if (pending_screen != current_screen) {
                /* Hide old screen */
                if (screens[current_screen]->hide)
                    screens[current_screen]->hide();
                
                /* Switch to new screen */
                current_screen = pending_screen;
                
                /* Show new screen */
                if (screens[current_screen]->show)
                    screens[current_screen]->show();
            }
        }

        p8_screen_t *screen = screens[current_screen];

        /* Draw screen */
        if (screen->draw)
            screen->draw(0, 0, P8_WIDTH, P8_HEIGHT);

        /* Flip to display */
        p8_flip();

        /* Update screen */
        if (screen->update)
            screen->update();

        int button_mask = m_buttons[0];
        int buttonp_mask = m_buttonsp[0];

        /* Handle button input */
        bool buttons_handled = false;
        if (screen->handle_buttons) 
            buttons_handled = screen->handle_buttons(button_mask, buttonp_mask);

        /* Escape switches screens: BROWSE -> BROWSE */
        if (!buttons_handled && (buttonp_mask & BUTTON_MASK_ESCAPE)) {
            p8_screen_index_t next_screen;
            switch (current_screen) {
                case P8_SCREEN_BROWSE:
                    next_screen = P8_SCREEN_EDIT;
                    break;
                case P8_SCREEN_EDIT:
                    next_screen = P8_SCREEN_BROWSE;
                    break;
                default:
                    next_screen = P8_SCREEN_BROWSE;
                    break;
            }
            p8_set_active_screen(next_screen);
        }

        /* Handle keypress input */
        if (screen->handle_keypress) {
            unsigned scancode = 0, keymod = 0;
            uint8_t keypress = 0;
            while (p8_get_next_keypress(&scancode, &keypress, &keymod))
                screen->handle_keypress(scancode, keypress, keymod);
        }

        /* Handle mouse input */
        if (screen->handle_mouse)
            screen->handle_mouse(m_mouse_x, m_mouse_y, m_mouse_buttons, m_mouse_buttonsp, m_mouse_keymod);
    }

    /* Shutdown all screens */
    for (int i = 0; i < NUM_SCREENS; i++) {
        if (screens[i]->shutdown)
            screens[i]->shutdown();
    }
}