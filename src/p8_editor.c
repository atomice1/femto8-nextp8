#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "p8_cstore.h"
#include "p8_dialog.h"
#include "p8_editor.h"
#include "p8_editor_code.h"
#include "p8_editor_map.h"
#include "p8_editor_music.h"
#include "p8_editor_sfx.h"
#include "p8_editor_sprite.h"
#include "p8_editor_tab.h"
#include "p8_emu.h"
#include "p8_input.h"
#include "p8_overlay_helper.h"
#include "p8_parser.h"
#include "strtcpy.h"

extern void draw_file_name(const char *str, int x, int y, int col);

static void editor_screen_show();
static void editor_screen_hide();

#define EDITOR_BG_NORMAL      DIALOG_BG_NORMAL
#define EDITOR_TEXT_NORMAL    DIALOG_TEXT_NORMAL

#define SCANCODE_F1       58
#define SCANCODE_F2       59
#define SCANCODE_F3       60
#define SCANCODE_F4       61
#define SCANCODE_F5       62
#define SCANCODE_F6       63
#define SCANCODE_F7       64
/* Letter scancodes for ctrl-key handling */
#define SCANCODE_A         4
#define SCANCODE_R        21
#define SCANCODE_S        22
#define SCANCODE_LEFT     80
#define SCANCODE_RIGHT    79

static p8_tab_t active_tab = P8_TAB_CODE;

/* --- Tab dispatch table ----------------------------------------------------- */

static p8_editor_tab_t * const tab_subeditors[P8_TAB_COUNT] = {
    &p8_subeditor_code,
    &p8_subeditor_sprite,
    &p8_subeditor_map,
    &p8_subeditor_sfx,
    &p8_subeditor_music,
};

/* --- Title bar with tab strip ---------------------------------------------- */

static void draw_top(const p8_dialog_t *dialog, void *user_data, int x, int y, int width, int height)
{
    (void)dialog; (void)user_data; (void)height;
    /* Inverted title bar background */
    overlay_draw_rectfill(x, y, x + width - 1, y + GLYPH_HEIGHT - 1, DIALOG_BG_INVERTED);

    /* File name on the left */
    const char *title;
    if (m_current_cart_file_name[0]) {
        const char *base = strrchr(m_current_cart_file_name, '/');
        title = base ? base + 1 : m_current_cart_file_name;
    } else {
        title = "untitled.p8";
    }
    draw_file_name(title, x + 1, y, DIALOG_TEXT_INVERTED);

    /* Tab labels right-aligned.  Each label is 3 chars wide (12 px) with a
     * 2 px gap between tabs; rightmost tab has no trailing gap. */
    int tab_x = x + width - 1;
    for (int t = P8_TAB_COUNT - 1; t >= 0; t--) {
        const char *lbl = tab_subeditors[t]->label;
        int lw = (int)strlen(lbl) * GLYPH_WIDTH;
        tab_x -= lw;
        bool active = (t == (int)active_tab);
        if (active) {
            overlay_draw_rectfill(tab_x, y, tab_x + lw - 1, y + GLYPH_HEIGHT - 1,
                                  DIALOG_BG_HIGHLIGHT);
            overlay_draw_simple_text(lbl, tab_x, y, DIALOG_TEXT_HIGHLIGHT);
        } else {
            overlay_draw_rectfill(tab_x, y, tab_x + lw - 1, y + GLYPH_HEIGHT - 1,
                                  6);
            overlay_draw_simple_text(lbl, tab_x, y, DIALOG_TEXT_INVERTED);
        }
        if (t > 0)
            tab_x -= 2; /* 2 px gap between tabs */
    }
}

static void draw_subeditor(const p8_dialog_t *dialog, void *user_data,
                           int x, int y, int width, int height)
{
    tab_subeditors[active_tab]->draw(dialog, user_data, x, y, width, height);
}

/* --- Action handling ------------------------------------------------------- */

static void handle_action(p8_dialog_t *dialog, int action)
{
    if (EDITOR_ACTION_IS_SWITCH_TAB(action)) {
        if (tab_subeditors[active_tab]->hide)
            tab_subeditors[active_tab]->hide();
        active_tab = EDITOR_ACTION_GET_TAB(action);
        if (tab_subeditors[active_tab]->show)
            tab_subeditors[active_tab]->show();
        return;
    }
    switch (action) {
        case EDITOR_ACTION_SAVE_AS: {
            p8_dialog_t save_dialog;
            char file_name[128];
            strncpy(file_name, m_current_cart_file_name[0] ? m_current_cart_file_name : "untitled.p8",
                    sizeof(file_name) - 1);
            file_name[127] = '\0';
            p8_dialog_control_t controls[] = {
                DIALOG_INPUTBOX("file name", file_name, 128),
                DIALOG_SPACING(),
                DIALOG_BUTTONBAR(),
            };
            p8_dialog_init(&save_dialog, "save as", controls,
                           sizeof(controls) / sizeof(controls[0]), 130);
            p8_dialog_action_t result = p8_dialog_run(&save_dialog);
            p8_dialog_cleanup(&save_dialog);
            if (result.type == DIALOG_RESULT_ACCEPTED) {
                strtcpy(m_current_cart_file_name, file_name, sizeof(m_current_cart_file_name) - 1);
                m_current_cart_file_name[sizeof(m_current_cart_file_name) - 1] = '\0';
                handle_action(dialog, EDITOR_ACTION_SAVE);
            }
            break;
        }
        case EDITOR_ACTION_SAVE: {
            if (!m_current_cart_file_name[0]) {
                handle_action(dialog, EDITOR_ACTION_SAVE_AS);
                break;
            }
            p8_editor_code_sync(); // sync code editor contents to m_lua_script
            int ret = write_cart_p8(m_current_cart_file_name, m_lua_script, m_cart_memory);
            if (ret != 0) {
                const char *error_lines[] = { "failed to save cart" };
                p8_show_error_dialog(error_lines, 1, P8_ERROR_ERROR);
            }
            break;
        }
        case EDITOR_ACTION_RUN: {
            editor_screen_hide();
            if (p8_run() != 0)
                p8_show_lua_error_dialog();
            editor_screen_show();
            break;
        }
        case EDITOR_ACTION_QUIT: {
            p8_quit();
            break;
        }
        case EDITOR_ACTION_GOTO_REPL:
            /* Handled by main loop via screen switching */
            break;
        default:
            break;
    }
}

static p8_dialog_control_t editor_controls[] = {
    DIALOG_CUSTOM_CONTROL(P8_WIDTH, GLYPH_HEIGHT, draw_top, NULL),
    DIALOG_CUSTOM_CONTROL(P8_WIDTH, P8_HEIGHT - GLYPH_HEIGHT, draw_subeditor, NULL),
};
static p8_dialog_t editor_dialog;

static void editor_screen_init(void)
{
    p8_dialog_init(&editor_dialog, NULL, editor_controls, sizeof(editor_controls) / sizeof(editor_controls[0]), P8_WIDTH);
    editor_dialog.draw_border = false;
    editor_dialog.padding = 0;
}

static void editor_screen_shutdown(void)
{
    p8_dialog_cleanup(&editor_dialog);
}

static void editor_screen_show(void)
{
    p8_dialog_set_showing(&editor_dialog, true);
    if (tab_subeditors[active_tab]->show)
        tab_subeditors[active_tab]->show();
}

static void editor_screen_hide(void)
{
    if (tab_subeditors[active_tab]->hide)
        tab_subeditors[active_tab]->hide();
    p8_dialog_set_showing(&editor_dialog, false);
}

static void editor_screen_draw(int x, int y, int w, int h)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;

    p8_dialog_draw(&editor_dialog);

    /* Draw cursor on top of everything */
    overlay_draw_mouse_cursor(m_mouse_x, m_mouse_y);
}

static void editor_screen_update(void)
{
    if (tab_subeditors[active_tab]->update)
        tab_subeditors[active_tab]->update();

    editor_dialog.cursor_blink++;
}

static void editor_screen_handle_keypress(int scancode, int keypress, int keymod)
{
    int editor_action = EDITOR_ACTION_NONE;

    /* Global editor shortcuts (all tabs) */
    switch (scancode) {
        case SCANCODE_F1: editor_action = EDITOR_ACTION_SWITCH_TAB(P8_TAB_CODE);   break;
        case SCANCODE_F2: editor_action = EDITOR_ACTION_SWITCH_TAB(P8_TAB_SPRITE); break;
        case SCANCODE_F3: editor_action = EDITOR_ACTION_SWITCH_TAB(P8_TAB_MAP);    break;
        case SCANCODE_F4: editor_action = EDITOR_ACTION_SWITCH_TAB(P8_TAB_SFX);    break;
        case SCANCODE_F5: editor_action = EDITOR_ACTION_SWITCH_TAB(P8_TAB_MUSIC);  break;
        case SCANCODE_F6: editor_action = EDITOR_ACTION_QUIT;                       break;
        case SCANCODE_F7: editor_action = EDITOR_ACTION_GOTO_REPL;                  break;
        case SCANCODE_LEFT: if (keymod & KMOD_ALT) editor_action = EDITOR_ACTION_SWITCH_TAB((active_tab == 0) ? P8_TAB_COUNT - 1 : active_tab - 1); break;
        case SCANCODE_RIGHT: if (keymod & KMOD_ALT) editor_action = EDITOR_ACTION_SWITCH_TAB((active_tab == P8_TAB_COUNT - 1) ? 0 : active_tab + 1); break;
        case SCANCODE_R: if (keymod & KMOD_CTRL) editor_action = EDITOR_ACTION_RUN; break;
        case SCANCODE_S: if (keymod & KMOD_CTRL) editor_action = EDITOR_ACTION_SAVE; break;
        //case SCANCODE_A: if (keymod & KMOD_CTRL) editor_action = EDITOR_ACTION_SAVE_AS; break;
        default: break;
    }
    if (editor_action != EDITOR_ACTION_NONE) {
        handle_action(&editor_dialog, editor_action);
        return;
    }

    tab_subeditors[active_tab]->handle_keypress(scancode, keypress, keymod);
}

static void editor_screen_handle_mouse(int mx, int my, int buttons, int buttonsp, int keymod)
{
    /* --- Tab-strip click (top bar, y < GLYPH_HEIGHT) --- */
    int editor_action = EDITOR_ACTION_NONE;
    if ((buttonsp & 1) && my < GLYPH_HEIGHT) {
        int tab_x = P8_WIDTH - 1;
        for (int t = P8_TAB_COUNT - 1; t >= 0; t--) {
            const char *lbl = tab_subeditors[t]->label;
            int lw = (int)strlen(lbl) * GLYPH_WIDTH;
            tab_x -= lw;
            if (mx >= tab_x && mx < tab_x + lw) {
                editor_action = EDITOR_ACTION_SWITCH_TAB((p8_tab_t)t);
                break;
            }
            if (t > 0)
                tab_x -= 2;  /* 2 px gap between tabs */
        }
    }
    if (editor_action != EDITOR_ACTION_NONE) {
        handle_action(&editor_dialog, editor_action);
        return;
    }

    /* --- Sub-editor mouse input --- */
    if (tab_subeditors[active_tab]->handle_mouse)
        tab_subeditors[active_tab]->handle_mouse(mx, my, buttons, buttonsp, keymod);
}

static bool editor_screen_handle_buttons(int button_mask, int buttonp_mask)
{
    bool buttons_handled = false;
    if (tab_subeditors[active_tab]->handle_buttons)
        buttons_handled = tab_subeditors[active_tab]->handle_buttons(button_mask, buttonp_mask);
    return buttons_handled;
}

p8_screen_t p8_screen_edit = {
    .init = editor_screen_init,
    .shutdown = editor_screen_shutdown,
    .show = editor_screen_show,
    .hide = editor_screen_hide,
    .draw = editor_screen_draw,
    .update = editor_screen_update,
    .handle_keypress = editor_screen_handle_keypress,
    .handle_mouse = editor_screen_handle_mouse,
    .handle_buttons = editor_screen_handle_buttons
};
