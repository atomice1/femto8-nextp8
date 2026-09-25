/**
 * dialog_stubs.c - Stub implementations for external dependencies
 * Only stubs that are not provided by p8_emu.c, p8_input.c, or p8_dialog.c
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

#include "p8_emu.h"
#include "p8_dialog.h"

/* Include the test header to expose static functions */
#include "p8_dialog_test_priv.h"

/* p8_overlay_helper.h - overlay clip variables (defined here instead of p8_overlay_helper.c) */
int overlay_clip_x0 = 0;
int overlay_clip_y0 = 0;
int overlay_clip_x1 = 128;
int overlay_clip_y1 = 128;

/* p8_emu.h - emu functions that are not in p8_emu.c */
/* Note: p8_flip, p8_update_input, p8_is_quit_requested are in p8_emu.c */

/* p8_overlay_helper.h - overlay functions (static inline, stubbed here) */
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

void overlay_draw_char(int n, int left, int top, int col)
{
    (void)n; (void)left; (void)top; (void)col;
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

/* p8_lua.h - lua functions */
void lua_load_api(void)
{
    /* Stub */
}

void lua_shutdown_api(void)
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
