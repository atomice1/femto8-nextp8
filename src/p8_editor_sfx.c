/**
 * Copyright (C) 2026 Chris January
 *
 * SFX editor.
 *
 *   List view:   64 SFX slots, each showing speed and a dot-preview.
 *   Detail view: three modes cycled by TAB:
 *     0 – pitch bars  (one column per note, height = pitch)
 *     1 – tracker     (one row per note: pitch name, waveform, vol, effect)
 *     2 – scale       (visual piano key toggle + scale presets)
 *   PCM/"wav" editor: mode 3 or shown whenever the SFX is marked as a waveform
 *   instrument (SFX 0..7, instrument bit). When wav is selected, the PCM editor
 *   is shown regardless of the tab-cycled mode and TAB becomes a no-op.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "p8_audio.h"
#include "p8_dialog.h"
#include "p8_editor.h"
#include "p8_editor_sfx.h"
#include "p8_editor_undo.h"
#include "p8_emu.h"
#include "p8_input.h"
#include "p8_overlay_helper.h"
#include "p8_editor_tab.h"

/* --- USB HID scancodes ---------------------------------------------------- */
#define SCANCODE_A         4
#define SCANCODE_C         6
#define SCANCODE_H        11
#define SCANCODE_R        21
#define SCANCODE_S        22
#define SCANCODE_V        25
#define SCANCODE_Y        28
#define SCANCODE_Z        29
#define SCANCODE_TAB      43
#define SCANCODE_UP       82
#define SCANCODE_DOWN     81
#define SCANCODE_LEFT     80
#define SCANCODE_RIGHT    79
#define SCANCODE_PAGEUP   75
#define SCANCODE_PAGEDOWN 78
#define SCANCODE_HOME     74
#define SCANCODE_END      77
#define SCANCODE_DELETE   83

/* --- SFX memory constants ------------------------------------------------- */
#define SFX_COUNT         64
#define SFX_ENTRY_BYTES   68
#define SFX_NOTES         32
/* Within each 68-byte entry: notes at bytes 0-63 (2 bytes/note), header at 64-67 */
#define SFX_HDR_EDMODE    64
#define SFX_HDR_SPEED     65
#define SFX_HDR_LOOP_ST   66
#define SFX_HDR_LOOP_END  67

/* Note bit-field extraction */
#define NOTE_PITCH(w)    ((w) & 0x003F)
#define NOTE_WAVE(w)     (((w) >> 6) & 0x07)
#define NOTE_VOL(w)      (((w) >> 9) & 0x07)
#define NOTE_EFF(w)      (((w) >> 12) & 0x07)
#define NOTE_CUSTOM(w)   (((w) >> 15) & 0x01)  /* 1 = use SFX slot as custom waveform */

/* --- State ----------------------------------------------------------------- */
static int  sfx_cursor       = 0;  /* selected SFX in list view */
static int  sfx_list_scroll  = 0;  /* first visible row in list */
static int  sfx_detail       = -1; /* -1 = list, >=0 = SFX being edited */
static int  sfx_mode         = 0;  /* 0=pitch, 1=tracker, 2=scale, 3=pcm */
static int  sfx_note_cursor  = 0;  /* selected note in tracker mode */
static int  sfx_note_scroll  = 0;  /* first visible note in tracker */
static int  sfx_field_cursor = 0;  /* tracker field: 0=pitch,1=wave,2=vol,3=eff */
static int  sfx_wave_cursor  = 0;  /* selected waveform type in waveform mode */
static int  sfx_eff_cursor   = 0;  /* selected effect type in effect selection */
static bool sfx_hdr_editing  = false; /* true = header field editing mode      */
static int  sfx_hdr_col      = 0;     /* 0=spd, 1=loop_st, 2=loop_end          */
/* Header click-and-drag state */
static int  sfx_hdr_drag_field     = -1;  /* field being dragged, -1=none */
static int  sfx_hdr_drag_start_x   = 0;
static int  sfx_hdr_drag_start_val = 0;
/* Tracker note range selection */
static int  sfx_sel_start   = -1;        /* selection start row, -1=none */
static int  sfx_sel_end     = -1;        /* selection end row (inclusive) */
/* Tracker "all fields" selection (double-click) */
static bool sfx_all_fields  = false;     /* true = all 4 fields of cursor note highlighted */
/* Copy/paste clipboard */
static uint16_t sfx_clipboard[SFX_NOTES];
static int      sfx_clipboard_len = 0;

static p8_editor_undo_ctx_t *sfx_undo_ctx = NULL;
static bool sfx_undo_pushed_this_key = false;
static int  sfx_vol_cursor   = 5;  /* selected volume (0-7) for instrument strip */
static int  sfx_custom       = 0;  /* 1 = custom instrument mode, 0 = built-in */
static int  sfx_drag_loop    = -1; /* -1=none, 0=start,1=end while dragging */

/* Scale snapping (12-bit mask: bit i = semitone i is in scale, C=0 .. B=11) */
/* Default to a 7-note major scale so scale editor shows 7 notes by default. */
static uint16_t sfx_scale_bits = 0x0AB5; /* Maj: 0,2,4,5,7,9,11 */
static const uint16_t sfx_scale_presets[] = {
    0x0FFF, /* Chromatic */

    0x0AB5, /* Ionian              1,2,3,4,5,6,7 */

    0x09AD, /* Harmonic minor      1,2,b3,4,5,b6,7 */
    0x0AAD, /* Melodic minor       1,2,b3,4,5,6,7 */

    0x035D, /* Major Blues         1,2,b3,3,5,6 */
    0x04E9, /* Minor Blues         1,b3,4,b5,5,b7 */

    0x0B6D, /* Diminished          1,2,b3,4,#4,#5,6,7 */

    0x0295, /* Major Pentatonic    1,2,3,5,6 */

    0x0555  /* Whole Tone          1,2,3,b5,b6,b7 */
};
static const char *sfx_scale_names[] = {
    "chrom",

    "maj",
    "hmin",
    "melmin",

    "majblues",
    "blues",

    "dim",

    "pent",

    "whole"
};

static void sfx_undo_maybe_push(void) {
    if (!sfx_undo_pushed_this_key) {
        p8_editor_undo_push(sfx_undo_ctx);
        sfx_undo_pushed_this_key = true;
    }
}

/* Piano keyboard layout: q2w3er5t6y7ui / zsxdcvgbhnjm
 * Lower row (z-row): C3 base (SHIFT: C2); Upper row (q-row): C4 base (SHIFT: C3).
 * Returns pitch 0-63, or -1 if scancode is not a piano key. */
static int piano_note_from_scancode(int sc, bool shift)
{
    /* Lower row: z s x d c v g b h n j m → C..B */
    static const uint8_t low_sc[]  = {29,22,27, 7, 6,25,10, 5,11,17,13,16};
    static const uint8_t low_off[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11};
    /* Upper row: q 2 w 3 e r 5 t 6 y 7 u i → C..C(+1) */
    static const uint8_t hi_sc[]   = {20,31,26,32, 8,21,34,23,35,28,36,24,12};
    static const uint8_t hi_off[]  = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12};
    int base_low = shift ? 24 : 36;  /* SHIFT: C2, normal: C3 */
    int base_hi  = shift ? 36 : 48;  /* SHIFT: C3, normal: C4 */
    for (int i = 0; i < 12; i++)
        if (sc == low_sc[i]) return base_low + low_off[i];
    for (int i = 0; i < 13; i++)
        if (sc == hi_sc[i])  return base_hi  + hi_off[i];
    return -1;
}

/* --- SFX data accessors --------------------------------------------------- */

static uint16_t sfx_note(int sfx_idx, int note_idx)
{
    int base = MEMORY_SFX + sfx_idx * SFX_ENTRY_BYTES + note_idx * 2;
    return (uint16_t)(m_cart_memory[base] | (m_cart_memory[base + 1] << 8));
}

static void sfx_note_write(int sfx_idx, int note_idx, uint16_t val)
{
    int base = MEMORY_SFX + sfx_idx * SFX_ENTRY_BYTES + note_idx * 2;
    m_cart_memory[base]     = (uint8_t)(val & 0xFF);
    m_cart_memory[base + 1] = (uint8_t)(val >> 8);
}

static void sfx_update_current_note(int pitch, int waveform, int volume, int effect, int custom)
{
    if (sfx_detail < 0 || sfx_note_cursor < 0 || sfx_note_cursor >= SFX_NOTES) return;
    int nd = sfx_note(sfx_detail, sfx_note_cursor);
    if (nd == 0) {
        if (pitch == -1) pitch = 0;
        if (waveform == -1) waveform = sfx_wave_cursor;
        if (volume == -1) volume = sfx_vol_cursor;
        if (effect == -1) effect = sfx_eff_cursor;
        if (custom == -1) custom = sfx_custom;
    } else {
        if (pitch == -1) pitch = NOTE_PITCH(nd);
        if (waveform == -1) waveform = NOTE_WAVE(nd);
        if (volume == -1) volume = NOTE_VOL(nd);
        if (effect == -1) effect = NOTE_EFF(nd);
        if (custom == -1) custom = NOTE_CUSTOM(nd);
    }
    uint16_t new_nd = (uint16_t)((pitch & 0x3F) | (waveform << 6) | (volume << 9) | (effect << 12) | (custom << 15));
    sfx_note_write(sfx_detail, sfx_note_cursor, new_nd);
}

static void sfx_set_note_cursor(int n)
{
    if (n < 0 || n >= SFX_NOTES) return;
    sfx_note_cursor = n;
    uint16_t nd = sfx_note(sfx_detail, n);
    if (nd != 0) {
        sfx_wave_cursor = NOTE_WAVE(nd);
        sfx_vol_cursor  = NOTE_VOL(nd);
        sfx_eff_cursor  = NOTE_EFF(nd);
        sfx_custom      = NOTE_CUSTOM(nd) > 0;
    }
}

static uint8_t sfx_speed(int sfx_idx)
{
    return m_cart_memory[MEMORY_SFX + sfx_idx * SFX_ENTRY_BYTES + SFX_HDR_SPEED];
}

static void sfx_hdr_write(int sfx_idx, int byte_offset, uint8_t val)
{
    m_cart_memory[MEMORY_SFX + sfx_idx * SFX_ENTRY_BYTES + byte_offset] = val;
}

/* --- Filter switch helpers (edmode byte encoding from p8-file-format.txt) --
 *   bit 0: tracker mode   bit 1: noiz   bit 2: buzz
 *   bits 3-7: (detune + reverb*3 + dampen*9) << 3   (base-3 packed, 0-26)
 * --------------------------------------------------------------------------- */
static void sfx_get_filters(int sfx, int *noiz, int *buzz,
                             int *detune, int *reverb, int *dampen)
{
    uint8_t edm = m_cart_memory[MEMORY_SFX + sfx * SFX_ENTRY_BYTES + SFX_HDR_EDMODE];
    *noiz   = (edm >> 1) & 1;
    *buzz   = (edm >> 2) & 1;
    *detune = (edm >> 3) % 3;
    *reverb = (edm / 24) % 3;
    *dampen = (edm / 72) % 3;
}

static void sfx_write_filters(int sfx, int noiz, int buzz,
                               int detune, int reverb, int dampen)
{
    uint8_t edm = m_cart_memory[MEMORY_SFX + sfx * SFX_ENTRY_BYTES + SFX_HDR_EDMODE];
    sfx_hdr_write(sfx, SFX_HDR_EDMODE,
                  (uint8_t)((edm & 1)
                            | (noiz   ? 2 : 0)
                            | (buzz   ? 4 : 0)
                            | ((detune + reverb * 3 + dampen * 9) << 3)));
}

/* Custom instrument: bit 7 of SFX_HDR_LOOP_ST enables PCM mode for SFX 0-7 */
static bool sfx_is_instrument(int sfx_idx)
{
    if (sfx_idx < 0 || sfx_idx > 7) return false;
    return (m_cart_memory[MEMORY_SFX + sfx_idx * SFX_ENTRY_BYTES + SFX_HDR_LOOP_ST] & 0x80) != 0;
}

static const char *wave_abbrev[8] = {
    "tri","tlt","saw","sqr","pls","org","nse","pha"
};

static const char *eff_name[8] = {
    "---","sld","vib","drp","fdi","fdo","ARP","arp",
};

/* Forward declarations for shared helpers */
static void draw_title_and_strip(int x, int y, int w, bool tracker_mode);
static bool handle_title_and_strip_mouse(int mx, int my, int x, int y, int w,
                                        int lmb_pressed, int lmb_held,
                                        int rmb_pressed, int keymod);

/* Draw the common title bar + instrument strip. 'tracker_mode' preserves
 * the small visual differences used by tracker vs pitch for the wav button. */
static void draw_title_and_strip(int x, int y, int w, bool tracker_mode)
{
    /* Title background */
    overlay_draw_rectfill(x, y, x + w - 1, y + GLYPH_HEIGHT - 1, 7);

    /* Clickable header fields (light grey) */
    static const int hdr_fx[3] = { 6, 13, 16 };
    static const int hdr_fw[3] = { 3,  2,  2 };
    for (int f = 0; f < 3; f++) {
        int hx = x + 1 + hdr_fx[f] * GLYPH_WIDTH;
        int hw = hdr_fw[f] * GLYPH_WIDTH;
        overlay_draw_rectfill(hx - 1, y, hx + hw, y + GLYPH_HEIGHT - 1, 6);
    }

    uint8_t sp_p  = sfx_speed(sfx_detail);
    uint8_t lps_p = m_cart_memory[MEMORY_SFX + sfx_detail * SFX_ENTRY_BYTES + SFX_HDR_LOOP_ST];
    uint8_t lpe_p = m_cart_memory[MEMORY_SFX + sfx_detail * SFX_ENTRY_BYTES + SFX_HDR_LOOP_END];
    char title[32];
    snprintf(title, sizeof(title), "%02d sp:%3d lp:%2d-%2d", sfx_detail, sp_p, lps_p, lpe_p);
    overlay_draw_simple_text(title, x + 1, y, 1);

    /* wav toggle button (keep previous per-mode colors) */
    if (sfx_detail <= 7) {
        bool ins = sfx_is_instrument(sfx_detail);
        int bx = x + w - 3 * GLYPH_WIDTH - 2;
        overlay_draw_rectfill(bx, y, x + w - 1, y + GLYPH_HEIGHT - 1, ins ? 10 : 6);
        overlay_draw_simple_text("wav", bx + 1, y, 1);
    }

    if (sfx_hdr_editing) {
        int hx = x + 1 + hdr_fx[sfx_hdr_col] * GLYPH_WIDTH;
        int hw = hdr_fw[sfx_hdr_col] * GLYPH_WIDTH;
        overlay_draw_rect(hx - 1, y, hx + hw, y + GLYPH_HEIGHT - 1, 10);
    }

    /* Draw the instrument/effect strip below the title */
    int strip_y = y + GLYPH_HEIGHT;
    int strip_row1 = strip_y;
    int strip_row2 = strip_y + GLYPH_HEIGHT + 1;
    /* overall strip background (two rows) */
    overlay_draw_rectfill(x, strip_row1, x + w - 1, strip_row2 + GLYPH_HEIGHT - 1, 1);

    /* First row: instrument buttons */
    overlay_draw_rectfill(x, strip_row1, x + GLYPH_WIDTH - 1, strip_row1 + GLYPH_HEIGHT - 1, sfx_custom ? 10 : 1);
    overlay_draw_simple_text("c", x, strip_row1, sfx_custom ? 1 : 7);
    for (int wi = 0; wi < 8; wi++) {
        int cx = x + GLYPH_WIDTH + 1 + wi * (GLYPH_WIDTH * 3 + 1);
        int cw = GLYPH_WIDTH * 3;
        int cy = strip_row1;
        bool selected = (wi == sfx_wave_cursor);
        int bg = selected ? 10 : 1;
        int fg = selected ? 1 : 7;
        overlay_draw_vline(cx - 1, cy, cy + GLYPH_HEIGHT - 1, 0);
        overlay_draw_rectfill(cx, cy, cx + cw - 1, cy + GLYPH_HEIGHT - 1, bg);
        if (sfx_custom) {
            char numbuf[2]; snprintf(numbuf, sizeof(numbuf), "%d", wi);
            overlay_draw_simple_text(numbuf, cx + GLYPH_WIDTH, cy, fg);
        } else {
            overlay_draw_simple_text(wave_abbrev[wi], cx, cy, fg);
        }
    }

    /* Second row: effects buttons */
    for (int ei = 0; ei < 8; ei++) {
        int cx = x + GLYPH_WIDTH + 1 + ei * (GLYPH_WIDTH * 3 + 1);
        int cw = GLYPH_WIDTH * 3;
        int cy = strip_row2;
        bool selected = (ei == sfx_eff_cursor);
        int bg = selected ? 10 : 1;
        int fg = selected ? 1 : 7;
        overlay_draw_vline(cx - 1, cy, cy + GLYPH_HEIGHT - 1, 0);
        overlay_draw_rectfill(cx, cy, cx + cw - 1, cy + GLYPH_HEIGHT - 1, bg);
        overlay_draw_simple_text(eff_name[ei], cx, cy, fg);
    }

    /* Volume buttons on second row far right (same tight spacing) */
    int vol_x0 = x + GLYPH_WIDTH + 1 + 8 * (GLYPH_WIDTH * 3 + 1);
    overlay_draw_hline(x, vol_x0 - 1, strip_row2 - 1, 0);
    for (int vi = 0; vi < 8; vi++) {
        int cx = vol_x0 + vi * 2;
        int cy = strip_row2;
        bool selected = (vi == sfx_vol_cursor);
        int bg = selected ? 5 : 1;
        int fg = selected ? 10 : 7;
        int block_h = vi;
        int by = cy + GLYPH_HEIGHT - 1 - block_h;
        overlay_draw_rectfill(cx, cy - 2, cx + 1, cy + GLYPH_HEIGHT - 2, bg);
        if (block_h > 0) overlay_draw_rectfill(cx, by, cx + 1, cy + GLYPH_HEIGHT - 2, fg);
    }
}

/* Combined mouse handler for title header + instrument strip. Calls the
 * header handler (which does not return a consumed flag) then the strip
 * handler which returns true if it handled the click. */
static bool handle_title_and_strip_mouse(int mx, int my, int x, int y, int w,
                                        int lmb_pressed, int lmb_held,
                                        int rmb_pressed, int keymod)
{
    /* header handling */
    static const int hdr_fx[3]      = { 6, 13, 16 };
    static const int hdr_fw[3]      = { 3,  2,  2 };
    static const int hdr_offsets[3] = { SFX_HDR_SPEED, SFX_HDR_LOOP_ST, SFX_HDR_LOOP_END };

    int cy = y;

    /* Clear drag on mouse button release */
    if (!lmb_held) sfx_hdr_drag_field = -1;

    if ((lmb_pressed || rmb_pressed) && my >= cy && my < cy + GLYPH_HEIGHT) {
        int delta = (keymod & KMOD_SHIFT) ? 4 : 1;
        for (int f = 0; f < 3; f++) {
            int hx = 1 + hdr_fx[f] * GLYPH_WIDTH;
            int hw = hdr_fw[f] * GLYPH_WIDTH;
            if (mx >= hx - 1 && mx < hx + hw) {
                int addr = MEMORY_SFX + sfx_detail * SFX_ENTRY_BYTES + hdr_offsets[f];
                uint8_t cur = m_cart_memory[addr];
                int vmax = (f == 0) ? 255 : 31;
                if (lmb_pressed)
                    cur = (uint8_t)((cur + delta < vmax) ? cur + delta : vmax);
                else if (rmb_pressed)
                    cur = (uint8_t)((cur > delta) ? cur - delta : 0);
                sfx_undo_maybe_push();
                m_cart_memory[addr] = cur;
                /* Begin drag from the post-click value */
                if (lmb_pressed) {
                    sfx_hdr_drag_field     = f;
                    sfx_hdr_drag_start_x   = mx;
                    sfx_hdr_drag_start_val = cur;
                }
                break;
            }
        }
    }

    /* Drag: continue adjusting while button is held and mouse moves */
    if (sfx_hdr_drag_field >= 0 && lmb_held && !lmb_pressed) {
        int f    = sfx_hdr_drag_field;
        int dx   = mx - sfx_hdr_drag_start_x;
        int vmax = (f == 0) ? 255 : 31;
        int nv   = sfx_hdr_drag_start_val + dx;
        nv = nv < 0 ? 0 : nv > vmax ? vmax : nv;
        int addr = MEMORY_SFX + sfx_detail * SFX_ENTRY_BYTES + hdr_offsets[f];
        if (m_cart_memory[addr] != (uint8_t)nv) {
            sfx_undo_maybe_push();
            m_cart_memory[addr] = (uint8_t)nv;
        }
    }

    /* strip handling (strip starts one glyphrow below title) */
    int strip_y = y + GLYPH_HEIGHT;
    int strip_row1 = strip_y;
    int strip_row2 = strip_y + GLYPH_HEIGHT + 1;

    if (!lmb_pressed) return false;
    if (my < strip_row1 || my >= strip_row2 + GLYPH_HEIGHT) return false;
    /* Coordinates used for drawing: leftmost small 'c' button, then 8 wide buttons */
    int left_c_x0 = x;
    int left_c_x1 = x + GLYPH_WIDTH; /* exclusive end */
    int base_x = x + GLYPH_WIDTH + 1;
    int cell_w = GLYPH_WIDTH * 3 + 1; /* per-button cell including spacing */

    /* First row: handle 'c' toggle and instrument buttons */
    if (my >= strip_row1 && my < strip_row2) {
        if (mx >= left_c_x0 && mx < left_c_x1) {
            sfx_undo_maybe_push();
            sfx_custom = 1 - sfx_custom; /* toggle custom mode */
            /* Apply to current note */
            sfx_update_current_note(-1, -1, -1, -1, sfx_custom);
            return true;
        }
        if (mx >= base_x && mx < base_x + 8 * cell_w) {
            int wi = (mx - base_x) / cell_w;
            if (wi >= 0 && wi < 8) {
                sfx_undo_maybe_push();
                sfx_wave_cursor = wi;
                /* Apply to current note */
                sfx_update_current_note(-1, wi, -1, -1, -1);
                return true;
            }
        }
    }

    /* Second row: effects buttons */
    if (my >= strip_row2 && my < strip_row2 + GLYPH_HEIGHT) {
        /* effects buttons area */
        if (mx >= base_x && mx < base_x + 8 * cell_w) {
            int ei = (mx - base_x) / cell_w;
            if (ei >= 0 && ei < 8) {
                sfx_undo_maybe_push();
                sfx_eff_cursor = ei;
                /* Apply to current note */
                sfx_update_current_note(-1, -1, -1, ei, -1);
                return true;
            }
        }
    }

    /* Volume buttons */
    if (my >= strip_row2 + GLYPH_HEIGHT - 8 && my < strip_row2 + GLYPH_HEIGHT - 1) {
        int vol_x0 = x + GLYPH_WIDTH + 1 + 8 * (GLYPH_WIDTH * 3 + 1);
        if (mx >= vol_x0) {
            int vi = (mx - vol_x0) / 2;
            if (vi >= 0 && vi < 8) {
                sfx_undo_maybe_push();
                sfx_vol_cursor = vi;
                /* Apply to current note */
                sfx_update_current_note(-1, -1, vi, -1, -1);
                return true;
            }
        }
    }

    return false;
}

/* --- Pixel helper (used by pitch and waveform modes) ---------------------- */
static inline void overlay_put_pixel(int ox, int oy, int col)
{
    if (ox < overlay_clip_x0 || ox >= overlay_clip_x1) return;
    if (oy < overlay_clip_y0 || oy >= overlay_clip_y1) return;
    uint8_t *dest = m_overlay_memory + (ox >> 1) + oy * 64;
    if (ox & 1) *dest = (*dest & 0x0F) | ((col & 0xF) << 4);
    else        *dest = (*dest & 0xF0) |  (col & 0xF);
}

/* --- Pitch name helper ----------------------------------------------------- */
/* Pitch 0 = C3 (130 Hz) in PICO-8 */
static const char *pitch_names[12] = {
    "c-","c#","d-","d#","e-","f-","f#","g-","g#","a-","a#","b-"
};

static void pitch_str(int pitch, char out[4])
{
    /* Shift displayed octave down by 3 so pitch 0 shows as C-0. */
    int oct  = 0 + pitch / 12;
    int semi = pitch % 12;
    out[0] = pitch_names[semi][0];
    out[1] = pitch_names[semi][1];
    out[2] = '0' + (char)oct;
    out[3] = '\0';
}

/* =========================================================================
 * LIST VIEW
 * ========================================================================= */

#define LIST_ROWS 18

static void list_ensure_visible(void)
{
    if (sfx_cursor < sfx_list_scroll)
        sfx_list_scroll = sfx_cursor;
    if (sfx_cursor >= sfx_list_scroll + LIST_ROWS)
        sfx_list_scroll = sfx_cursor - LIST_ROWS + 1;
}

static void draw_list(int x, int y, int w, int h)
{
    overlay_draw_rectfill(x, y, x + w - 1, y + h - 1, 1);

    /* Column header row */
    overlay_draw_rectfill(x, y, x + w - 1, y + GLYPH_HEIGHT - 1, 2);
    overlay_draw_simple_text("## spd", x + 1, y, 6);

    for (int i = 0; i < LIST_ROWS; i++) {
        int idx = sfx_list_scroll + i;
        if (idx >= SFX_COUNT) break;
        int ry = y + GLYPH_HEIGHT + i * GLYPH_HEIGHT;
        bool sel = (idx == sfx_cursor);
        if (sel) overlay_draw_rectfill(x, ry, x + w - 1, ry + GLYPH_HEIGHT - 1, 5);
        int fg = sel ? 7 : 6;

        /* Slot number + speed */
        char buf[12];
        snprintf(buf, sizeof(buf), "%02d", idx);
        overlay_draw_simple_text(buf, x + 1, ry, fg);

        /* Speed */
        uint8_t sp = sfx_speed(idx);
        snprintf(buf, sizeof(buf), "%3d", sp);
        overlay_draw_simple_text(buf, x + GLYPH_WIDTH * 3, ry, fg);

        /* Dot preview: 32 notes, 1 px wide each, single-pixel indicator */
        int dot_x = x + GLYPH_WIDTH * 7;
        for (int n = 0; n < SFX_NOTES && dot_x + n < x + w; n++) {
            uint16_t nd = sfx_note(idx, n);
            int vol = NOTE_VOL(nd);
            if (vol) {
                int py = ry + GLYPH_HEIGHT / 2;
                overlay_put_pixel(dot_x + n, py, sel ? 2 : 11);
            }
        }
    }

    /* Bottom: key hints */
    char buf[32];
    snprintf(buf, sizeof(buf), "sfx:%02d enter:edit", sfx_cursor);
    overlay_draw_rectfill(x, y + h - GLYPH_HEIGHT, x + w - 1, y + h - 1, 1);
    overlay_draw_simple_text(buf, x + 1, y + h - GLYPH_HEIGHT, 6);
}

/* =========================================================================
 * PITCH MODE (detail mode 0) — note pitches shown as vertical bars
 * ========================================================================= */

static void draw_pitch_mode(int x, int y, int w, int h)
{
    overlay_draw_rectfill(x, y, x + w - 1, y + h - 1, 0);

    /* Title + instrument strip */
    draw_title_and_strip(x, y, w, false);
    int strip_h = GLYPH_HEIGHT * 2 + 1;

    int ybase = y + GLYPH_HEIGHT + strip_h;
    int bar_h = h - GLYPH_HEIGHT * 2 - strip_h; /* leave a status line at bottom */
    if (bar_h < 1) bar_h = 1;

    /* Each note: 4px wide bar */
    for (int n = 0; n < SFX_NOTES; n++) {
        uint16_t nd = sfx_note(sfx_detail, n);
        int pitch = NOTE_PITCH(nd);
        int vol   = NOTE_VOL(nd);
        int eff   = NOTE_EFF(nd);
        int bx    = x + n * 4;
        if (bx + 3 >= x + w) break;

        /* Draw a 2px-thick horizontal line at the note pitch position.
           Compute pixel Y for the pitch. */
        int py = ybase + (bar_h - 1) - (pitch * (bar_h - 1) / 63);
        if (py < ybase) py = ybase;
        if (py > ybase + bar_h - 1) py = ybase + bar_h - 1;
        /* Two-pixel thick line */
        int col = (n == sfx_note_cursor) ? 7 : 10;
        if ((vol > 0 || pitch > 0 || eff > 0) || n == sfx_note_cursor) {
            overlay_draw_hline(bx, bx + 3, py, col);
            if (vol > 0 && py - 1 >= ybase) overlay_draw_hline(bx, bx + 3, py - 1, col);
        }
    }

    /* Loop start / end markers */
    {
        uint8_t lps_p = m_cart_memory[MEMORY_SFX + sfx_detail * SFX_ENTRY_BYTES + SFX_HDR_LOOP_ST];
        uint8_t lpe_p = m_cart_memory[MEMORY_SFX + sfx_detail * SFX_ENTRY_BYTES + SFX_HDR_LOOP_END];
        overlay_draw_vline(x + lps_p * 4, ybase, ybase + bar_h, 11); /* green */
        overlay_draw_vline(x + lpe_p * 4 + 3, ybase, ybase + bar_h, 8); /* red */
    }

    /* Pitch axis labels: C0..C5 guide lines + labels at the right edge */
    {
        for (int i = 0; i <= 5; i++) {
            int pitch_c = i * 12; /* C0..C5 */
            int bh = (pitch_c * bar_h + 32) / 64;
            int ly = ybase + bar_h - bh;
            //if (ly < ybase || ly >= ybase + bar_h) continue;
            overlay_draw_hline(x, x + w - 1, ly, 5);  /* dim guide */
            int label_y = ly - GLYPH_HEIGHT;
            if (label_y < ybase) label_y = ybase;
            int lx = x + w - 2 * GLYPH_WIDTH;
            overlay_draw_rectfill(lx, label_y, lx + 2 * GLYPH_WIDTH - 1,
                                  label_y + GLYPH_HEIGHT - 1, 1);
            char lbl[4]; lbl[0] = 'c'; lbl[1] = '0' + i; lbl[2] = '\0';
            overlay_draw_simple_text(lbl, lx, label_y, 7);
        }
    }

    /* Status: note info */
    {
        uint16_t nd = sfx_note(sfx_detail, sfx_note_cursor);
        char pstr[4]; pitch_str(NOTE_PITCH(nd), pstr);
        char buf[24];
        snprintf(buf, sizeof(buf), "%02d:%s %s v%d e%d",
                 sfx_note_cursor, pstr,
                 wave_abbrev[NOTE_WAVE(nd)],
                 NOTE_VOL(nd), NOTE_EFF(nd));
        overlay_draw_rectfill(x, y + h - GLYPH_HEIGHT, x + w - 1, y + h - 1, 1);
        overlay_draw_simple_text(buf, x + 1, y + h - GLYPH_HEIGHT, 7);
    }
}

/* =========================================================================
 * TRACKER MODE (detail mode 1) — note-by-note text list
 * ========================================================================= */

#define TRACKER_ROWS 14

static void tracker_ensure_visible(void)
{
    if (sfx_note_cursor < sfx_note_scroll)
        sfx_note_scroll = sfx_note_cursor;
    if (sfx_note_cursor >= sfx_note_scroll + TRACKER_ROWS)
        sfx_note_scroll = sfx_note_cursor - TRACKER_ROWS + 1;
}

/* Tracker edit-mode flag: when true, up/down modify current field rather
 * than moving the cursor. Toggled by Enter. */
static bool sfx_tracker_edit_mode = false;

static void draw_tracker_mode(int x, int y, int w, int h)
{
    overlay_draw_rectfill(x, y, x + w - 1, y + h - 1, 1);

    /* Title + instrument strip */
    draw_title_and_strip(x, y, w, true);
    int strip_h = GLYPH_HEIGHT * 2 + 1;

    /* Move tracker list down to make room for the instrument strip */
    int ybase = y + GLYPH_HEIGHT + strip_h;

    /* Column header row */
    overlay_draw_rectfill(x, ybase, x + w - 1, ybase + GLYPH_HEIGHT - 1, 2);
    overlay_draw_simple_text("## pit wav v eff", x + 1, ybase, 6);

    for (int i = 0; i < TRACKER_ROWS; i++) {
        int n  = sfx_note_scroll + i;
        if (n >= SFX_NOTES) break;
        int ry = ybase + GLYPH_HEIGHT + i * GLYPH_HEIGHT;
        bool sel = (n == sfx_note_cursor);
        /* Determine if note is within selection range */
        bool in_sel = false;
        if (sfx_sel_start >= 0) {
            int lo = sfx_sel_start < sfx_sel_end ? sfx_sel_start : sfx_sel_end;
            int hi = sfx_sel_start < sfx_sel_end ? sfx_sel_end   : sfx_sel_start;
            in_sel = (n >= lo && n <= hi);
        }
        if (sel)    overlay_draw_rectfill(x, ry, x + w - 1, ry + GLYPH_HEIGHT - 1, 5);
        else if (in_sel) overlay_draw_rectfill(x, ry, x + w - 1, ry + GLYPH_HEIGHT - 1, 2);
        int fg = sel ? 7 : in_sel ? 7 : 6;

        uint16_t nd = sfx_note(sfx_detail, n);
        int vol     = NOTE_VOL(nd);
        int custom  = NOTE_CUSTOM(nd);
        char pstr[4]; pitch_str(NOTE_PITCH(nd), pstr);
        char row[28];
        if (vol && custom)
            snprintf(row, sizeof(row), "%02d %s  c%d %d %s",
                     n, pstr, NOTE_WAVE(nd),
                     vol, eff_name[NOTE_EFF(nd)]);
        else if (vol)
            snprintf(row, sizeof(row), "%02d %s %s %d %s",
                     n, pstr, wave_abbrev[NOTE_WAVE(nd)],
                     vol, eff_name[NOTE_EFF(nd)]);
        else
            snprintf(row, sizeof(row), "%02d --- --- - ---", n);
        overlay_draw_simple_text(row, x + 1, ry, fg);

        /* Highlight active field on the selected row */
        if (sel) {
            /* Column start x-offsets (in character widths × GLYPH_WIDTH):
             * note# at col 0 (2 chars), pitch at col 3 (3 chars),
             * wave at col 7 (3 chars), vol at col 11 (1 char), eff at col 13 */
            static const int field_col[4] = { 3, 7, 11, 13 };
            static const int field_len[4] = { 3, 3,  1,  3 };
            int edit_col = sfx_tracker_edit_mode ? 10 : 2;
            if (sfx_all_fields) {
                /* Double-click mode: highlight all 4 fields */
                for (int f = 0; f < 4; f++) {
                    int fx = x + 1 + field_col[f] * GLYPH_WIDTH;
                    int fw = field_len[f] * GLYPH_WIDTH;
                    overlay_draw_rect(fx - 1, ry - 1, fx + fw, ry + GLYPH_HEIGHT - 1, edit_col);
                }
            } else {
                int fx = x + 1 + field_col[sfx_field_cursor] * GLYPH_WIDTH;
                int fw = field_len[sfx_field_cursor] * GLYPH_WIDTH;
                overlay_draw_rect(fx - 1, ry - 1, fx + fw, ry + GLYPH_HEIGHT - 1, edit_col);
            }
    }
    }

    /* Filter switches row (above hints): NZ BZ D0 R0 P0 */
    {
        int fy = y + h - 2 * GLYPH_HEIGHT;
        overlay_draw_rectfill(x, fy, x + w - 1, fy + GLYPH_HEIGHT - 1, 1);
        int noiz, buzz, detune, reverb, dampen;
        sfx_get_filters(sfx_detail, &noiz, &buzz, &detune, &reverb, &dampen);
        overlay_draw_simple_text("flt:", x + 1, fy, 6);
        int bx = x + 1 + 4 * GLYPH_WIDTH;
        overlay_draw_simple_text("n", bx, fy, noiz ? 11 : 5);
        bx += 3 * GLYPH_WIDTH;
        overlay_draw_simple_text("b", bx, fy, buzz ? 11 : 5);
        bx += 3 * GLYPH_WIDTH;
        char ds[3] = {'d', (char)('0' + detune), '\0'};
        overlay_draw_simple_text(ds, bx, fy, detune == 0 ? 5 : detune == 1 ? 9 : 11);
        bx += 3 * GLYPH_WIDTH;
        char rs[3] = {'r', (char)('0' + reverb), '\0'};
        overlay_draw_simple_text(rs, bx, fy, reverb == 0 ? 5 : reverb == 1 ? 9 : 11);
        bx += 3 * GLYPH_WIDTH;
        char ps[3] = {'p', (char)('0' + dampen), '\0'};
        overlay_draw_simple_text(ps, bx, fy, dampen == 0 ? 5 : dampen == 1 ? 9 : 11);
    }

    /* Key hints at bottom */
    overlay_draw_rectfill(x, y + h - GLYPH_HEIGHT, x + w - 1, y + h - 1, 1);
    overlay_draw_simple_text("tab:mode esc:list", x + 1, y + h - GLYPH_HEIGHT, 6);
}

/* =========================================================================
 * PCM INSTRUMENT MODE (detail mode 2, SFX 0-7 with instrument bit set)
 * 64 signed 8-bit samples at 5512.5 Hz
 * ========================================================================= */

static void draw_pcm_mode(int x, int y, int w, int h)
{
    overlay_draw_rectfill(x, y, x + w - 1, y + h - 1, 1);

    /* Title bar */
    char title[24];
    snprintf(title, sizeof(title), "sfx:%02d pcm", sfx_detail);
    overlay_draw_rectfill(x, y, x + w - 1, y + GLYPH_HEIGHT - 1, 7);
    overlay_draw_simple_text(title, x + 1, y, 1);
    {   /* "wav" button always lit (we ARE in waveform/PCM mode) */
        int bx = x + w - 3 * GLYPH_WIDTH - 2;
        overlay_draw_rectfill(bx, y, x + w - 1, y + GLYPH_HEIGHT - 1, 10);
        overlay_draw_simple_text("wav", bx + 1, y, 0);
    }

    /* PCM drawing area: occupy whole vertical space between title and status bar */
    int area_y0 = y + GLYPH_HEIGHT + 1;
    int area_y1 = y + h - GLYPH_HEIGHT - 1;
    int area_h = area_y1 - area_y0 + 1;
    if (area_h < 4) area_h = 4;

    /* Sample width: fit 64 samples across available width, at least 1px each */
    int sample_w = w / 64; if (sample_w < 1) sample_w = 1;
    int total_w = sample_w * 64;
    int left = x + (w - total_w) / 2; /* horizontally centered */

    /* Vertical centering: full area used, center line halfway through area */
    int py0 = area_y0;
    int pcm_h = area_h;
    int mid_y = py0 + pcm_h / 2;

    /* Zero line */
    overlay_draw_hline(left, left + total_w - 1, mid_y, 5);

    /* Draw all 64 PCM samples as sample_w-wide vertical bars */
    for (int i = 0; i < 64; i++) {
        int8_t sample = (int8_t)m_cart_memory[MEMORY_SFX + sfx_detail * SFX_ENTRY_BYTES + i];
        /* Map sample to Y: sample * pcm_h / 256, centered at mid_y */
        int bar_y = mid_y - (sample * pcm_h) / 256;
        if (bar_y < py0)          bar_y = py0;
        if (bar_y >= py0 + pcm_h) bar_y = py0 + pcm_h - 1;
        int bar_top_y, bar_bot_y;
        if (bar_y <= mid_y) { bar_top_y = bar_y; bar_bot_y = mid_y; }
        else                { bar_top_y = mid_y; bar_bot_y = bar_y; }
        int cx = left + i * sample_w;
        for (int sx = 0; sx < sample_w; sx++)
            overlay_draw_vline(cx + sx, bar_top_y, bar_bot_y, 10);
    }

    /* Status bar */
    overlay_draw_rectfill(x, y + h - GLYPH_HEIGHT, x + w - 1, y + h - 1, 1);
    overlay_draw_simple_text("drag:draw  tab:mode", x + 1, y + h - GLYPH_HEIGHT, 6);
}

/* =========================================================================
 * WAVEFORM MODE (detail mode 2) — show built-in waveform shapes
 * ========================================================================= */

/* built-in wave shapes removed: waveform selector replaced by PCM editor */

/* draw_waveform_mode removed — waveform selector replaced by PCM editor */

/* =========================================================================
 * SCALE EDITOR (detail mode 3) — visual piano key toggle + scale presets
 * ========================================================================= */

static const char * const note_name_short[12] = {
    "c","c#","d","d#","e","f","f#","g","g#","a","a#","b"
};

static void draw_scale_mode(int x, int y, int w, int h)
{
    overlay_draw_rectfill(x, y, x + w - 1, y + h - 1, 0);

    /* Title bar */
    overlay_draw_rectfill(x, y, x + w - 1, y + GLYPH_HEIGHT - 1, 7);
    overlay_draw_simple_text("scale", x + 1, y, 1);

    /* Piano keyboard: 12 cells of 10px, starting at x+4 (4px left margin) */
    int key_x0 = x + 4;
    int key_y0 = y + GLYPH_HEIGHT + 2;
    int key_h  = 20;
    int key_w  = 10;
    for (int semi = 0; semi < 12; semi++) {
        bool active  = (sfx_scale_bits >> semi) & 1;
        bool is_black = (semi == 1 || semi == 3 || semi == 6 || semi == 8 || semi == 10);
        int  kx = key_x0 + semi * key_w;
        int  bg = active ? (is_black ? 3 : 11) : (is_black ? 1 : 6);
        int  fg = active ? 7 : (is_black ? 5 : 1);
        overlay_draw_rectfill(kx, key_y0, kx + key_w - 2, key_y0 + key_h - 1, bg);
        overlay_draw_rect(kx, key_y0, kx + key_w - 2, key_y0 + key_h - 1,
                          active ? 10 : 5);
        /* Note name inside key */
        overlay_draw_simple_text(note_name_short[semi], kx + 1,
                                 key_y0 + key_h - GLYPH_HEIGHT - 1, fg);
    }

    /* Active note count + scale name/tonic detection */
    char buf[33];
    buf[0] = '\0';
    {
        int cnt = 0;
        for (int i = 0; i < 12; i++) if ((sfx_scale_bits >> i) & 1) cnt++;
        /* Determine if current scale matches a rotated preset */
        for (int b = 0; b < sizeof(sfx_scale_presets) / sizeof(sfx_scale_presets[0]); b++) {
            uint16_t base = sfx_scale_presets[b] & 0x0FFF;
            for (int r = 0; r < 12; r++) {
                /* rotate base left by r */
                uint16_t rot = (uint16_t)(((base << r) | (base >> (12 - r))) & 0x0FFF);
                if (rot == sfx_scale_bits) {
                    char tmp[32];
                    snprintf(tmp, sizeof(tmp), "%s %s", note_name_short[r], sfx_scale_names[b]);
                    if (strlen(buf) + 1 + strlen(tmp) < sizeof(buf)-1) {
                        if (buf[0] != '\0') strcat(buf, "/");
                        strcat(buf, tmp);
                    }
                    if (b == 0) break;
                }
            }
        }
        if (buf[0] == '\0')
            snprintf(buf, sizeof(buf), "custom - %d notes", cnt);
        overlay_draw_simple_text(buf, x + 1, key_y0 + key_h + 3, 6);
    }

    /* Button row below keyboard: [<] [>] [inv] */
    int btn_y = key_y0 + key_h + 12;
    static const struct { const char *label; int bx; int bw; } btns[6] = {
        { "<",   2,  10 },
        { ">",  13,  10 },
        { "inv", 30, 18 },
    };
    for (int b = 0; b < 3; b++) {
        int bx = x + btns[b].bx;
        overlay_draw_rectfill(bx, btn_y, bx + btns[b].bw - 1, btn_y + GLYPH_HEIGHT - 1, 1);
        overlay_draw_simple_text(btns[b].label, bx + 1, btn_y, 7);
    }

    /* Rows of preset name buttons underneath the control row */
    int names_y = btn_y + GLYPH_HEIGHT + 6;
    int cols = 3;
    int total = (int)(sizeof(sfx_scale_names) / sizeof(sfx_scale_names[0]));
    int btn_w = 36;
    int gap = 2;
    int start_x = x + 2;
    for (int i = 0; i < total; i++) {
        int col = i % cols;
        int row = i / cols;
        int bx = start_x + col * (btn_w + gap);
        int by = names_y + row * (GLYPH_HEIGHT + 4);
        overlay_draw_rectfill(bx, by, bx + btn_w - 1, by + GLYPH_HEIGHT - 1, 1);
        overlay_draw_simple_text(sfx_scale_names[i], bx + 2, by, 7);
    }

    /* Status bar */
    overlay_draw_rectfill(x, y + h - GLYPH_HEIGHT, x + w - 1, y + h - 1, 1);
    overlay_draw_simple_text("click:toggle esc:back", x + 1, y + h - GLYPH_HEIGHT, 6);
}

/* =========================================================================
 * SUBEDITOR CALLBACKS
 * ========================================================================= */

static void sfx_draw(const p8_dialog_t *dialog, void *user_data,
                     int x, int y, int w, int h)
{
    (void)dialog; (void)user_data;
    if (sfx_detail < 0)
        draw_list(x, y, w, h);
    else if (sfx_is_instrument(sfx_detail))
        draw_pcm_mode(x, y, w, h);
    else if (sfx_mode == 0)
        draw_pitch_mode(x, y, w, h);
    else if (sfx_mode == 1)
        draw_tracker_mode(x, y, w, h);
    else if (sfx_mode == 2)
        draw_scale_mode(x, y, w, h);
    else
        draw_pitch_mode(x, y, w, h); /* fallback to pitch if unknown */
}

/* Snap a pitch (0-63) to the nearest note in sfx_scale_bits.
 * The 12-bit mask has bit i = 1 if semitone i is in the scale (C=0 .. B=11). */
static int snap_to_scale(int pitch)
{
    if (!sfx_scale_bits) return pitch;  /* empty scale: no snapping */
    int octave = pitch / 12;
    int semi   = pitch % 12;
    int best_note = -1, best_dist = 13;
    for (int i = 0; i < 12; i++) {
        if (!((sfx_scale_bits >> i) & 1)) continue;
        int dist = semi - i;
        if (dist >  6) dist -= 12;  /* prefer next-octave note */
        if (dist < -6) dist += 12;  /* prefer prev-octave note */
        if (abs(dist) < abs(best_dist)) { best_dist = dist; best_note = i; }
    }
    if (best_note < 0) return pitch;
    int result = octave * 12 + best_note;
    /* Adjust octave if wrap-around occurred */
    if (best_dist < 0 && best_note < semi) result += 12;  /* note is in next octave */
    if (best_dist > 0 && best_note > semi) result -= 12;  /* note is in prev octave */
    result = result < 0 ? 0 : result > 63 ? 63 : result;
    return result;
}

static void sfx_handle_keypress(int scancode, int keypress, int keymod)
{
    (void)keypress;
    sfx_undo_pushed_this_key = false;
    if (keymod & KMOD_CTRL) {
        if (scancode == SCANCODE_Z) { if (p8_editor_undo_do_undo(sfx_undo_ctx)) return; }
        if (scancode == SCANCODE_Y) { if (p8_editor_undo_do_redo(sfx_undo_ctx)) return; }
        /* CTRL-C: copy selected notes (or single note if no selection) */
        if (scancode == SCANCODE_C && sfx_detail >= 0 && sfx_mode == 1) {
            int lo = sfx_note_cursor, hi = sfx_note_cursor;
            if (sfx_sel_start >= 0) {
                lo = sfx_sel_start < sfx_sel_end ? sfx_sel_start : sfx_sel_end;
                hi = sfx_sel_start < sfx_sel_end ? sfx_sel_end   : sfx_sel_start;
            }
            sfx_clipboard_len = hi - lo + 1;
            for (int i = 0; i < sfx_clipboard_len; i++)
                sfx_clipboard[i] = sfx_note(sfx_detail, lo + i);
            return;
        }
        /* CTRL-V: paste clipboard at cursor */
        if (scancode == SCANCODE_V && sfx_detail >= 0 && sfx_mode == 1 && sfx_clipboard_len > 0) {
            sfx_undo_maybe_push();
            for (int i = 0; i < sfx_clipboard_len && sfx_note_cursor + i < SFX_NOTES; i++)
                sfx_note_write(sfx_detail, sfx_note_cursor + i, sfx_clipboard[i]);
            /* Move cursor to end of pasted region */
            int end = sfx_note_cursor + sfx_clipboard_len;
            if (end > SFX_NOTES) end = SFX_NOTES;
            sfx_set_note_cursor(end - 1);
            sfx_sel_start = -1; sfx_sel_end = -1;
            tracker_ensure_visible();
            return;
        }
    }

    if (sfx_detail < 0) {
        /* --- List view navigation --- */
        switch (scancode) {
            case SCANCODE_UP:
                if (sfx_cursor > 0) sfx_cursor--;
                list_ensure_visible();
                break;
            case SCANCODE_DOWN:
                if (sfx_cursor < SFX_COUNT - 1) sfx_cursor++;
                list_ensure_visible();
                break;
            case SCANCODE_PAGEUP:
                sfx_cursor = sfx_cursor > LIST_ROWS ? sfx_cursor - LIST_ROWS : 0;
                list_ensure_visible();
                break;
            case SCANCODE_PAGEDOWN:
                sfx_cursor = sfx_cursor + LIST_ROWS < SFX_COUNT
                             ? sfx_cursor + LIST_ROWS : SFX_COUNT - 1;
                list_ensure_visible();
                break;
            default:
                if (keypress == 13) { /* Enter: open detail */
                    sfx_detail = sfx_cursor;
                    sfx_mode   = 0;
                    sfx_note_scroll = 0;
                    sfx_set_note_cursor(0);
                }
                break;
        }
    } else {
        /* --- Detail view navigation --- */
        /* SFX navigation: '-' = prev SFX, '=' = next SFX */
        if (keypress == '-' && sfx_detail > 0) {
            sfx_detail--;
            sfx_note_scroll = 0;
            sfx_set_note_cursor(0);
        }
        if (keypress == '=' && sfx_detail < SFX_COUNT - 1) {
            sfx_detail++;
            sfx_note_scroll = 0;
            sfx_set_note_cursor(0);
        }
        /* Backspace in tracker mode: clear current note */
        if (keypress == 8 && sfx_mode == 1) {
            sfx_undo_maybe_push();
            sfx_note_write(sfx_detail, sfx_note_cursor, 0);
        }
        /* A: release looping sample (stop sfx on all channels) */
        if (scancode == SCANCODE_A && !(keymod & KMOD_CTRL)) {
#ifdef ENABLE_AUDIO
            audio_sound(sfx_detail, -2, 0, 0);
#endif
        }
        /* SPACE: toggle play/stop current SFX
         * SHIFT+SPACE: play from cursor note position (not note 0) */
        if (keypress == 32) {
#ifdef ENABLE_AUDIO
            bool playing = false;
            for (int ch = 0; ch < 4; ch++) {
                if (audio_stat(16 + ch) == sfx_detail) { playing = true; break; }
            }
            if (playing) {
                audio_sound(sfx_detail, -2, 0, 0); /* stop this SFX on all channels */
            } else {
                int start = (keymod & KMOD_SHIFT) ? sfx_note_cursor : 0;
                audio_sound(sfx_detail, -1, start, SFX_NOTES - start);
            }
#endif
        }

        /* CTRL+UP/DOWN in pitch/tracker mode: skip 4 notes
         * CTRL+LEFT/RIGHT in tracker mode: jump to first/last field column */
        if ((keymod & KMOD_CTRL) && !sfx_hdr_editing &&
            (sfx_mode == 0 || sfx_mode == 1)) {
            if (scancode == SCANCODE_UP) {
                sfx_set_note_cursor(sfx_note_cursor >= 4 ? sfx_note_cursor - 4 : 0);
                tracker_ensure_visible();
                return;
            }
            if (scancode == SCANCODE_DOWN) {
                int end = SFX_NOTES - 1;
                sfx_set_note_cursor(sfx_note_cursor + 4 <= end
                                    ? sfx_note_cursor + 4 : end);
                tracker_ensure_visible();
                return;
            }
            if (sfx_mode == 1 && scancode == SCANCODE_LEFT) {
                sfx_field_cursor = 0;
                return;
            }
            if (sfx_mode == 1 && scancode == SCANCODE_RIGHT) {
                sfx_field_cursor = 3;
                return;
            }
        }

        switch (scancode) {
            case SCANCODE_TAB:
                /* If current SFX is marked as wav/instrument, show PCM editor always; don't cycle modes.
                 * Otherwise cycle between pitch(0), tracker(1), scale(2). */
                if (sfx_detail >= 0 && sfx_is_instrument(sfx_detail)) {
                    /* no-op while wav mode active */
                } else {
                    if (sfx_mode == 0) sfx_mode = 1;
                    else if (sfx_mode == 1) sfx_mode = 2;
                    else sfx_mode = 0;
                }
                break;
            case SCANCODE_DELETE:
                if (sfx_mode == 0) {
                    /* Delete current selected note and move selection to prev non-empty or first non-empty */
                    sfx_undo_maybe_push();
                    sfx_note_write(sfx_detail, sfx_note_cursor, 0);
                    int found = -1;
                    for (int i = sfx_note_cursor - 1; i >= 0; i--) {
                        if (NOTE_VOL(sfx_note(sfx_detail, i)) > 0) { found = i; break; }
                    }
                    if (found < 0) {
                        for (int i = 0; i < SFX_NOTES; i++) if (NOTE_VOL(sfx_note(sfx_detail, i)) > 0) { found = i; break; }
                    }
                    if (found >= 0) sfx_set_note_cursor(found);
                    tracker_ensure_visible();
                }
                break;
            case SCANCODE_HOME:
                sfx_set_note_cursor(0);
                tracker_ensure_visible();
                break;
            case SCANCODE_END:
                sfx_set_note_cursor(SFX_NOTES - 1);
                tracker_ensure_visible();
                break;
            case SCANCODE_PAGEUP:
                if (sfx_mode == 1) {
                    sfx_set_note_cursor(sfx_note_cursor > TRACKER_ROWS ? sfx_note_cursor - TRACKER_ROWS : 0);
                    tracker_ensure_visible();
                }
                break;
            case SCANCODE_PAGEDOWN:
                if (sfx_mode == 1) {
                    sfx_set_note_cursor(sfx_note_cursor + TRACKER_ROWS < SFX_NOTES ? sfx_note_cursor + TRACKER_ROWS : SFX_NOTES - 1);
                    tracker_ensure_visible();
                }
                break;
            case SCANCODE_LEFT:
                if (sfx_hdr_editing) {
                    if (sfx_hdr_col > 0) sfx_hdr_col--;
                } else if (sfx_mode == 1) {
                    /* Tracker: move field cursor left */
                    if (sfx_field_cursor > 0) sfx_field_cursor--;
                } else {
                    if (sfx_note_cursor > 0) sfx_set_note_cursor(sfx_note_cursor - 1);
                    tracker_ensure_visible();
                }
                break;
            case SCANCODE_RIGHT:
                if (sfx_hdr_editing) {
                    if (sfx_hdr_col < 2) sfx_hdr_col++;
                } else if (sfx_mode == 1) {
                    /* Tracker: move field cursor right */
                    if (sfx_field_cursor < 3) sfx_field_cursor++;
                } else {
                    if (sfx_note_cursor < SFX_NOTES - 1) sfx_set_note_cursor(sfx_note_cursor + 1);
                    tracker_ensure_visible();
                }
                break;
            case SCANCODE_UP: {
                if (sfx_hdr_editing) {
                    sfx_undo_maybe_push();
                    if (sfx_hdr_col == 0) {
                        int v = sfx_speed(sfx_detail);
                        if (v < 255) sfx_hdr_write(sfx_detail, SFX_HDR_SPEED, (uint8_t)(v + 1));
                    } else {
                        int off = (sfx_hdr_col == 1) ? SFX_HDR_LOOP_ST : SFX_HDR_LOOP_END;
                        int v = m_cart_memory[MEMORY_SFX + sfx_detail * SFX_ENTRY_BYTES + off];
                        if (v < 31) sfx_hdr_write(sfx_detail, off, (uint8_t)(v + 1));
                    }
                    break;
                }
                /* In pitch mode: raise pitch; tracker: raise field value or move cursor */
                if (sfx_mode == 0) {
                    sfx_undo_maybe_push();
                    uint16_t nd = sfx_note(sfx_detail, sfx_note_cursor);
                    int p = NOTE_PITCH(nd);
                    if (p < 63)
                        sfx_update_current_note(p + 1, -1, -1, -1, -1);
                } else if (sfx_mode == 1) {
                    if (!sfx_tracker_edit_mode) {
                        if (sfx_note_cursor > 0) sfx_note_cursor--;
                        tracker_ensure_visible();
                    } else {
                        sfx_undo_maybe_push();
                        uint16_t nd = sfx_note(sfx_detail, sfx_note_cursor);
                        switch (sfx_field_cursor) {
                            case 0: {
                                int p = NOTE_PITCH(nd);
                                if (p < 63)
                                    sfx_update_current_note(p + 1, -1, -1, -1, -1);
                                break;
                            }
                            case 1: {
                                int w = NOTE_WAVE(nd) + 8 * NOTE_CUSTOM(nd);
                                int wave = (w + 1) % 8;
                                int custom = (w != 15 && ((w + 1) >= 8)) ? 1 : 0;
                                sfx_wave_cursor = wave;
                                sfx_custom = custom;
                                sfx_update_current_note(-1, wave, -1, -1, custom);
                                break;
                            }
                            case 2: {
                                int v = NOTE_VOL(nd);
                                if (v < 7) {
                                    sfx_vol_cursor = v + 1;
                                    sfx_update_current_note(-1, -1, sfx_vol_cursor, -1, -1);
                                }
                                break;
                            }
                            case 3: {
                                int e = NOTE_EFF(nd);
                                sfx_eff_cursor = (e + 1) % 8;
                                sfx_update_current_note(-1, -1, -1, sfx_eff_cursor, -1);
                                break;
                            }
                        }
                    }
                } else {
                    if (sfx_note_cursor > 0) sfx_note_cursor--;
                    tracker_ensure_visible();
                }
                break;
            }
            case SCANCODE_DOWN: {
                if (sfx_hdr_editing) {
                    sfx_undo_maybe_push();
                    if (sfx_hdr_col == 0) {
                        int v = sfx_speed(sfx_detail);
                        if (v > 1) sfx_hdr_write(sfx_detail, SFX_HDR_SPEED, (uint8_t)(v - 1));
                    } else {
                        int off = (sfx_hdr_col == 1) ? SFX_HDR_LOOP_ST : SFX_HDR_LOOP_END;
                        int v = m_cart_memory[MEMORY_SFX + sfx_detail * SFX_ENTRY_BYTES + off];
                        if (v > 0) sfx_hdr_write(sfx_detail, off, (uint8_t)(v - 1));
                    }
                    sfx_hdr_editing = false; /* Down always exits header editing */
                    break;
                }
                /* In pitch mode: lower pitch; tracker: lower field value or move cursor */
                if (sfx_mode == 0) {
                    sfx_undo_maybe_push();
                    uint16_t nd = sfx_note(sfx_detail, sfx_note_cursor);
                    int p = NOTE_PITCH(nd);
                    if (p > 0)
                        sfx_update_current_note(p - 1, -1, -1, -1, -1);
                } else if (sfx_mode == 1) {
                    if (!sfx_tracker_edit_mode) {
                        if (sfx_note_cursor < SFX_NOTES - 1) sfx_note_cursor++;
                        tracker_ensure_visible();
                    } else {
                        sfx_undo_maybe_push();
                        uint16_t nd = sfx_note(sfx_detail, sfx_note_cursor);
                        switch (sfx_field_cursor) {
                            case 0: {
                                int p = NOTE_PITCH(nd);
                                if (p > 0 )
                                    sfx_update_current_note(p - 1, -1, -1, -1, -1);
                                break;
                            }
                            case 1: {
                                int w = NOTE_WAVE(nd) + 8 * NOTE_CUSTOM(nd);
                                int wave = (w == 0) ? 7 : ((w - 1) % 8);
                                int custom = ((w == 0) || ((w - 1) >= 8)) ? 1 : 0;
                                sfx_wave_cursor = wave;
                                sfx_custom = custom;
                                sfx_update_current_note(-1, wave, -1, -1, custom);
                                break;
                            }
                            case 2: {
                                int v = NOTE_VOL(nd);
                                if (v > 0) {
                                    sfx_vol_cursor = v - 1;
                                    sfx_update_current_note(-1, -1, sfx_vol_cursor, -1, -1);
                                }
                                break;
                            }
                            case 3: {
                                int e = NOTE_EFF(nd);
                                sfx_eff_cursor = (e == 0) ? 7 : (e - 1);
                                sfx_update_current_note(-1, -1, -1, sfx_eff_cursor, -1);
                                break;
                            }
                        }
                    }
                } else {
                    if (sfx_note_cursor < SFX_NOTES - 1) sfx_note_cursor++;
                    tracker_ensure_visible();
                }
                break;
            }
            default:
                /* Piano keyboard note entry in tracker mode.
                 * Layout: q2w3er5t6y7ui (upper, C4) / zsxdcvgbhnjm (lower, C3)
                 * SHIFT transposes both rows down one octave. */
                if ((sfx_mode == 1 || sfx_mode == 0) && !sfx_hdr_editing && !(keymod & KMOD_CTRL)) {
                    bool shift = (keymod & KMOD_SHIFT) != 0;
                    int pitch = piano_note_from_scancode(scancode, shift);
                    if (pitch >= 0 && pitch <= 63) {
                        sfx_undo_maybe_push();
                        sfx_update_current_note(pitch, -1, -1, -1, -1);
                        if (sfx_mode == 1) {
                            /* Tracker mode */
                            if (sfx_note_cursor < SFX_NOTES - 1) {
                                sfx_note_cursor++;
                                tracker_ensure_visible();
                            }
                        }
                        break;
                    }
                }
                if (keypress == 13 && sfx_hdr_editing) {
                    sfx_hdr_editing = false; /* Enter also exits header editing */
                } else if (keypress == 13 && sfx_mode == 1) {
                    /* Toggle tracker edit mode */
                    sfx_tracker_edit_mode = !sfx_tracker_edit_mode;
                }
                /* H key: toggle header editing in pitch mode only
                 * (in tracker mode H = G# via piano keyboard) */
                if (scancode == SCANCODE_H && sfx_mode == 0) {
                    sfx_hdr_editing = !sfx_hdr_editing;
                    sfx_hdr_col = 0;
                }
                /* waveform selector removed: no-op here */
                break;
        }
    }

}

/* --- Mouse handler -------------------------------------------------------- */

static void sfx_handle_mouse(int mx, int my, int buttons, int buttonsp, int keymod)
{
    sfx_undo_pushed_this_key = false;
    /* Mousewheel: scroll list or tracker depending on current view. */
    if (m_mouse_wheel != 0) {
        if (sfx_detail < 0) {
            sfx_list_scroll -= m_mouse_wheel;
            if (sfx_list_scroll < 0) sfx_list_scroll = 0;
            if (sfx_list_scroll > SFX_COUNT - LIST_ROWS) sfx_list_scroll = SFX_COUNT - LIST_ROWS;
        } else if (sfx_mode == 1) {  /* tracker mode */
            sfx_note_scroll -= m_mouse_wheel;
            if (sfx_note_scroll < 0) sfx_note_scroll = 0;
            if (sfx_note_scroll > SFX_NOTES - TRACKER_ROWS) sfx_note_scroll = SFX_NOTES - TRACKER_ROWS;
        }
    }

    int lmb_pressed = (buttonsp & 1);
    int lmb_held    = buttons & 1;
    int rmb_pressed = (buttonsp & 2);
    int rmb_held    = buttons & 2;

    /* Content area: x=0, y=GLYPH_HEIGHT, w=P8_WIDTH, h=P8_HEIGHT-GLYPH_HEIGHT */
    int cy    = GLYPH_HEIGHT;  /* = 6 */
    int h     = P8_HEIGHT - GLYPH_HEIGHT;  /* = 122 */
    int ybase = cy + GLYPH_HEIGHT;         /* = 12 (below title bar) */

    /* Global "wav" toggle button (top-right of title bar) - clickable in any detail mode */
    if (sfx_detail >= 0 && sfx_detail <= 7 && lmb_pressed &&
        mx >= P8_WIDTH - 3 * GLYPH_WIDTH - 2 && mx < P8_WIDTH &&
        my >= cy && my < cy + GLYPH_HEIGHT) {
        sfx_undo_maybe_push();
        int addr = MEMORY_SFX + sfx_detail * SFX_ENTRY_BYTES + SFX_HDR_LOOP_ST;
        uint8_t val = m_cart_memory[addr];
        uint8_t new = val ^ 0x80;
        if (new & 0x80) {
            /* enabling instrument: ensure loop start < SFX_NOTES */
            if ((new & 0x7F) >= SFX_NOTES) new = (new & 0x80) | 0x00;
        }
        m_cart_memory[addr] = new;
        goto handled;
    }

    if (sfx_detail < 0) {
        /* ── List view ───────────────────────────────────────────────────── */
        if (lmb_pressed && mx >= 0 && mx < P8_WIDTH &&
            my >= ybase && my < ybase + LIST_ROWS * GLYPH_HEIGHT) {
            int row_i = (my - ybase) / GLYPH_HEIGHT;
            int clicked_idx = sfx_list_scroll + row_i;
            if (clicked_idx >= 0 && clicked_idx < SFX_COUNT) {
                if (clicked_idx == sfx_cursor) {
                    /* Second click on same slot: open detail view */
                    sfx_detail      = sfx_cursor;
                    sfx_mode        = 0;
                    sfx_note_scroll = 0;
                    sfx_set_note_cursor(0);
                } else {
                    sfx_cursor = clicked_idx;
                    list_ensure_visible();
                }
            }
        }

    } else if (sfx_is_instrument(sfx_detail)) {
        /* ── PCM editor ────────────────────────────────────────── */
        if (sfx_is_instrument(sfx_detail)) {
            /* PCM draw: LMB drag over waveform area sets sample values */
            if (lmb_held) {
                int area_y0 = cy + GLYPH_HEIGHT + 1;
                int area_y1 = cy + h - GLYPH_HEIGHT - 1;
                int area_h = area_y1 - area_y0 + 1;
                if (area_h < 4) area_h = 4;
                int sample_w = P8_WIDTH / 64; if (sample_w < 1) sample_w = 1;
                int total_w = sample_w * 64;
                int left = (P8_WIDTH - total_w) / 2;
                /* only handle mouse when inside the centered PCM area */
                if (mx >= left && mx < left + total_w && my >= area_y0 && my < area_y0 + area_h) {
                    int sample_idx = (mx - left) / sample_w;
                    if (sample_idx >= 0 && sample_idx < 64) {
                        sfx_undo_maybe_push();
                        /* Exact inverse of draw_pcm_mode mapping:
                         * draw used: bar_y = mid_y - (sample * pcm_h) / 256
                         * so sample = (mid_y - my) * 256 / pcm_h
                         */
                        int pcm_h = area_h;
                        int mid_y = area_y0 + pcm_h / 2;
                        int raw = (mid_y - my) * 256 / pcm_h;
                        if (raw < -128) raw = -128;
                        if (raw > 127)  raw = 127;
                        m_cart_memory[MEMORY_SFX + sfx_detail * SFX_ENTRY_BYTES + sample_idx] = (uint8_t)(int8_t)raw;
                    }
                }
            }
        }

    } else if (sfx_mode == 0) {
        /* ── Pitch mode ──────────────────────────────────────────────────── */
        int bar_h = h - GLYPH_HEIGHT * 2;   /* = 110 */

        /* Title/header + instrument strip handling */
        if (handle_title_and_strip_mouse(mx, my, 0, cy, P8_WIDTH,
                                         lmb_pressed, lmb_held, rmb_pressed, keymod)) {
            goto handled;
        }
        /* instrument strip height used below */
        int strip_h = GLYPH_HEIGHT * 2 + 1;

        /* Bar area LMB drag: set note pitch from y-position.
         * Push undo once on initial press, not on every drag frame. */
        /* Account for compact strip height when computing bar area */
        ybase = cy + GLYPH_HEIGHT + strip_h;
        bar_h = h - GLYPH_HEIGHT * 2 - strip_h;
        if (bar_h < 1) bar_h = 1;

        if (lmb_pressed && mx >= 0 && mx < SFX_NOTES * 4 &&
            my >= ybase && my < ybase + bar_h) {
            sfx_undo_maybe_push();
        }
        /* Loop-drag handling: detect press near loop markers */
        int lps_p = m_cart_memory[MEMORY_SFX + sfx_detail * SFX_ENTRY_BYTES + SFX_HDR_LOOP_ST] & 0x7F;
        int lpe_p = m_cart_memory[MEMORY_SFX + sfx_detail * SFX_ENTRY_BYTES + SFX_HDR_LOOP_END] & 0x7F;
        int lps_x = lps_p * 4;
        int lpe_x = lpe_p * 4 + 3;
        if (lmb_pressed && my >= ybase && my < ybase + bar_h) {
            if (abs(mx - lps_x) <= 2) { sfx_drag_loop = 0; sfx_undo_maybe_push(); }
            else if (abs(mx - lpe_x) <= 2) { sfx_drag_loop = 1; sfx_undo_maybe_push(); }
        }
        if (sfx_drag_loop >= 0 && lmb_held) {
            int newpos = mx / 4; if (newpos < 0) newpos = 0; if (newpos >= SFX_NOTES) newpos = SFX_NOTES - 1;
            int addr = MEMORY_SFX + sfx_detail * SFX_ENTRY_BYTES + (sfx_drag_loop == 0 ? SFX_HDR_LOOP_ST : SFX_HDR_LOOP_END);
            uint8_t cur = m_cart_memory[addr];
            uint8_t nv = (cur & 0x80) | (uint8_t)(newpos & 0x7F);
            if (nv != cur) m_cart_memory[addr] = nv;
        }
        if (!lmb_held) sfx_drag_loop = -1;

        if (lmb_held && mx >= 0 && mx < SFX_NOTES * 4 &&
            my >= ybase && my < ybase + bar_h) {
            int n = mx / 4;
            if (n >= 0 && n < SFX_NOTES) {
                // Update note cursor, but *not* the selected waveform, volume or effect.
                sfx_note_cursor = n;
                /* Compute new pitch from mouse Y */
                int pitch = (bar_h - 1 - (my - ybase)) * 63 / (bar_h - 1);
                if (pitch < 0)  pitch = 0;
                if (pitch > 63) pitch = 63;
                if (keymod & KMOD_CTRL) pitch = snap_to_scale(pitch);
                /* Use current selected instrument and volume */
                sfx_update_current_note(pitch, sfx_wave_cursor, sfx_vol_cursor, sfx_eff_cursor, sfx_custom);
            }
        }
        /* Bar area RMB: clear the note (silence) at that column. Support drag clearing. */
        if ((rmb_pressed || rmb_held) && mx >= 0 && mx < SFX_NOTES * 4 &&
            my >= ybase && my < ybase + bar_h) {
            int n = mx / 4;
            if (n >= 0 && n < SFX_NOTES) {
                /* Right-click: select note and adopt its instrument+volume */
                sfx_set_note_cursor(n);
            }
        }

    } else if (sfx_mode == 1) {
        /* ── Tracker mode ───────────────────────────────────────────────── */



        /* Title/header + instrument strip handling */
        if (handle_title_and_strip_mouse(mx, my, 0, cy, P8_WIDTH,
                                         lmb_pressed, lmb_held, rmb_pressed, keymod)) {
            goto handled;
        }

        /* Filter switches row: LMB toggles/cycles, RMB cycles in reverse */
        {
            int fy = cy + h - 2 * GLYPH_HEIGHT;
            if ((lmb_pressed || rmb_pressed) && my >= fy && my < fy + GLYPH_HEIGHT) {
                int noiz, buzz, detune, reverb, dampen;
                sfx_get_filters(sfx_detail, &noiz, &buzz, &detune, &reverb, &dampen);
                /* Button x positions must match draw_tracker_mode */
                int bx_nz = 1 + 4  * GLYPH_WIDTH;
                int bx_bz = 1 + 7  * GLYPH_WIDTH;
                int bx_d  = 1 + 10 * GLYPH_WIDTH;
                int bx_r  = 1 + 13 * GLYPH_WIDTH;
                int bx_p  = 1 + 16 * GLYPH_WIDTH;
                int btn_w = 2 * GLYPH_WIDTH;
                bool changed = false;
                if (mx >= bx_nz && mx < bx_nz + btn_w) {
                    noiz = !noiz; changed = true;
                } else if (mx >= bx_bz && mx < bx_bz + btn_w) {
                    buzz = !buzz; changed = true;
                } else if (mx >= bx_d && mx < bx_d + btn_w) {
                    detune = lmb_pressed ? (detune + 1) % 3 : (detune + 2) % 3;
                    changed = true;
                } else if (mx >= bx_r && mx < bx_r + btn_w) {
                    reverb = lmb_pressed ? (reverb + 1) % 3 : (reverb + 2) % 3;
                    changed = true;
                } else if (mx >= bx_p && mx < bx_p + btn_w) {
                    dampen = lmb_pressed ? (dampen + 1) % 3 : (dampen + 2) % 3;
                    changed = true;
                }
                if (changed) {
                    sfx_undo_maybe_push();
                    sfx_write_filters(sfx_detail, noiz, buzz, detune, reverb, dampen);
                }
            }
        }

        /* Row click: move note cursor; SHIFT+click extends selection or
         * applies a single field to all notes; second click on same row
         * toggles all-fields highlight mode */
        if (lmb_pressed && my >= ybase &&
            my < ybase + TRACKER_ROWS * GLYPH_HEIGHT) {
            int row_i = (my - ybase) / GLYPH_HEIGHT;
            int n = sfx_note_scroll + row_i;
            if (n >= 0 && n < SFX_NOTES) {
                /* Determine which column was clicked */
                static const int field_col[4] = { 3, 7, 11, 13 };
                static const int field_len[4] = { 3, 3,  1,  3 };
                int clicked_f = -1;
                for (int f = 3; f >= 0; f--) {
                    int fx = 1 + field_col[f] * GLYPH_WIDTH;
                    int fw = field_len[f] * GLYPH_WIDTH;
                    if (mx >= fx - 1 && mx < fx + fw) { clicked_f = f; break; }
                }
                if (keymod & KMOD_SHIFT) {
                    /* SHIFT+click on wave/vol/eff column: apply that field to all notes */
                    if (clicked_f >= 1) {
                        uint16_t src = sfx_note(sfx_detail, n);
                        int lo = 0, hi = SFX_NOTES - 1;
                        if (sfx_sel_start >= 0) {
                            lo = sfx_sel_start < sfx_sel_end ? sfx_sel_start : sfx_sel_end;
                            hi = sfx_sel_start < sfx_sel_end ? sfx_sel_end   : sfx_sel_start;
                        }
                        sfx_undo_maybe_push();
                        for (int i = lo; i <= hi; i++) {
                            uint16_t nd = sfx_note(sfx_detail, i);
                            switch (clicked_f) {
                                case 1: nd = (nd & ~0x01C0) | (src & 0x01C0); break;
                                case 2: nd = (nd & ~0x0E00) | (src & 0x0E00); break;
                                case 3: nd = (nd & ~0x7000) | (src & 0x7000); break;
                            }
                            sfx_note_write(sfx_detail, i, nd);
                        }
                    } else {
                        /* SHIFT+click on pitch col or row# area: extend selection */
                        if (sfx_sel_start < 0) sfx_sel_start = sfx_note_cursor;
                        sfx_sel_end = n;
                        sfx_all_fields = false;
                    }
                } else if (n == sfx_note_cursor && !(keymod & KMOD_CTRL)) {
                    /* Double-click (second click on same row): select all fields */
                    sfx_all_fields = !sfx_all_fields;
                } else {
                    /* Normal click: move cursor, clear selection and all-fields */
                    sfx_sel_start = -1;
                    sfx_sel_end   = -1;
                    sfx_all_fields = false;
                    sfx_set_note_cursor(n);
                    tracker_ensure_visible();
                }
                if (clicked_f >= 0) sfx_field_cursor = clicked_f;
            }
        }

    } else if (sfx_mode == 2) {
        /* ── Scale editor ───────────────────────────────────────────────── */
        int key_x0 = 4;
        int key_y0 = cy + GLYPH_HEIGHT + 2;  /* = 14 */
        int key_h  = 20;
        int key_w  = 10;
        int btn_y  = key_y0 + key_h + 12;

        /* Piano key click: toggle semitone in scale mask */
        if (lmb_pressed && mx >= key_x0 && mx < key_x0 + 12 * key_w &&
            my >= key_y0 && my < key_y0 + key_h) {
            int semi = (mx - key_x0) / key_w;
            if (semi >= 0 && semi < 12)
                sfx_scale_bits ^= (uint16_t)(1u << semi);
        }

            /* Button row clicks */
            if (lmb_pressed && my >= btn_y && my < btn_y + GLYPH_HEIGHT) {
                if (mx >= 2 && mx < 12) {
                    /* [<] transpose down: rotate bits right */
                    sfx_scale_bits = (uint16_t)(((sfx_scale_bits >> 1) |
                                                 ((sfx_scale_bits & 1) << 11)) & 0x0FFF);
                } else if (mx >= 13 && mx < 23) {
                    /* [>] transpose up: rotate bits left */
                    sfx_scale_bits = (uint16_t)(((sfx_scale_bits << 1) |
                                                 (sfx_scale_bits >> 11)) & 0x0FFF);
                } else if (mx >= 30 && mx < 48) {
                    /* [inv] complement */
                    sfx_scale_bits = (uint16_t)((~sfx_scale_bits) & 0x0FFF);
                }
            }

            /* Click on preset name buttons */
            {
                int names_y_local = btn_y + GLYPH_HEIGHT + 6;
                int btn_w_local = 36;
                int gap_local = 2;
                int start_x_local = 2;
                int cols_local = 3;
                int total_local = (int)(sizeof(sfx_scale_names) / sizeof(sfx_scale_names[0]));
                int rows_local = (total_local + cols_local - 1) / cols_local;
                if (lmb_pressed && my >= names_y_local && my < names_y_local + rows_local * (GLYPH_HEIGHT + 4)) {
                    int rel_y = my - names_y_local;
                    int row = rel_y / (GLYPH_HEIGHT + 4);
                    int rel_x = mx - start_x_local;
                    if (rel_x >= 0) {
                        int col = rel_x / (btn_w_local + gap_local);
                        if (col >= 0 && col < cols_local) {
                            int idx = row * cols_local + col;
                            if (idx >= 0 && idx < total_local) {
                                sfx_undo_maybe_push();
                                sfx_scale_bits = sfx_scale_presets[idx];
                            }
                        }
                    }
                }
            }
    }

    handled:;
}

static bool sfx_handle_buttons(int button_mask, int buttonsp_mask)
{
    if (buttonsp_mask & BUTTON_MASK_ESCAPE) {
        if (sfx_mode == 2) { /* ESC exits scale editor (now mode 2) */
            sfx_mode = 0;
            return true;
        } else if (sfx_hdr_editing) {
            sfx_hdr_editing = false;
            return true;
        } else if (sfx_all_fields) {
            sfx_all_fields = false;
            return true;
        } else if (sfx_sel_start >= 0) {
            sfx_sel_start = -1;
            sfx_sel_end = -1;
            return true;
        } else if (sfx_detail != -1) {
            sfx_detail = -1; /* Escape: back to list */
            return true;
        }
    }
    return false;
}

static void sfx_init(void)
{
    assert(sfx_undo_ctx == NULL);

    sfx_cursor = 0;
    sfx_list_scroll = 0;
    sfx_detail = -1;
    sfx_mode = 0;
    sfx_note_cursor = 0;
    sfx_note_scroll = 0;
    sfx_field_cursor = 0;
    sfx_wave_cursor = 0;
    sfx_eff_cursor = 0;
    sfx_vol_cursor = 5;
    sfx_hdr_editing = false;
    sfx_hdr_col = 0;
    sfx_hdr_drag_field = -1;
    sfx_sel_start = -1;
    sfx_sel_end = -1;
    sfx_all_fields = false;
    sfx_clipboard_len = 0;
    sfx_custom = 0;

    sfx_undo_ctx = p8_editor_undo_create(MEMORY_SFX, MEMORY_SFX_SIZE);
}

static void sfx_shutdown(void)
{
    p8_editor_undo_destroy(sfx_undo_ctx);
    sfx_undo_ctx = NULL;
}

p8_editor_tab_t p8_subeditor_sfx = {
    "sfx",
    .init=sfx_init,
    .shutdown=sfx_shutdown,
    .draw=sfx_draw,
    .handle_keypress=sfx_handle_keypress,
    .handle_mouse=sfx_handle_mouse,
    .handle_buttons=sfx_handle_buttons
};
