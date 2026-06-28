/**
 * Copyright (C) 2026 Chris January
 *
 * Music editor.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "p8_audio.h"
#include "p8_dialog.h"
#include "p8_editor.h"
#include "p8_editor_music.h"
#include "p8_editor_undo.h"
#include "p8_emu.h"
#include "p8_input.h"
#include "p8_overlay_helper.h"
#include "p8_editor_tab.h"
#include "p8_editor_music.h"

/* =========================================================================
 * CONSTANTS
 * ========================================================================= */

#define MUSIC_COUNT       64    /* number of patterns */
#define MUSIC_ENTRY_BYTES  4    /* bytes per pattern */
#define MUSIC_ROWS        18    /* visible rows in list */

/* Byte offsets within a pattern */
#define MUS_CH0  0              /* bit7=loop_begin, bits0-6=ch0 SFX */
#define MUS_CH1  1              /* bit7=loop_end,   bits0-6=ch1 SFX */
#define MUS_CH2  2              /* bit7=stop,        bits0-6=ch2 SFX */
#define MUS_CH3  3              /* bits0-6=ch3 SFX */

#define SCANCODE_UP       82
#define SCANCODE_DOWN     81
#define SCANCODE_LEFT     80
#define SCANCODE_RIGHT    79
#define SCANCODE_PAGEUP   75
#define SCANCODE_PAGEDOWN 78
#define SCANCODE_R        21
#define SCANCODE_S        22
#define SCANCODE_A         4
#define SCANCODE_Y        28
#define SCANCODE_Z        29
#define SCANCODE_C         6
#define SCANCODE_V        25

/* =========================================================================
 * STATE
 * ========================================================================= */

static int     music_cursor = 0;   /* currently selected pattern (0-63) */
static int     music_scroll = 0;   /* first visible pattern index */
static int     music_col    = 0;   /* column cursor: 0=flags, 1-4=ch0-ch3 */
static int     music_sel_start = -1; /* selection start row, -1=none */
static int     music_sel_end   = -1; /* selection end row (inclusive) */
static int     music_sfx_input        = -1; /* digit accumulator for SFX# entry */
static int     music_sfx_input_digits =  0;
static uint8_t music_copy_buf[MUSIC_ENTRY_BYTES];
static bool    music_copy_valid = false;

static p8_editor_undo_ctx_t *music_undo_ctx = NULL;
static bool music_edit_mode = false; /* when true, up/down edit the selected cell */

/* =========================================================================
 * HELPERS
 * ========================================================================= */

static uint8_t music_byte(int pat, int b)
{
    return m_cart_memory[MEMORY_MUSIC + pat * MUSIC_ENTRY_BYTES + b];
}

static void music_write(int pat, int byte_offset, uint8_t val)
{
    m_cart_memory[MEMORY_MUSIC + pat * MUSIC_ENTRY_BYTES + byte_offset] = val;
}

static void music_ensure_visible(void)
{
    if (music_cursor < music_scroll)
        music_scroll = music_cursor;
    if (music_cursor >= music_scroll + MUSIC_ROWS)
        music_scroll = music_cursor - MUSIC_ROWS + 1;
}

/* =========================================================================
 * DRAW
 * ========================================================================= */

static void music_draw(const p8_dialog_t *dialog, void *user_data,
                       int x, int y, int w, int h)
{
    (void)dialog; (void)user_data;
    overlay_draw_rectfill(x, y, x + w - 1, y + h - 1, 1);

    /* Column header row */
    overlay_draw_rectfill(x, y, x + w - 1, y + GLYPH_HEIGHT - 1, 2);
    overlay_draw_simple_text("## flg c0 c1 c2 c3", x + 1, y, 6);

    for (int i = 0; i < MUSIC_ROWS; i++) {
        int pat = music_scroll + i;
        if (pat >= MUSIC_COUNT) break;

        int ry  = y + GLYPH_HEIGHT + i * GLYPH_HEIGHT;
        bool sel = (pat == music_cursor);
        bool in_sel = false;
        if (music_sel_start >= 0) {
            int lo = music_sel_start < music_sel_end ? music_sel_start : music_sel_end;
            int hi = music_sel_start < music_sel_end ? music_sel_end   : music_sel_start;
            in_sel = (pat >= lo && pat <= hi);
        }
        bool playing = (audio_stat(54) == pat);

        if (sel)
            overlay_draw_rectfill(x, ry, x + w - 1, ry + GLYPH_HEIGHT - 1, 5);
        else if (in_sel)
            overlay_draw_rectfill(x, ry, x + w - 1, ry + GLYPH_HEIGHT - 1, 2);

        int fg = sel ? 7 : in_sel ? 7 : 6;
        if (playing) fg = sel ? 15 : 11;  /* bright/green when this pattern plays */

        uint8_t b0 = music_byte(pat, MUS_CH0);
        uint8_t b1 = music_byte(pat, MUS_CH1);
        uint8_t b2 = music_byte(pat, MUS_CH2);
        uint8_t b3 = music_byte(pat, MUS_CH3);

        /* Flags: L=loop-begin, E=loop-end, S=stop */
        char flags[4] = "   ";
        if (b0 & 0x80) flags[0] = 'L';
        if (b1 & 0x80) flags[1] = 'E';
        if (b2 & 0x80) flags[2] = 'S';

        char row[32];
        snprintf(row, sizeof(row), "%02d %s %02d %02d %02d %02d",
                 pat, flags,
                 b0 & 0x7F, b1 & 0x7F, b2 & 0x7F, b3 & 0x7F);
        overlay_draw_simple_text(row, x + 1, ry, fg);

        /* Coloured L/E/S flags drawn over the flags column */
        if (b0 & 0x80)
            overlay_draw_simple_text("L", x + 1 + 3 * GLYPH_WIDTH, ry, 11); /* green */
        if (b1 & 0x80)
            overlay_draw_simple_text("E", x + 1 + 4 * GLYPH_WIDTH, ry, 8);  /* red */
        if (b2 & 0x80)
            overlay_draw_simple_text("S", x + 1 + 5 * GLYPH_WIDTH, ry, 9);  /* orange */

        /* Playback indicator: ">" at right edge of row when pattern is playing */
        if (playing)
            overlay_draw_simple_text(">", x + w - GLYPH_WIDTH - 1, ry, 11);

        /* Column cursor on selected row */
        if (sel) {
            static const int col_char[5] = { 3, 7, 10, 13, 16 };
            static const int col_len[5]  = { 3, 2,  2,  2,  2 };
            int fc  = x + 1 + col_char[music_col] * GLYPH_WIDTH;
            int fcw = col_len[music_col] * GLYPH_WIDTH;
            int col = music_edit_mode ? 10 : 2;
            overlay_draw_rect(fc - 1, ry - 1, fc + fcw, ry + GLYPH_HEIGHT - 1, col);
        }
    }

    /* Status bar */
    overlay_draw_rectfill(x, y + h - GLYPH_HEIGHT, x + w - 1, y + h - 1, 1);
    char buf[40];
    if (music_col == 0 || music_sfx_input < 0)
        snprintf(buf, sizeof(buf), "pat:%02d", music_cursor);
    else if (music_sfx_input >= 0)
        snprintf(buf, sizeof(buf), "pat:%02d sfx:%d_", music_cursor, music_sfx_input);
    overlay_draw_simple_text(buf, x + 1, y + h - GLYPH_HEIGHT, 6);
}

/* =========================================================================
 * KEY HANDLER
 * ========================================================================= */

/* Commit any pending single-digit SFX input for the current column */
static void music_commit_sfx_input(void)
{
    if (music_sfx_input_digits > 0 && music_col > 0) {
        int sfx_val = music_sfx_input;
        if (sfx_val > 63) sfx_val = 63;
        int bidx = music_col - 1;
        uint8_t b = music_byte(music_cursor, bidx);
        p8_editor_undo_push(music_undo_ctx);
        music_write(music_cursor, bidx, (uint8_t)((b & 0x80) | (sfx_val & 0x3F)));
    }
    music_sfx_input = -1;
    music_sfx_input_digits = 0;
}

static void music_handle_keypress(int scancode, int keypress, int keymod)
{
    if (keymod & KMOD_CTRL) {
        if (scancode == SCANCODE_Z) { if (p8_editor_undo_do_undo(music_undo_ctx)) return; }
        if (scancode == SCANCODE_Y) { if (p8_editor_undo_do_redo(music_undo_ctx)) return; }
        if (scancode == SCANCODE_C) {
            memcpy(music_copy_buf,
                   &m_cart_memory[MEMORY_MUSIC + music_cursor * MUSIC_ENTRY_BYTES],
                   MUSIC_ENTRY_BYTES);
            music_copy_valid = true;
            return;
        }
        if (scancode == SCANCODE_V && music_copy_valid) {
            p8_editor_undo_push(music_undo_ctx);
            memcpy(&m_cart_memory[MEMORY_MUSIC + music_cursor * MUSIC_ENTRY_BYTES],
                   music_copy_buf, MUSIC_ENTRY_BYTES);
            return;
        }
    }

    switch (scancode) {
        case SCANCODE_LEFT:
            music_commit_sfx_input();
            if (music_col > 0) music_col--;
            break;
        case SCANCODE_RIGHT:
            music_commit_sfx_input();
            if (music_col < 4) music_col++;
            break;
        case SCANCODE_UP:
            if (music_edit_mode && music_col > 0) {
                /* increment current cell value */
                int bidx = music_col - 1;
                uint8_t v = music_byte(music_cursor, bidx) & 0x7F;
                if (v < 63) {
                    p8_editor_undo_push(music_undo_ctx);
                    music_write(music_cursor, bidx, (uint8_t)((music_byte(music_cursor,bidx) & 0x80) | (v + 1)));
                }
            } else {
                music_commit_sfx_input();
                if (music_cursor > 0) { music_cursor--; music_ensure_visible(); }
            }
            break;
        case SCANCODE_DOWN:
            if (music_edit_mode && music_col > 0) {
                int bidx = music_col - 1;
                uint8_t v = music_byte(music_cursor, bidx) & 0x7F;
                if (v > 0) {
                    p8_editor_undo_push(music_undo_ctx);
                    music_write(music_cursor, bidx, (uint8_t)((music_byte(music_cursor,bidx) & 0x80) | (v - 1)));
                }
            } else {
                music_commit_sfx_input();
                if (music_cursor < MUSIC_COUNT - 1) { music_cursor++; music_ensure_visible(); }
            }
            break;
        case SCANCODE_PAGEUP:
            music_cursor = music_cursor > MUSIC_ROWS ? music_cursor - MUSIC_ROWS : 0;
            music_ensure_visible();
            break;
        case SCANCODE_PAGEDOWN:
            music_cursor = music_cursor + MUSIC_ROWS < MUSIC_COUNT
                            ? music_cursor + MUSIC_ROWS : MUSIC_COUNT - 1;
            music_ensure_visible();
            break;
        default:
            /* SPACE: toggle play/stop current pattern */
            if (keypress == 32) {
#ifdef ENABLE_AUDIO
                if (audio_stat(54) == music_cursor)
                    audio_music(-1, 0, 0); /* stop */
                else
                    audio_music(music_cursor, 0, 0); /* play */
#endif
            }
            if (music_col == 0) {
                if (keypress == 'l' || keypress == 'L') {
                    p8_editor_undo_push(music_undo_ctx);
                    music_write(music_cursor, MUS_CH0,
                                music_byte(music_cursor, MUS_CH0) ^ 0x80);
                }
                if (keypress == 'e' || keypress == 'E') {
                    p8_editor_undo_push(music_undo_ctx);
                    music_write(music_cursor, MUS_CH1,
                                music_byte(music_cursor, MUS_CH1) ^ 0x80);
                }
                if (keypress == 's' || keypress == 'S') {
                    p8_editor_undo_push(music_undo_ctx);
                    music_write(music_cursor, MUS_CH2,
                                music_byte(music_cursor, MUS_CH2) ^ 0x80);
                }
            } else if (music_col > 0) {
                if (keypress >= '0' && keypress <= '9') {
                    /* Type-in SFX number for the selected channel column */
                    int d = keypress - '0';
                    if (music_sfx_input < 0) {
                        music_sfx_input = d;
                        music_sfx_input_digits = 1;
                    } else {
                        music_sfx_input = music_sfx_input * 10 + d;
                        music_sfx_input_digits++;
                    }
                    if (music_sfx_input_digits >= 2) {
                        /* Commit after 2 digits */
                        music_commit_sfx_input();
                    }
                } else if (keypress == 13) {
                    /* Toggle music cell edit mode */
                    music_edit_mode = !music_edit_mode;
                }
            }
            break;
    }
}

/* =========================================================================
 * MOUSE HANDLER
 * ========================================================================= */

static void music_handle_mouse(int mx, int my, int buttons, int buttonsp, int keymod)
{
    /* Mousewheel scrolls the pattern list. */
    if (m_mouse_wheel != 0) {
        music_scroll -= m_mouse_wheel;
        if (music_scroll < 0) music_scroll = 0;
        if (music_scroll > MUSIC_COUNT - MUSIC_ROWS) music_scroll = MUSIC_COUNT - MUSIC_ROWS;
    }

    int lmb_pressed = (buttonsp & 1);

    /* Content area: x=0, y=GLYPH_HEIGHT, w=P8_WIDTH, h=P8_HEIGHT-GLYPH_HEIGHT */
    int cy    = GLYPH_HEIGHT;  /* = 6 */
    int ybase = cy + GLYPH_HEIGHT;  /* = 12, below header row */

    if (!lmb_pressed)
        return;

    /* Row click: select pattern; SHIFT+click extends selection range */
    if (mx >= 0 && mx < P8_WIDTH &&
        my >= ybase && my < ybase + MUSIC_ROWS * GLYPH_HEIGHT) {
        int row_i       = (my - ybase) / GLYPH_HEIGHT;
        int clicked_pat = music_scroll + row_i;
        if (clicked_pat >= 0 && clicked_pat < MUSIC_COUNT) {
            if (keymod & KMOD_SHIFT) {
                /* Shift-click: extend selection from cursor to clicked row */
                if (music_sel_start < 0) music_sel_start = music_cursor;
                music_sel_end = clicked_pat;
                music_cursor  = clicked_pat;
                music_ensure_visible();
            } else {
                /* Normal click: move cursor, clear selection */
                music_cursor = clicked_pat;
                music_sel_start = -1;
                music_sel_end   = -1;
                music_ensure_visible();
            }

            /* Determine which column was clicked */
            static const int col_char[5] = { 3, 7, 10, 13, 16 };
            static const int col_len[5]  = { 3, 2,  2,  2,  2 };
            for (int c = 4; c >= 0; c--) {
                int fc  = 1 + col_char[c] * GLYPH_WIDTH;
                int fcw = col_len[c] * GLYPH_WIDTH;
                if (mx >= fc - 1 && mx < fc + fcw) {
                    music_col = c;
                    break;
                }
            }

            /* If the click landed on the flags column, toggle the individual
             * flag character (L / E / S) based on x position within it.
             * Flags string starts at char 3: x = 1 + 3*GLYPH_WIDTH = 13.
             *   char 3 → L (loop-begin on ch0)
             *   char 4 → E (loop-end   on ch1)
             *   char 5 → S (stop       on ch2) */
            if (music_col == 0) {
                int flags_x = 1 + 3 * GLYPH_WIDTH;  /* = 13 */
                if (mx >= flags_x && mx < flags_x + 3 * GLYPH_WIDTH) {
                    int sub = (mx - flags_x) / GLYPH_WIDTH;  /* 0, 1, or 2 */
                    p8_editor_undo_push(music_undo_ctx);
                    if (sub == 0)
                        music_write(clicked_pat, MUS_CH0,
                                    music_byte(clicked_pat, MUS_CH0) ^ 0x80);
                    else if (sub == 1)
                        music_write(clicked_pat, MUS_CH1,
                                    music_byte(clicked_pat, MUS_CH1) ^ 0x80);
                    else if (sub == 2)
                        music_write(clicked_pat, MUS_CH2,
                                    music_byte(clicked_pat, MUS_CH2) ^ 0x80);
                }
            }
        }
    }
}

static bool music_handle_buttons(int button_mask, int buttonp_mask)
{
    (void)button_mask;
    if (buttonp_mask & BUTTON_MASK_ESCAPE) {
        if (music_sfx_input >= 0) {
            music_sfx_input = -1;
            music_sfx_input_digits = 0;
            return true;  /* consume Escape; don't switch screens */
        }
    }
    return false;
}

static void music_init(void)
{
    assert(music_undo_ctx == NULL);

    music_cursor = 0;
    music_scroll = 0;
    music_col = 0;
    music_sel_start = -1;
    music_sel_end = -1;
    music_sfx_input = -1;
    music_copy_valid = false;

    music_undo_ctx = p8_editor_undo_create(MEMORY_MUSIC, MEMORY_MUSIC_SIZE);
}

static void music_shutdown(void)
{
    p8_editor_undo_destroy(music_undo_ctx);
    music_undo_ctx = NULL;
}

p8_editor_tab_t p8_subeditor_music = {
    "mus",
    .init=music_init,
    .shutdown=music_shutdown,
    .draw=music_draw,
    .handle_keypress=music_handle_keypress,
    .handle_mouse=music_handle_mouse,
    .handle_buttons=music_handle_buttons,
};
