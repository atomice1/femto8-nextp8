/**
 * emu_stubs.c - Stub implementations for external dependencies
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
#include <errno.h>

#include "p8_dialog.h"
#include "p8_input.h"
#include "p8_emu.h"

/* Note: p8_lua_helper.h functions (static inline) are stubbed here */

/* p8_input.h - overlay clip variables */
int overlay_clip_x0 = 0;
int overlay_clip_y0 = 0;
int overlay_clip_x1 = 128;
int overlay_clip_y1 = 128;

/* p8_input.h - overlay clip functions */
void overlay_clip_get(int *x, int *y, int *w, int *h)
{
    if (x) *x = overlay_clip_x0;
    if (y) *y = overlay_clip_y0;
    if (w) *w = overlay_clip_x1 - overlay_clip_x0;
    if (h) *h = overlay_clip_y1 - overlay_clip_y0;
}

void overlay_clip_set(int x, int y, int w, int h)
{
    overlay_clip_x0 = x;
    overlay_clip_y0 = y;
    overlay_clip_x1 = x + w;
    overlay_clip_y1 = y + h;
}

void overlay_clip_reset(void)
{
    overlay_clip_x0 = 0;
    overlay_clip_y0 = 0;
    overlay_clip_x1 = 128;
    overlay_clip_y1 = 128;
}

void overlay_clip_intersect(int x, int y, int w, int h)
{
    (void)x; (void)y; (void)w; (void)h;
}

/* overlay functions - minimal stubs */
void overlay_draw_char(uint8_t c, int x, int y, int col)
{
    (void)c; (void)x; (void)y; (void)col;
}

void overlay_draw_rect(int x0, int y0, int x1, int y1, int col)
{
    (void)x0; (void)y0; (void)x1; (void)y1; (void)col;
}

void overlay_draw_rectfill(int x0, int y0, int x1, int y1, int col)
{
    (void)x0; (void)y0; (void)x1; (void)y1; (void)col;
}

void overlay_draw_vline(int x, int y0, int y1, int col)
{
    (void)x; (void)y0; (void)y1; (void)col;
}

void overlay_draw_hline(int x0, int x1, int y, int col)
{
    (void)x0; (void)x1; (void)y; (void)col;
}

int overlay_get_text_width(const char *text)
{
    (void)text;
    return 10;
}

int overlay_get_text_width_len(const char *text, size_t len)
{
    (void)text; (void)len;
    return 10;
}

void overlay_draw_simple_text(const char *str, int x, int y, int col)
{
    (void)str; (void)x; (void)y; (void)col;
}

void overlay_draw_simple_text_len(const char *str, size_t len, int x, int y, int col)
{
    (void)str; (void)len; (void)x; (void)y; (void)col;
}

/* p8_main.h - main loop functions */
bool m_dialog_showing = false;

/* p8_lua.h - lua functions */
void lua_load_api(void)
{
    /* Stub */
}

/* p8_audio.h - audio functions */
int audio_init(void)
{
    return 0;
}

void audio_pause(void)
{
    /* Stub */
}

void audio_resume(void)
{
    /* Stub */
}

/* p8_overlay_helper.h - overlay functions */
void overlay_draw_icon(uint8_t *icon, int x, int y)
{
    (void)icon; (void)x; (void)y;
}

/* p8_dialog.h - dialog functions */
void p8_dialog_draw_stack(void)
{
    /* Stub */
}

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

/* p8_editor.h - editor functions */
void p8_editor_invalidate(void)
{
    /* Stub */
}

void p8_editor_code_set_line(int line)
{
    (void)line;
}

/* p8_pause_menu.h - pause menu */
void p8_show_pause_menu(void)
{
    /* Stub */
}

/* p8_lua.h - lua helper functions */
void lua_init(void)
{
    /* Stub */
}

void lua_init_script(void)
{
    /* Stub */
}

bool lua_has_main_loop_callbacks(void)
{
    return false;
}

void lua_update(void)
{
    /* Stub */
}

void lua_draw(void)
{
    /* Stub */
}

const char *lua_get_error(void)
{
    return NULL;
}

int lua_exec_repl(const char *input, const char **err_type, char *err, int err_size)
{
    (void)input;
    (void)err_type;
    (void)err;
    (void)err_size;
    return 0;
}

void lua_shutdown_api(void)
{
    /* Stub */
}

/* p8_audio.h - audio functions */
void audio_stop(void)
{
    /* Stub */
}

void audio_close(void)
{
    /* Stub */
}

/* cache.h - cache functions */
int cache_download_to_dir(const char *cart_id, const char *download_dir,
                          char *cached_filename, size_t cached_filename_size, bool force_download)
{
    (void)cart_id;
    (void)download_dir;
    (void)cached_filename;
    (void)cached_filename_size;
    (void)force_download;
    return -1;
}

/* p8_parser.h - parser functions */
int parse_cart_file(const char *filename, const char *bbs_cart_id, const char *breadcrumb)
{
    (void)filename;
    (void)bbs_cart_id;
    (void)breadcrumb;
    return -1;
}

int parse_cart_ram(uint8_t *buffer, int size, const char *bbs_cart_id, const char *breadcrumb)
{
    (void)buffer;
    (void)size;
    (void)bbs_cart_id;
    (void)breadcrumb;
    return -1;
}

/* femto8.h - version */
const char *femto8_version = "test";

/* p8_input.h */
uint16_t m_buttons[PLAYER_COUNT];
uint16_t m_buttonsp[PLAYER_COUNT];

bool p8_get_next_keypress(unsigned *scancode, uint8_t *keychar, unsigned *mod)
{
    return false;
}

bool p8_get_next_mouse_click(int *x, int *y, int *button, unsigned *mod)
{
    return false;
}

bool p8_has_pending_keypress(void)
{
    return false;
}

bool p8_is_key_down(unsigned scancode)
{
    return false;
}

void p8_init_input(void)
{
    /* Stub */
}

void p8_pump_events(void)
{
    /* Stub */
}

void p8_reset_input(void)
{
    /* Stub */
}

void p8_update_input(void)
{
    /* Stub */
}

/* p8_lua_helper.h stub implementations */

/* Functions for p8_wait_for_any_key */
void cursor_get(int *x, int *y)
{
    if (x) *x = 0;
    if (y) *y = 0;
}

int scroll(int y, int line_height)
{
    (void)y; (void)line_height;
    return 0;
}

void draw_simple_text(const char *str, int x, int y, int col)
{
    (void)str; (void)x; (void)y; (void)col;
}

void cursor_set(int x, int y, int col)
{
    (void)x; (void)y; (void)col;
}

void draw_rectfill(int x0, int y0, int x1, int y1, int col, int fillp)
{
    (void)x0; (void)y0; (void)x1; (void)y1; (void)col; (void)fillp;
}

/* Functions for high_color_resolve */
uint8_t color_get(int type, int index)
{
    (void)type; (void)index;
    return 0;
}

/* Functions for p8_reset */
void pencolor_set(uint8_t col)
{
    (void)col;
}

void reset_color(void)
{
}

void clip_set(int x, int y, int w, int h)
{
    (void)x; (void)y; (void)w; (void)h;
}

/* Functions for p8_reboot */
void clear_screen(int color)
{
    (void)color;
}

/* color_index - defined as static inline in p8_emu.c */
int color_index(uint8_t c)
{
    return ((c >> 3) & 0x10) | (c & 0xf);
}
