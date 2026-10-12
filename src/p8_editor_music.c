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
#include "scancodes.h"

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

/* All scancodes now defined in scancodes.h */

/* =========================================================================
 * STATE
 * ========================================================================= */

static int     music_cursor = 0;   /* currently selected pattern (0-63) */
static int     music_scroll = 0;   /* first visible pattern index */
static int     music_col    = 0;   /* column cursor: 0=flags, 1-4=ch0-ch3 */
static int     music_sel_anchor = -1; /* selection anchor row (fixed point) */
static int     music_sel_start = -1; /* selection start row */
static int     music_sel_end   = -1; /* selection end row (exclusive) */
static int     music_sel_col_start = 0; /* selection start column (0-4) */
static int     music_sel_col_end   = 5; /* selection end column (exclusive, 1-5) */
static int     music_sfx_input        = -1; /* digit accumulator for SFX# entry */
static int     music_sfx_input_digits =  0;
static uint8_t music_copy_buf[MUSIC_ENTRY_BYTES * MUSIC_COUNT];
static int     music_copy_len = 0;
static bool    music_copy_valid = false;
static uint8_t music_copy_mask = 0; /* bit0-4 = columns to copy/paste */

static p8_editor_undo_ctx_t *music_undo_ctx = NULL;
static bool music_edit_mode = false; /* when true, up/down edit the selected cell */

/* =========================================================================
 * HELPERS
 * ========================================================================= */

static uint8_t music_byte(int pat, int b)
{
    if (pat < 0 || pat >= MUSIC_COUNT || b < 0 || b >= MUSIC_ENTRY_BYTES)
        return 0;
    return m_cart_memory[MEMORY_MUSIC + pat * MUSIC_ENTRY_BYTES + b];
}

static void music_write(int pat, int byte_offset, uint8_t val)
{
    if (pat < 0 || pat >= MUSIC_COUNT || byte_offset < 0 || byte_offset >= MUSIC_ENTRY_BYTES)
        return;
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
        bool in_sel = (pat >= music_sel_start && pat < music_sel_end);
        bool playing = (audio_stat(54) == pat);

        /* Check if this row and column is in the rectangle selection */
        bool in_rect_sel = false;
        if (in_sel && music_col >= music_sel_col_start && music_col < music_sel_col_end)
            in_rect_sel = true;

        if (sel)
            overlay_draw_rectfill(x, ry, x + w - 1, ry + GLYPH_HEIGHT - 1, 5);
        else if (in_rect_sel)
            overlay_draw_rectfill(x, ry, x + w - 1, ry + GLYPH_HEIGHT - 1, 2);

        int fg = sel ? 7 : in_rect_sel ? 7 : 6;
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

        /* Highlight selected columns in selected rows */
        if (in_sel && music_col >= music_sel_col_start && music_col < music_sel_col_end) {
            static const int col_char[5] = { 3, 7, 10, 13, 16 };
            static const int col_len[5]  = { 3, 2,  2,  2,  2 };
            int fc  = x + 1 + col_char[music_col] * GLYPH_WIDTH;
            int fcw = col_len[music_col] * GLYPH_WIDTH;
            overlay_draw_rect(fc - 1, ry - 1, fc + fcw, ry + GLYPH_HEIGHT - 1, 8);  /* red highlight */
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
            int lo, hi;
            if (music_sel_start != music_sel_end) {
                /* Range copy: copy selected patterns and columns */
                assert(music_sel_start < music_sel_end);
                lo = music_sel_start;
                hi = music_sel_end - 1;  /* inclusive */
                music_copy_len = hi - lo + 1;
                /* Clamp to clipboard array size */
                if (music_copy_len > MUSIC_COUNT) music_copy_len = MUSIC_COUNT;
                if (music_copy_len < 0) music_copy_len = 0;
                /* Build column mask: bit0-4 = flags, ch0, ch1, ch2, ch3 */
                music_copy_mask = 0;
                for (int c = music_sel_col_start; c < music_sel_col_end; c++)
                    music_copy_mask |= (1u << c);
                /* Copy only the selected columns for each pattern */
                for (int i = 0; i < music_copy_len; i++) {
                    int pat = lo + i;
                    uint8_t *dst = &music_copy_buf[i * MUSIC_ENTRY_BYTES];
                    const uint8_t *src = &m_cart_memory[MEMORY_MUSIC + pat * MUSIC_ENTRY_BYTES];
                    /* Copy bytes based on mask */
                    if (music_copy_mask & (1u << 0)) dst[0] = src[0];  /* flags (b0) */
                    if (music_copy_mask & (1u << 1)) dst[1] = src[1];  /* ch0 */
                    if (music_copy_mask & (1u << 2)) dst[2] = src[2];  /* ch1 */
                    if (music_copy_mask & (1u << 3)) dst[3] = src[3];  /* ch2 */
                    if (music_copy_mask & (1u << 4)) dst[4] = src[4];  /* ch3 */
                }
            } else {
                /* Single pattern copy */
                memcpy(music_copy_buf,
                       &m_cart_memory[MEMORY_MUSIC + music_cursor * MUSIC_ENTRY_BYTES],
                       MUSIC_ENTRY_BYTES);
                music_copy_len = 1;
                music_copy_mask = 0x1F;  /* All 5 columns */
                music_copy_valid = true;
            }
            return;
        }
        if (scancode == SCANCODE_V && music_copy_len > 0) {
            p8_editor_undo_push(music_undo_ctx);
            for (int i = 0; i < music_copy_len && music_cursor + i < MUSIC_COUNT; i++) {
                uint8_t *dst = &m_cart_memory[MEMORY_MUSIC + (music_cursor + i) * MUSIC_ENTRY_BYTES];
                const uint8_t *src = &music_copy_buf[i * MUSIC_ENTRY_BYTES];
                /* Paste only the columns that were copied (based on mask) */
                if (music_copy_mask & (1u << 0)) dst[0] = src[0];  /* flags (b0) */
                if (music_copy_mask & (1u << 1)) dst[1] = src[1];  /* ch0 */
                if (music_copy_mask & (1u << 2)) dst[2] = src[2];  /* ch1 */
                if (music_copy_mask & (1u << 3)) dst[3] = src[3];  /* ch2 */
                if (music_copy_mask & (1u << 4)) dst[4] = src[4];  /* ch3 */
            }
            /* Move cursor to end of pasted region */
            int end = music_cursor + music_copy_len;
            if (end > MUSIC_COUNT) end = MUSIC_COUNT;
            music_cursor = end - 1;
            music_sel_anchor = -1;
            music_sel_start = -1;
            music_sel_end   = -1;
            music_ensure_visible();
            return;
        }
    }

    switch (scancode) {
        case SCANCODE_LEFT:
            music_commit_sfx_input();
            if (keymod & KMOD_SHIFT) {
                /* Shift+left: clear selection */
                music_sel_anchor = -1;
                music_sel_start = -1;
                music_sel_end   = -1;
            } else {
                /* Unshifted: move column */
                if (music_col > 0) music_col--;
            }
            break;
        case SCANCODE_RIGHT:
            music_commit_sfx_input();
            if (keymod & KMOD_SHIFT) {
                /* Shift+right: clear selection */
                music_sel_anchor = -1;
                music_sel_start = -1;
                music_sel_end   = -1;
            } else {
                /* Unshifted: move column */
                if (music_col < 4) music_col++;
            }
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
                if (keymod & KMOD_SHIFT) {
                    /* Shift+up: extend selection */
                    if (music_sel_anchor < 0) music_sel_anchor = music_cursor;
                    if (music_cursor > 0) {
                        music_cursor--;
                        /* Selection is [min, max] inclusive, which is [min, max+1) exclusive */
                        int min_row = music_sel_anchor < music_cursor ? music_sel_anchor : music_cursor;
                        int max_row = music_sel_anchor > music_cursor ? music_sel_anchor : music_cursor;
                        music_sel_start = min_row;
                        music_sel_end = max_row + 1;  /* exclusive end */
                        music_ensure_visible();
                    }
                } else {
                    /* Unshifted: clear selection and move cursor */
                    music_sel_anchor = -1;
                    music_sel_start = -1;
                    music_sel_end   = -1;
                    if (music_cursor > 0) { music_cursor--; music_ensure_visible(); }
                }
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
                if (keymod & KMOD_SHIFT) {
                    /* Shift+down: extend selection */
                    if (music_sel_anchor < 0) music_sel_anchor = music_cursor;
                    if (music_cursor < MUSIC_COUNT - 1) {
                        music_cursor++;
                        /* Selection is [min, max] inclusive, which is [min, max+1) exclusive */
                        int min_row = music_sel_anchor < music_cursor ? music_sel_anchor : music_cursor;
                        int max_row = music_sel_anchor > music_cursor ? music_sel_anchor : music_cursor;
                        music_sel_start = min_row;
                        music_sel_end = max_row + 1;  /* exclusive end */
                        music_ensure_visible();
                    }
                } else {
                    /* Unshifted: clear selection and move cursor */
                    music_sel_anchor = -1;
                    music_sel_start = -1;
                    music_sel_end   = -1;
                    if (music_cursor < MUSIC_COUNT - 1) { music_cursor++; music_ensure_visible(); }
                }
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

static void music_handle_mouse_buttons(int mx, int my, int buttons, int buttonsp, int keymod)
{
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
                /* SHIFT+click: set anchor and extend selection */
                music_sel_anchor = music_cursor;
                music_cursor = clicked_pat;
                /* Selection is [min, max] inclusive, which is [min, max+1) exclusive */
                int min_row = music_sel_anchor < music_cursor ? music_sel_anchor : music_cursor;
                int max_row = music_sel_anchor > music_cursor ? music_sel_anchor : music_cursor;
                music_sel_start = min_row;
                music_sel_end = max_row + 1;  /* exclusive end */
                /* Set column selection from current column */
                music_sel_col_start = music_col;
                music_sel_col_end = music_col + 1;
                music_ensure_visible();
            } else {
                /* Normal click: move cursor, clear selection */
                music_sel_anchor = -1;
                music_sel_start = -1;
                music_sel_end   = -1;
                music_sel_col_start = 0;
                music_sel_col_end = 5;
                music_cursor = clicked_pat;
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
                    music_sel_col_start = c;
                    music_sel_col_end = c + 1;
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

static void music_handle_mouse_move(int mx, int my, int mx_rel, int my_rel, int buttons, int keymod)
{
    (void)mx_rel; (void)my_rel; (void)keymod;
    /* Mouse drag selection */
    if (buttons & 1) {
        /* If LMB is held, we're dragging to extend selection */
        int cy = GLYPH_HEIGHT;  /* Title bar height */
        int ybase = cy + GLYPH_HEIGHT;
        int row_i = (my - ybase) / GLYPH_HEIGHT;
        int n = music_scroll + row_i;
        if (n >= 0 && n < MUSIC_COUNT) {
            if (music_sel_anchor < 0) music_sel_anchor = music_cursor;
            int min_row = music_sel_anchor < n ? music_sel_anchor : n;
            int max_row = music_sel_anchor > n ? music_sel_anchor : n;
            music_sel_start = min_row;
            music_sel_end = max_row + 1;  /* exclusive end */
            music_cursor = n;
            music_ensure_visible();
        }
    }
}

static void music_handle_mouse_wheel(int mx, int my, int wheel, int buttons, int keymod)
{
    (void)mx; (void)my; (void)buttons; (void)keymod;
    /* Mousewheel scrolls the pattern list. */
    if (wheel != 0) {
        music_scroll -= wheel;
        if (music_scroll < 0) music_scroll = 0;
        if (music_scroll > MUSIC_COUNT - MUSIC_ROWS) music_scroll = MUSIC_COUNT - MUSIC_ROWS;
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
    music_sel_anchor = -1;
    music_sel_start = -1;
    music_sel_end = -1;
    music_sel_col_start = 0;
    music_sel_col_end = 5;
    music_sfx_input = -1;
    music_copy_len = 0;
    music_copy_valid = false;
    music_copy_mask = 0;

    music_undo_ctx = p8_editor_undo_create(MEMORY_MUSIC, MEMORY_MUSIC_SIZE);
}

static void music_shutdown(void)
{
#ifdef ENABLE_AUDIO
    audio_music(-1, 0, 0);  /* stop music */
#endif
    p8_editor_undo_destroy(music_undo_ctx);
    music_undo_ctx = NULL;
    /* Reset all static state */
    music_cursor = 0;
    music_scroll = 0;
    music_col = 0;
    music_sel_anchor = -1;
    music_sel_start = -1;
    music_sel_end = -1;
    music_sel_col_start = 0;
    music_sel_col_end = 5;
    music_sfx_input = -1;
    music_copy_len = 0;
    music_copy_valid = false;
    music_copy_mask = 0;
    memset(music_copy_buf, 0, sizeof(music_copy_buf));
}

static void music_hide(void)
{
#ifdef ENABLE_AUDIO
    audio_music(-1, 0, 0);  /* stop music */
#endif
}

p8_editor_tab_t p8_subeditor_music = {
    "mus",
    .init=music_init,
    .shutdown=music_shutdown,
    .hide=music_hide,
    .draw=music_draw,
    .handle_keypress=music_handle_keypress,
    .handle_mouse_move=music_handle_mouse_move,
    .handle_mouse_buttons=music_handle_mouse_buttons,
    .handle_mouse_wheel=music_handle_mouse_wheel,
    .handle_buttons=music_handle_buttons,
};
