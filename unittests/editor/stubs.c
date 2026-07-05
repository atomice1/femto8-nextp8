/**
 * test_stubs.c - Stub implementations for external dependencies
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "p8_dialog.h"

/* p8_editor.h */
void p8_editor_mark_modified(void)
{
    /* Stub - no-op for tests */
}

/* p8_emu.h - stubs for functions used by p8_editor_undo.c */
uint8_t *m_cart_memory = NULL;
int m_cart_size = 0;

/* m_lua_script */
char *m_lua_script = NULL;
size_t m_lua_script_size = 0;

/* m_mouse_wheel */
int m_mouse_wheel = 0;

void p8_dialog_init(p8_dialog_t *dlg, const char *title,
                    p8_dialog_control_t *controls, int control_count, int x)
{
    (void)dlg;
    (void)title;
    (void)controls;
    (void)control_count;
    (void)x;
}

p8_dialog_action_t p8_dialog_run(p8_dialog_t *dlg)
{
    (void)dlg;
    return (p8_dialog_action_t){ DIALOG_RESULT_NONE, 0 };
}

void p8_dialog_cleanup(p8_dialog_t *dlg)
{
    (void)dlg;
}

/* p8_overlay_helper.h - stub implementations */
void overlay_clip_get(int *x, int *y, int *w, int *h)
{
    if (x) *x = 0;
    if (y) *y = 0;
    if (w) *w = 240;
    if (h) *h = 136;
}

void overlay_clip_set(int x, int y, int w, int h)
{
    (void)x; (void)y; (void)w; (void)h;
}

void overlay_draw_rectfill(int x, int y, int w, int h, int col)
{
    (void)x; (void)y; (void)w; (void)h; (void)col;
}

void overlay_draw_simple_text(const char *text, int x, int y, int col)
{
    (void)text; (void)x; (void)y; (void)col;
}

int overlay_get_text_width_len(const char *text, int len)
{
    (void)text;
    return len * 4;
}

void draw_line(int x0, int y0, int x1, int y1, int col)
{
    (void)x0; (void)y0; (void)x1; (void)y1; (void)col;
}
