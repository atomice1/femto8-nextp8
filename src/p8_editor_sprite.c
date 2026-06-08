/**
 * Copyright (C) 2026 Chris January
 *
 * Sprite editor — pixel painting, colour selection and sprite flag editing.
 *
 * Sheet mode (default):
 *   Sprite sheet view with 12 visible rows; colour palette (16 swatches) and
 *   sprite flag indicators are shown at the bottom.
 *
 * Pixel edit mode (Enter to enter, ESC to exit):
 *   8x zoomed view of the selected sprite with a moveable pixel cursor.
 *   Right-hand panel shows the colour palette and flag indicators.
 *   Mini sprite navigator strip at the bottom.
 *
 * Controls — sheet mode:
 *   Arrow keys  move sprite cursor          Q/W  prev/next sprite
 *   1/2         cycle selected colour       0-7  toggle sprite flag
 *   Enter       enter pixel edit mode
 *
 * Controls — pixel edit mode:
 *   Arrow keys  move pixel cursor           Space/Enter  paint pixel
 *   Backspace   clear pixel (colour 0)      1/2  cycle colour
 *   0-7         toggle sprite flag          Q/W  prev/next sprite
 *   ESC         back to sheet mode
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "p8_dialog.h"
#include "p8_editor.h"
#include "p8_editor_sprite.h"
#include "p8_editor_undo.h"
#include "p8_emu.h"
#include "p8_input.h"
#include "p8_overlay_helper.h"
#include "p8_editor_tab.h"

/* --- USB HID scancodes ---------------------------------------------------- */
#define SCANCODE_A         4
#define SCANCODE_C         6
#define SCANCODE_F         9
#define SCANCODE_G        10
#define SCANCODE_H        11
#define SCANCODE_P        19
#define SCANCODE_Q        20
#define SCANCODE_R        21
#define SCANCODE_S        22
#define SCANCODE_V        25
#define SCANCODE_W        26
#define SCANCODE_Y        28
#define SCANCODE_Z        29

/* Tool IDs */
#define SPRITE_TOOL_PENCIL 0
#define SPRITE_TOOL_FILL   1
#define SPRITE_TOOL_SELECT 2
#define SPRITE_TOOL_LINE   3
#define SPRITE_TOOL_RECT   4
#define SPRITE_TOOL_OVAL   5
#define SPRITE_TOOL_PAN    6
#define SCANCODE_L        15
#define SCANCODE_UP       82
#define SCANCODE_DOWN     81
#define SCANCODE_LEFT     80
#define SCANCODE_RIGHT    79
#define SCANCODE_PAGEUP   75
#define SCANCODE_PAGEDOWN 78

/* How many sprite-sheet rows are visible in sheet mode */
#define SHEET_VISIBLE_ROWS 13
/* Height of a flag indicator box (3px taller than a glyph) */
#define FLAGS_ROW_H (GLYPH_HEIGHT + 3)

/* --- State ----------------------------------------------------------------- */
static int  sel_sprite      = 0;     /* selected sprite 0-255              */
static int  sprite_scroll   = 0;     /* first visible sprite-sheet row     */
static int  sel_color       = 7;     /* active drawing colour 0-15         */
static bool sprite_edit     = false; /* false=sheet mode, true=px edit     */
static bool sprite_grid     = false; /* true=draw pixel grid overlay        */
static int  pixel_x         = 0;     /* pixel cursor x within sprite (0-7) */
static int  pixel_y         = 0;     /* pixel cursor y within sprite (0-7) */
static int  sprite_tool      = SPRITE_TOOL_PENCIL; /* active tool */
/* Pixel selection rectangle (in sprite-local pixel coords 0-7) */
static bool sel_rect_active   = false;
static int  sel_rect_x0       = 0;
static int  sel_rect_y0       = 0;
static int  sel_rect_x1       = 0;
static int  sel_rect_y1       = 0;
static bool sel_rect_dragging = false;
static uint8_t sprite_copy_buf[8 * 4]; /* 8 rows × 4 bytes */
static bool    sprite_copy_valid = false;
/* Shape tool drag state */
static bool  sprite_shape_dragging = false;  /* true while dragging a shape */
static int   sprite_shape_px0      = 0;      /* drag start pixel x (0-7)   */
static int   sprite_shape_py0      = 0;      /* drag start pixel y (0-7)   */
static uint8_t sprite_shape_snap[32];        /* 8 rows × 4 bytes: snapshot before drag */
/* Pan tool drag state */
static int pan_start_px = -1;  /* pixel x at drag start */
static int pan_start_py = -1;  /* pixel y at drag start */
static int pan_orig_sel_x0 = 0, pan_orig_sel_y0 = 0;  /* selection at drag start */
static int pan_orig_sel_x1 = 0, pan_orig_sel_y1 = 0;
/* Suppress spurious first click when entering edit mode from a sheet double-click */
static bool sprite_suppress_click = false;
/* Extra scroll offset for the mini sheet in edit mode */
static int sprite_mini_offset = 0;

static p8_editor_undo_ctx_t *sprite_undo_ctx = NULL;

/* --- Helpers --------------------------------------------------------------- */

static void sprite_ensure_visible(void)
{
    int row = sel_sprite / 16;
    if (row < sprite_scroll)
        sprite_scroll = row;
    if (row > sprite_scroll + SHEET_VISIBLE_ROWS - 1)
        sprite_scroll = row - SHEET_VISIBLE_ROWS + 1;
}

static inline void put_pixel(int x, int y, int col)
{
    if (x < overlay_clip_x0 || x >= overlay_clip_x1) return;
    if (y < overlay_clip_y0 || y >= overlay_clip_y1) return;
    uint8_t *dest = m_overlay_memory + (x >> 1) + y * 64;
    if (x & 1) *dest = (*dest & 0x0F) | ((col & 0xF) << 4);
    else        *dest = (*dest & 0xF0) |  (col & 0xF);
}

/* Draw rows of the sprite sheet starting at a given scroll row */
static void draw_sprite_sheet_region(int screen_x, int screen_y, int w,
                                     int max_rows, int scroll)
{
    for (int row = 0; row < max_rows; row++) {
        int sheet_row = scroll + row;
        if (sheet_row >= 16) break;
        int sheet_py_base = sheet_row * 8;
        for (int py = 0; py < 8; py++) {
            int oy = screen_y + row * 8 + py;
            if (oy < overlay_clip_y0 || oy >= overlay_clip_y1) continue;
            for (int px = 0; px < 128 && px < w; px++) {
                int ox = screen_x + px;
                if (ox < overlay_clip_x0 || ox >= overlay_clip_x1) continue;
                uint8_t b = m_cart_memory[MEMORY_SPRITES + (px >> 1) +
                                          (sheet_py_base + py) * 64];
                put_pixel(ox, oy, (px & 1) ? (b >> 4) : (b & 0xF));
            }
        }
    }
}

/* Draw 16 colour swatches in one row, highlighting the selected colour */
/* Draw 8 flag indicators for the given sprite: numbered 0-7, lit when set */
static void draw_flags_row(int x, int y, int sprite_idx)
{
    uint8_t flags = m_cart_memory[MEMORY_SPRITEFLAGS + sprite_idx];
    for (int f = 0; f < 8; f++) {
        bool on = (flags >> f) & 1;
        int cx = x + f * 8;
        overlay_draw_rectfill(cx, y, cx + 7, y + FLAGS_ROW_H - 1, on ? 10 : 1);
        overlay_draw_rect(cx, y, cx + 7, y + FLAGS_ROW_H - 1, on ? 6 : 5);
        char digit[2] = { '0' + f, '\0' };
        overlay_draw_simple_text(digit, cx + 2, y + 2, on ? 1 : 5);
    }
}

/* Draw a small tool icon inside a tool button.
 * bx/by = top-left corner of the button; ic = icon colour. */
static void draw_tool_icon(int tool, int bx, int by, int ic)
{
    int ox = bx + 1;
    int oy = by + 1;
    switch (tool) {
        case SPRITE_TOOL_PENCIL:   /* pencil: body diagonal + tip at bottom-left */
            put_pixel(ox+5, oy+0, ic); put_pixel(ox+6, oy+0, ic); /* eraser end */
            put_pixel(ox+4, oy+1, ic); put_pixel(ox+5, oy+1, ic);
            put_pixel(ox+3, oy+2, ic); put_pixel(ox+4, oy+2, ic);
            put_pixel(ox+2, oy+3, ic); put_pixel(ox+3, oy+3, ic);
            put_pixel(ox+1, oy+4, ic); put_pixel(ox+2, oy+4, ic);
            put_pixel(ox+0, oy+5, ic);                             /* tip point */
            break;
        case SPRITE_TOOL_FILL:     /* paint bucket: rectangular body + drip */
            overlay_draw_hline(ox+2, ox+4, oy+0, ic);             /* opening */
            put_pixel(ox+1, oy+1, ic); put_pixel(ox+5, oy+1, ic); /* sides */
            put_pixel(ox+1, oy+2, ic); put_pixel(ox+5, oy+2, ic);
            overlay_draw_hline(ox+1, ox+5, oy+3, ic);             /* bottom */
            put_pixel(ox+3, oy+4, ic);                             /* drip */
            put_pixel(ox+3, oy+5, ic);
            break;
        case SPRITE_TOOL_SELECT:   /* corner marks = selection rectangle */
            overlay_draw_hline(ox,   ox+2, oy,   ic);
            overlay_draw_vline(ox,   oy,   oy+2, ic);
            overlay_draw_hline(ox+5, ox+7, oy,   ic);
            overlay_draw_vline(ox+7, oy,   oy+2, ic);
            overlay_draw_hline(ox,   ox+2, oy+5, ic);
            overlay_draw_vline(ox,   oy+3, oy+5, ic);
            overlay_draw_hline(ox+5, ox+7, oy+5, ic);
            overlay_draw_vline(ox+7, oy+3, oy+5, ic);
            break;
        case SPRITE_TOOL_LINE:     /* "/" diagonal */
            put_pixel(ox+6, oy+0, ic); put_pixel(ox+5, oy+1, ic);
            put_pixel(ox+4, oy+2, ic); put_pixel(ox+3, oy+3, ic);
            put_pixel(ox+2, oy+4, ic); put_pixel(ox+1, oy+5, ic);
            break;
        case SPRITE_TOOL_RECT:     /* hollow rectangle */
            overlay_draw_rect(ox, oy, ox+7, oy+5, ic);
            break;
        case SPRITE_TOOL_OVAL:     /* oval outline */
            overlay_draw_hline(ox+2, ox+5, oy,   ic);
            overlay_draw_hline(ox+2, ox+5, oy+5, ic);
            overlay_draw_vline(ox+1, oy+1, oy+4, ic);
            overlay_draw_vline(ox+6, oy+1, oy+4, ic);
            break;
        case SPRITE_TOOL_PAN:      /* 4-arrow move symbol */
            put_pixel(ox+3, oy+0, ic);
            put_pixel(ox+2, oy+1, ic); put_pixel(ox+3, oy+1, ic); put_pixel(ox+4, oy+1, ic);
            put_pixel(ox+0, oy+2, ic); put_pixel(ox+1, oy+2, ic);
            put_pixel(ox+2, oy+2, ic); put_pixel(ox+3, oy+2, ic); put_pixel(ox+4, oy+2, ic);
            put_pixel(ox+5, oy+2, ic); put_pixel(ox+6, oy+2, ic);
            put_pixel(ox+2, oy+3, ic); put_pixel(ox+3, oy+3, ic); put_pixel(ox+4, oy+3, ic);
            put_pixel(ox+3, oy+4, ic);
            break;
    }
}

/* Draw the selected sprite at 8x zoom (64x64 px) with pixel cursor */
static void draw_zoomed_sprite(int x, int y)
{
    int sheet_bx = (sel_sprite % 16) * 8;
    int sheet_by = (sel_sprite / 16) * 8;
    for (int py = 0; py < 8; py++) {
        for (int px = 0; px < 8; px++) {
            int sx = sheet_bx + px;
            int sy = sheet_by + py;
            uint8_t b = m_cart_memory[MEMORY_SPRITES + (sx >> 1) + sy * 64];
            int col = (sx & 1) ? (b >> 4) : (b & 0xF);
            for (int dy = 0; dy < 8; dy++)
                for (int dx = 0; dx < 8; dx++)
                    put_pixel(x + px * 8 + dx, y + py * 8 + dy, col);
            if (px == pixel_x && py == pixel_y)
                overlay_draw_rect(x + px * 8, y + py * 8,
                                  x + px * 8 + 7, y + py * 8 + 7, 7);
        }
    }
    /* Grid overlay (CTRL-G) */
    if (sprite_grid) {
        for (int gx = 8; gx < 64; gx += 8)
            overlay_draw_vline(x + gx, y, y + 63, 5);
        for (int gy = 8; gy < 64; gy += 8)
            overlay_draw_hline(x, x + 63, y + gy, 5);
    }
}

/* Paint one pixel in the selected sprite */
static void sprite_paint_pixel(int px, int py, int color)
{
    /* Respect active selection: only paint within selected region */
    if (sel_rect_active) {
        int x0 = sel_rect_x0 < sel_rect_x1 ? sel_rect_x0 : sel_rect_x1;
        int x1 = sel_rect_x0 < sel_rect_x1 ? sel_rect_x1 : sel_rect_x0;
        int y0 = sel_rect_y0 < sel_rect_y1 ? sel_rect_y0 : sel_rect_y1;
        int y1 = sel_rect_y0 < sel_rect_y1 ? sel_rect_y1 : sel_rect_y0;
        if (px < x0 || px > x1 || py < y0 || py > y1) return;
    }
    int sheet_x = (sel_sprite % 16) * 8 + px;
    int sheet_y = (sel_sprite / 16) * 8 + py;
    uint8_t *byte = &m_cart_memory[MEMORY_SPRITES + sheet_y * 64 + (sheet_x >> 1)];
    if (sheet_x & 1) *byte = (*byte & 0x0F) | ((color & 0xF) << 4);
    else              *byte = (*byte & 0xF0) |  (color & 0xF);
}

/* Toggle one sprite flag */
static void toggle_flag(int sprite_idx, int flag)
{
    m_cart_memory[MEMORY_SPRITEFLAGS + sprite_idx] ^= (uint8_t)(1u << flag);
}

/* Flood-fill the 8x8 sprite at the current pixel cursor position */

/* Read the 8×8 sprite pixels into pixels[y][x] */
static void sprite_read_pixels(int pixels[8][8])
{
    int base_x = (sel_sprite % 16) * 8;
    int base_y = (sel_sprite / 16) * 8;
    for (int py = 0; py < 8; py++) {
        for (int px = 0; px < 8; px++) {
            int sx = base_x + px, sy = base_y + py;
            uint8_t b = m_cart_memory[MEMORY_SPRITES + (sx >> 1) + sy * 64];
            pixels[py][px] = (sx & 1) ? (b >> 4) : (b & 0xF);
        }
    }
}

/* Write pixels[y][x] back to the current sprite */
static void sprite_write_pixels(int pixels[8][8])
{
    for (int py = 0; py < 8; py++)
        for (int px = 0; px < 8; px++)
            sprite_paint_pixel(px, py, pixels[py][px]);
}

/* Flip sprite horizontally (mirror left↔right) */
static void sprite_flip_h(void)
{
    int src[8][8], dst[8][8];
    sprite_read_pixels(src);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            dst[y][x] = src[y][7 - x];
    sprite_write_pixels(dst);
}

/* Flip sprite vertically (mirror top↔bottom) */
static void sprite_flip_v(void)
{
    int src[8][8], dst[8][8];
    sprite_read_pixels(src);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            dst[y][x] = src[7 - y][x];
    sprite_write_pixels(dst);
}

/* Rotate sprite 90° clockwise: new[y][x] = old[7-x][y] */
static void sprite_rotate_cw(void)
{
    int src[8][8], dst[8][8];
    sprite_read_pixels(src);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            dst[y][x] = src[7 - x][y];
    sprite_write_pixels(dst);
}

/* Shift all sprite pixels by (dx,dy) with wrap-around */
static void sprite_shift_pixels(int dx, int dy)
{
    int src[8][8], dst[8][8];
    sprite_read_pixels(src);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            dst[y][x] = src[(y - dy + 8) % 8][(x - dx + 8) % 8];
    sprite_write_pixels(dst);
}

/* Flood-fill the 8x8 sprite at the current pixel cursor position */
static void sprite_flood_fill(int start_x, int start_y, int fill_color)
{
    int base_x = (sel_sprite % 16) * 8;
    int base_y = (sel_sprite / 16) * 8;

    int sx0 = base_x + start_x, sy0 = base_y + start_y;
    uint8_t b0 = m_cart_memory[MEMORY_SPRITES + (sx0 >> 1) + sy0 * 64];
    int target = (sx0 & 1) ? (b0 >> 4) : (b0 & 0xF);
    if (target == fill_color) return;

    /* Stack-based BFS; 8x8 sprite has at most 64 cells */
    uint8_t sx[64], sy[64];
    int sp = 0;
    uint8_t visited[8] = {0};  /* bit-per-pixel: visited[y] has bit x */

    sx[sp] = (uint8_t)start_x;
    sy[sp] = (uint8_t)start_y;
    sp++;
    visited[start_y] |= (uint8_t)(1u << start_x);

    while (sp > 0) {
        sp--;
        int cx = sx[sp], cy = sy[sp];
        sprite_paint_pixel(cx, cy, fill_color);

        static const int ndx[4] = { -1, 1, 0, 0 };
        static const int ndy[4] = {  0, 0, -1, 1 };
        for (int d = 0; d < 4; d++) {
            int nx = cx + ndx[d], ny = cy + ndy[d];
            if (nx < 0 || nx > 7 || ny < 0 || ny > 7) continue;
            if (visited[ny] & (uint8_t)(1u << nx)) continue;
            int snx = base_x + nx, sny = base_y + ny;
            uint8_t b = m_cart_memory[MEMORY_SPRITES + (snx >> 1) + sny * 64];
            int c = (snx & 1) ? (b >> 4) : (b & 0xF);
            if (c != target) continue;
            visited[ny] |= (uint8_t)(1u << nx);
            sx[sp] = (uint8_t)nx;
            sy[sp] = (uint8_t)ny;
            sp++;
        }
    }
}

/* --- Shape tool helpers ---------------------------------------------------- */

/* Integer square root (floor): works correctly for small n (up to ~2500) */
static int isqrt_i(int n)
{
    int r = 0;
    while ((r + 1) * (r + 1) <= n) r++;
    return r;
}

/* Save/restore the current sprite's 8×8 pixels as a snapshot */
static void sprite_save_snap(void)
{
    int bx = (sel_sprite % 16) * 4;
    int by = (sel_sprite / 16) * 8;
    for (int py = 0; py < 8; py++)
        memcpy(&sprite_shape_snap[py * 4],
               &m_cart_memory[MEMORY_SPRITES + (by + py) * 64 + bx], 4);
}

static void sprite_restore_snap(void)
{
    int bx = (sel_sprite % 16) * 4;
    int by = (sel_sprite / 16) * 8;
    for (int py = 0; py < 8; py++)
        memcpy(&m_cart_memory[MEMORY_SPRITES + (by + py) * 64 + bx],
               &sprite_shape_snap[py * 4], 4);
}

/* Bresenham line */
static void sprite_draw_line(int x0, int y0, int x1, int y1, int color)
{
    int dx = x1 - x0, dy = y1 - y0;
    int sx = dx > 0 ? 1 : dx < 0 ? -1 : 0;
    int sy = dy > 0 ? 1 : dy < 0 ? -1 : 0;
    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;
    int err = dx - dy;
    while (1) {
        if (x0 >= 0 && x0 < 8 && y0 >= 0 && y0 < 8)
            sprite_paint_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

/* Rectangle outline */
static void sprite_draw_rect_outline(int x0, int y0, int x1, int y1, int color)
{
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    for (int x = x0; x <= x1; x++) {
        if (x >= 0 && x < 8) {
            if (y0 >= 0 && y0 < 8) sprite_paint_pixel(x, y0, color);
            if (y1 != y0 && y1 >= 0 && y1 < 8) sprite_paint_pixel(x, y1, color);
        }
    }
    for (int y = y0 + 1; y < y1; y++) {
        if (y >= 0 && y < 8) {
            if (x0 >= 0 && x0 < 8) sprite_paint_pixel(x0, y, color);
            if (x1 != x0 && x1 >= 0 && x1 < 8) sprite_paint_pixel(x1, y, color);
        }
    }
}

/* Ellipse outline using double-coordinate scan (x-scan + y-scan for connectivity) */
static void sprite_draw_oval(int x0, int y0, int x1, int y1, int color)
{
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    int a2 = x1 - x0;  /* 2× semi-width  */
    int b2 = y1 - y0;  /* 2× semi-height */
    int cx2 = x0 + x1; /* 2× centre-x    */
    int cy2 = y0 + y1; /* 2× centre-y    */
    if (a2 == 0 && b2 == 0) {
        if (x0 >= 0 && x0 < 8 && y0 >= 0 && y0 < 8) sprite_paint_pixel(x0, y0, color);
        return;
    }
    if (a2 == 0) {
        int cx = cx2 / 2;
        for (int y = y0; y <= y1; y++)
            if (y >= 0 && y < 8 && cx >= 0 && cx < 8) sprite_paint_pixel(cx, y, color);
        return;
    }
    if (b2 == 0) {
        int cy = cy2 / 2;
        for (int x = x0; x <= x1; x++)
            if (x >= 0 && x < 8 && cy >= 0 && cy < 8) sprite_paint_pixel(x, cy, color);
        return;
    }
    /* Ellipse eq (doubled coords): (2px-cx2)^2*b2^2 + (2py-cy2)^2*a2^2 = (a2*b2)^2
     * Scan x: find top/bottom y.  Scan y: find left/right x. */
    for (int px = x0; px <= x1; px++) {
        if (px < 0 || px >= 8) continue;
        int dx2 = 2 * px - cx2;
        int numer = b2 * b2 * (a2 * a2 - dx2 * dx2);
        if (numer < 0) continue;
        int sv  = isqrt_i(numer);
        int top = (cy2 * a2 - sv) / (2 * a2);
        int bot = (cy2 * a2 + sv) / (2 * a2);
        if (top >= 0 && top < 8) sprite_paint_pixel(px, top, color);
        if (bot != top && bot >= 0 && bot < 8) sprite_paint_pixel(px, bot, color);
    }
    for (int py = y0; py <= y1; py++) {
        if (py < 0 || py >= 8) continue;
        int dy2   = 2 * py - cy2;
        int numer = a2 * a2 * (b2 * b2 - dy2 * dy2);
        if (numer < 0) continue;
        int sv    = isqrt_i(numer);
        int left  = (cx2 * b2 - sv) / (2 * b2);
        int right = (cx2 * b2 + sv) / (2 * b2);
        if (left >= 0 && left < 8) sprite_paint_pixel(left, py, color);
        if (right != left && right >= 0 && right < 8) sprite_paint_pixel(right, py, color);
    }
}

/* --- Draw ------------------------------------------------------------------ */

static void draw_sheet_mode(int x, int y, int w, int h)
{
    /* flags_y: where the flags bar starts (just above status bar separator) */
    int flags_y = h - GLYPH_HEIGHT - 1 - FLAGS_ROW_H;  /* = 106 when h=122 */

    overlay_draw_rectfill(x, y, x + w - 1, y + h - 1, 0);
    draw_sprite_sheet_region(x, y, w, SHEET_VISIBLE_ROWS, sprite_scroll);

    /* Selection box */
    {
        int col = sel_sprite % 16;
        int row = sel_sprite / 16;
        int bx  = x + col * 8;
        int by  = y + (row - sprite_scroll) * 8;
        overlay_draw_rect(bx - 1, by - 1, bx + 8, by + 8, 7);
    }

    /* Flags row just above status bar */
    overlay_draw_rectfill(x, y + flags_y, x + w - 1, y + flags_y + FLAGS_ROW_H - 1, 1);
    overlay_draw_simple_text("flags:", x + 1, y + flags_y + 2, 6);
    draw_flags_row(x + 6 * GLYPH_WIDTH, y + flags_y, sel_sprite);

    /* 1-pixel black separator between flags and status bar */
    overlay_draw_hline(x, x + w - 1, y + flags_y + FLAGS_ROW_H, 0);

    /* Status bar */
    overlay_draw_rectfill(x, y + h - GLYPH_HEIGHT, x + w - 1, y + h - 1, 1);
    char buf[32];
    snprintf(buf, sizeof(buf), "spr:%03d  1/2:col  enter:edit", sel_sprite);
    overlay_draw_simple_text(buf, x + 1, y + h - GLYPH_HEIGHT, 6);

    /* Scroll position indicator at top-right of sheet area */
    {
        char scroll_buf[12];
        snprintf(scroll_buf, sizeof(scroll_buf), "%02d/16", sprite_scroll);
        int sw = (int)strlen(scroll_buf) * GLYPH_WIDTH;
        overlay_draw_simple_text(scroll_buf, x + w - sw - 1, y, 5);
    }
}

static void draw_edit_mode(int x, int y, int w, int h)
{
    overlay_draw_rectfill(x, y, x + w - 1, y + h - 1, 1);

    /* Zoomed sprite: upper-left 64x64 */
    overlay_draw_rectfill(x, y, x + 63, y + 63, 0);
    draw_zoomed_sprite(x, y);

    /* Right panel: x+64..x+127, y..y+63 */
    int rx = x + 64;

    /* Palette: 2 rows x 8 swatches x 8x8 px = 64x16 */
    for (int c = 0; c < 16; c++) {
        int row = c / 8, pcol = c % 8;
        int cx = rx + pcol * 8;
        int cy = y + row * 8;
        overlay_draw_rectfill(cx, cy, cx + 7, cy + 7, c);
        if (c == sel_color)
            overlay_draw_rect(cx, cy, cx + 7, cy + 7, 7);
    }

    /* Flags row: y+16..y+16+FLAGS_ROW_H-1 on right panel */
    draw_flags_row(rx, y + 16, sel_sprite);

    /* Tool palette: two rows of 4 tools, 16px each (7 tools, last slot empty) */
    {
        static const char *tool_names[7] = {
            "pencil", "fill", "select", "line", "rect", "oval", "pan"
        };
        int tools_y0 = y + 16 + FLAGS_ROW_H + 1;  /* first row starts 1px below flags */
        for (int t = 0; t < 7; t++) {
            int row = t / 4, col = t % 4;
            int bx = rx + col * 16;
            int by_t = tools_y0 + row * 8;
            int bc = (sprite_tool == t) ? 2 : 1;
            int tc = (sprite_tool == t) ? 7 : 5;
            overlay_draw_rectfill(bx, by_t, bx + 15, by_t + 7, bc);
            draw_tool_icon(t, bx + 3, by_t, tc);
            if (col < 3) overlay_draw_vline(bx + 15, by_t, by_t + 7, 0);
        }
        /* Separator between two tool rows */
        overlay_draw_hline(rx, rx + 63, tools_y0 + 8, 0);
        /* Tooltip: name of active tool below tool rows */
        int tooltip_y = tools_y0 + 17;
        overlay_draw_rectfill(rx, tooltip_y, rx + 63, tooltip_y + GLYPH_HEIGHT - 1, 1);
        overlay_draw_simple_text(tool_names[sprite_tool], rx + 1, tooltip_y, 6);
    }

    /* Selection rect overlay on zoomed sprite */
    if (sel_rect_active) {
        int sx0 = sel_rect_x0 < sel_rect_x1 ? sel_rect_x0 : sel_rect_x1;
        int sx1 = sel_rect_x0 < sel_rect_x1 ? sel_rect_x1 : sel_rect_x0;
        int sy0 = sel_rect_y0 < sel_rect_y1 ? sel_rect_y0 : sel_rect_y1;
        int sy1 = sel_rect_y0 < sel_rect_y1 ? sel_rect_y1 : sel_rect_y0;
        overlay_draw_rect(x + sx0 * 8, y + sy0 * 8,
                          x + (sx1 + 1) * 8 - 1, y + (sy1 + 1) * 8 - 1, 7);
        overlay_draw_rect(x + sx0 * 8 + 1, y + sy0 * 8 + 1,
                          x + (sx1 + 1) * 8 - 2, y + (sy1 + 1) * 8 - 2, 0);
    }

    /* Info text */
    /* Mini sprite sheet: extends to status bar, partial last row visible */
    {
        int mini_rows = 8; /* draw 8 rows; status bar overdraw clips the last partial row */
        overlay_draw_rectfill(x, y + 64, x + w - 1, y + 64 + mini_rows * 8 - 1, 0);
        int mini_scroll = (sel_sprite / 16) - 3 + sprite_mini_offset;
        if (mini_scroll < 0)  mini_scroll = 0;
        if (mini_scroll > 10) mini_scroll = 10;
        draw_sprite_sheet_region(x, y + 64, w, mini_rows, mini_scroll);

        /* Selection box on mini sheet */
        {
            int col = sel_sprite % 16;
            int row = sel_sprite / 16;
            int bx  = x + col * 8;
            int by  = y + 64 + (row - mini_scroll) * 8;
            overlay_draw_rect(bx - 1, by - 1, bx + 8, by + 8, 7);
        }
    }

    /* Status bar */
    overlay_draw_rectfill(x, y + h - GLYPH_HEIGHT, x + w - 1, y + h - 1, 1);
    char buf[32];
    snprintf(buf, sizeof(buf), "spr:%d  px:%d,%d", sel_sprite, pixel_x, pixel_y);
    overlay_draw_simple_text(buf, x + 1, y + h - GLYPH_HEIGHT, 6);
}

static void sprite_draw(const p8_dialog_t *dialog, void *user_data,
                        int x, int y, int w, int h)
{
    (void)dialog; (void)user_data;
    if (sprite_edit)
        draw_edit_mode(x, y, w, h);
    else
        draw_sheet_mode(x, y, w, h);
}

/* --- Key handler ----------------------------------------------------------- */

static void sprite_handle_keypress(int scancode, int keypress, int keymod)
{
    if (keymod & KMOD_CTRL) {
        if (scancode == SCANCODE_G && sprite_edit) { sprite_grid = !sprite_grid; return; }
        if (scancode == SCANCODE_Z) { if (p8_editor_undo_do_undo(sprite_undo_ctx)) return; }
        if (scancode == SCANCODE_Y) { if (p8_editor_undo_do_redo(sprite_undo_ctx)) return; }
        if (scancode == SCANCODE_A && sprite_edit) {
            /* CTRL-A in edit mode: select all pixels */
            sel_rect_x0 = 0; sel_rect_y0 = 0;
            sel_rect_x1 = 7; sel_rect_y1 = 7;
            sel_rect_active = true;
            sprite_tool = SPRITE_TOOL_SELECT;
            return;
        }
        if (scancode == SCANCODE_C) {
            int bx = (sel_sprite % 16) * 4;  /* byte x: each row = 4 bytes */
            int by = (sel_sprite / 16) * 8;  /* pixel y: 8 rows per sprite */
            for (int py = 0; py < 8; py++)
                memcpy(&sprite_copy_buf[py * 4],
                       &m_cart_memory[MEMORY_SPRITES + (by + py) * 64 + bx], 4);
            sprite_copy_valid = true;
            return;
        }
        if (scancode == SCANCODE_V && sprite_copy_valid) {
            p8_editor_undo_push(sprite_undo_ctx);
            int bx = (sel_sprite % 16) * 4;
            int by = (sel_sprite / 16) * 8;
            for (int py = 0; py < 8; py++)
                memcpy(&m_cart_memory[MEMORY_SPRITES + (by + py) * 64 + bx],
                       &sprite_copy_buf[py * 4], 4);
            return;
        }
    }

    /* Colour cycling: '1' = prev, '2' = next */
    if (keypress == '1' && sel_color > 0)  sel_color--;
    if (keypress == '2' && sel_color < 15) sel_color++;

    /* Flag toggle: digit keys '0'-'7' */
    if (keypress >= '0' && keypress <= '7')
        toggle_flag(sel_sprite, keypress - '0');

    /* Q/W: previous / next sprite */
    if (scancode == SCANCODE_Q && sel_sprite > 0)   { sel_sprite--; sprite_ensure_visible(); }
    if (scancode == SCANCODE_W && sel_sprite < 255)  { sel_sprite++; sprite_ensure_visible(); }

    if (sprite_edit) {
        /* Pixel edit mode */
        switch (scancode) {
            case SCANCODE_LEFT:
                if (keymod & KMOD_SHIFT) { p8_editor_undo_push(sprite_undo_ctx); sprite_shift_pixels(-1, 0); }
                else if (pixel_x > 0) pixel_x--;
                break;
            case SCANCODE_RIGHT:
                if (keymod & KMOD_SHIFT) { p8_editor_undo_push(sprite_undo_ctx); sprite_shift_pixels(1, 0); }
                else if (pixel_x < 7) pixel_x++;
                break;
            case SCANCODE_UP:
                if (keymod & KMOD_SHIFT) { p8_editor_undo_push(sprite_undo_ctx); sprite_shift_pixels(0, -1); }
                else if (pixel_y > 0) pixel_y--;
                break;
            case SCANCODE_DOWN:
                if (keymod & KMOD_SHIFT) { p8_editor_undo_push(sprite_undo_ctx); sprite_shift_pixels(0, 1); }
                else if (pixel_y < 7) pixel_y++;
                break;
            default: break;
        }
        if (keypress == 32 || keypress == 13) {
            p8_editor_undo_push(sprite_undo_ctx);
            if (sprite_tool == SPRITE_TOOL_FILL)
                sprite_flood_fill(pixel_x, pixel_y, sel_color);
            else
                sprite_paint_pixel(pixel_x, pixel_y, sel_color);
        }
        if (keypress == 8) {
            if (sel_rect_active && sprite_tool == SPRITE_TOOL_SELECT) {
                /* DEL/BACKSPACE clears selected pixels */
                p8_editor_undo_push(sprite_undo_ctx);
                int x0 = sel_rect_x0 < sel_rect_x1 ? sel_rect_x0 : sel_rect_x1;
                int x1 = sel_rect_x0 < sel_rect_x1 ? sel_rect_x1 : sel_rect_x0;
                int y0 = sel_rect_y0 < sel_rect_y1 ? sel_rect_y0 : sel_rect_y1;
                int y1 = sel_rect_y0 < sel_rect_y1 ? sel_rect_y1 : sel_rect_y0;
                for (int cy = y0; cy <= y1; cy++)
                    for (int cx = x0; cx <= x1; cx++)
                        sprite_paint_pixel(cx, cy, 0);
            } else {
                p8_editor_undo_push(sprite_undo_ctx);
                sprite_paint_pixel(pixel_x, pixel_y, 0);
            }
        }
        if (scancode == SCANCODE_F && !(keymod & KMOD_CTRL)) {
            sprite_tool = SPRITE_TOOL_FILL;
            sprite_shape_dragging = false;
        }
        if (scancode == SCANCODE_P && !(keymod & KMOD_CTRL)) {
            sprite_tool = SPRITE_TOOL_PENCIL;
            sprite_shape_dragging = false;
        }
        if (scancode == SCANCODE_S && !(keymod & KMOD_CTRL)) {
            sprite_tool = SPRITE_TOOL_SELECT;
            sprite_shape_dragging = false;
        }
        if (scancode == SCANCODE_L && !(keymod & KMOD_CTRL)) {
            sprite_tool = SPRITE_TOOL_LINE;
            sprite_shape_dragging = false;
        }
        if (scancode == SCANCODE_H && !(keymod & KMOD_CTRL)) {
            p8_editor_undo_push(sprite_undo_ctx);
            sprite_flip_h();
        }
        if (scancode == SCANCODE_V && !(keymod & KMOD_CTRL)) {
            p8_editor_undo_push(sprite_undo_ctx);
            sprite_flip_v();
        }
        if (scancode == SCANCODE_R && !(keymod & KMOD_CTRL)) {
            p8_editor_undo_push(sprite_undo_ctx);
            sprite_rotate_cw();
        }
        if (keypress == '\t')
            sprite_edit = false;  /* TAB: toggle to sheet view */
    } else {
        /* Sheet mode */
        switch (scancode) {
            case SCANCODE_LEFT:  if (sel_sprite % 16 > 0)  sel_sprite--;    break;
            case SCANCODE_RIGHT: if (sel_sprite % 16 < 15) sel_sprite++;    break;
            case SCANCODE_UP:    if (sel_sprite >= 16)  sel_sprite -= 16;   break;
            case SCANCODE_DOWN:  if (sel_sprite < 240)  sel_sprite += 16;   break;
            case SCANCODE_PAGEUP:
                sprite_scroll -= SHEET_VISIBLE_ROWS;
                if (sprite_scroll < 0) sprite_scroll = 0;
                break;
            case SCANCODE_PAGEDOWN:
                sprite_scroll += SHEET_VISIBLE_ROWS;
                if (sprite_scroll > 16 - SHEET_VISIBLE_ROWS) sprite_scroll = 16 - SHEET_VISIBLE_ROWS;
                break;
            default: break;
        }
        if (keypress == 13) {
            sprite_edit = true;
            pixel_x = pixel_y = 0;
        }
        if (keypress == '\t') {
            sprite_edit = true;   /* TAB: toggle to pixel-edit view */
            pixel_x = pixel_y = 0;
        }
        sprite_ensure_visible();
    }

}

/* Replace all pixels of `from_color` with `to_color` in the current sprite */
static void sprite_replace_color(int from_color, int to_color)
{
    if (from_color == to_color) return;
    int bx = (sel_sprite % 16) * 8;  /* pixel x start of sprite in sheet */
    int by = (sel_sprite / 16) * 8;  /* pixel y start */
    for (int py = 0; py < 8; py++) {
        for (int px = 0; px < 8; px++) {
            int sx = bx + px, sy = by + py;
            int addr = MEMORY_SPRITES + (sx >> 1) + sy * 64;
            uint8_t b = m_cart_memory[addr];
            int c = (sx & 1) ? (b >> 4) : (b & 0xF);
            if (c == from_color) {
                if (sx & 1) b = (b & 0x0F) | ((to_color & 0xF) << 4);
                else        b = (b & 0xF0) | (to_color & 0xF);
                m_cart_memory[addr] = b;
            }
        }
    }
}

/* --- Mouse handler -------------------------------------------------------- */

static void sprite_handle_mouse(int mx, int my, int buttons, int buttonsp, int keymod)
{
    /* Mousewheel scrolls the sprite sheet (both sheet mode and edit-mode mini sheet). */
    if (m_mouse_wheel != 0 && !sprite_edit) {
        sprite_scroll -= m_mouse_wheel;
        if (sprite_scroll < 0) sprite_scroll = 0;
        if (sprite_scroll > 16 - SHEET_VISIBLE_ROWS) sprite_scroll = 16 - SHEET_VISIBLE_ROWS;
    }
    if (m_mouse_wheel != 0 && sprite_edit) {
        int cy2 = GLYPH_HEIGHT;
        if (my >= cy2 + 64 && my < P8_HEIGHT - GLYPH_HEIGHT) {
            sprite_mini_offset -= m_mouse_wheel;
            if (sprite_mini_offset < -3) sprite_mini_offset = -3;
            if (sprite_mini_offset >  3) sprite_mini_offset =  3;
        }
    }

    int lmb_pressed = (buttonsp & 1);
    int lmb_held    = buttons & 1;
    int rmb_pressed = (buttonsp & 2);

    /* Content area origin (sub-editor content starts at y = GLYPH_HEIGHT) */
    int cy = GLYPH_HEIGHT;  /* = 6 */

    if (sprite_edit) {
        /* ── Pixel edit mode ─────────────────────────────────────────────── */

        /* Suppress spurious first click when entering edit mode via double-click */
        if (!lmb_held) sprite_suppress_click = false;
        if (sprite_suppress_click) { lmb_pressed = 0; lmb_held = 0; }

        /* Tool palette: two rows of 4, 16px wide each, at cy+26..cy+42 */
        if (lmb_pressed && mx >= 64 && mx < 128 &&
            my >= cy + 16 + FLAGS_ROW_H + 1 && my < cy + 16 + FLAGS_ROW_H + 1 + 16) {
            int row = (my - (cy + 16 + FLAGS_ROW_H + 1)) / 8;
            int col = (mx - 64) / 16;
            int t = row * 4 + col;
            if (t >= 0 && t < 7) {
                sprite_tool = t;
                sprite_shape_dragging = false;
            }
        }

        /* Zoomed sprite area: x=[0,64), y=[cy, cy+64) */
        if (mx >= 0 && mx < 64 && my >= cy && my < cy + 64) {
            int px = mx / 8;
            int py = (my - cy) / 8;

            /* Always track pixel cursor from mouse position */
            pixel_x = px;
            pixel_y = py;

            /* Right-click: eyedropper (sample pixel colour) for all tools */
            if (!lmb_held && rmb_pressed) {
                int sheet_x = (sel_sprite % 16) * 8 + px;
                int sheet_y = (sel_sprite / 16) * 8 + py;
                uint8_t b = m_cart_memory[MEMORY_SPRITES + (sheet_x >> 1) + sheet_y * 64];
                sel_color = (sheet_x & 1) ? (b >> 4) : (b & 0xF);
            } else if (sprite_tool == SPRITE_TOOL_SELECT) {
                /* Selection tool: LMB drag creates selection rect */
                if (lmb_pressed) {
                    sel_rect_x0 = sel_rect_x1 = px;
                    sel_rect_y0 = sel_rect_y1 = py;
                    sel_rect_active   = true;
                    sel_rect_dragging = true;
                }
                if (lmb_held && sel_rect_dragging) {
                    sel_rect_x1 = px;
                    sel_rect_y1 = py;
                }
                if (!lmb_held) sel_rect_dragging = false;
            } else if (sprite_tool >= SPRITE_TOOL_LINE && sprite_tool < SPRITE_TOOL_PAN) {
                /* Shape tools: line, rect, oval — LMB drag defines the shape */
                if (lmb_pressed) {
                    p8_editor_undo_push(sprite_undo_ctx);
                    sprite_save_snap();
                    sprite_shape_dragging = true;
                    sprite_shape_px0 = px;
                    sprite_shape_py0 = py;
                }
                if (lmb_held && sprite_shape_dragging) {
                    sprite_restore_snap();
                    switch (sprite_tool) {
                        case SPRITE_TOOL_LINE: sprite_draw_line(sprite_shape_px0, sprite_shape_py0, px, py, sel_color); break;
                        case SPRITE_TOOL_RECT: sprite_draw_rect_outline(sprite_shape_px0, sprite_shape_py0, px, py, sel_color); break;
                        case SPRITE_TOOL_OVAL: sprite_draw_oval(sprite_shape_px0, sprite_shape_py0, px, py, sel_color); break;
                        default: break;
                    }
                }
                if (!lmb_held) sprite_shape_dragging = false;
            } else if (sprite_tool == SPRITE_TOOL_PAN) {
                /* Pan tool: non-destructive — restore snap each frame, then blit */
                if (lmb_pressed) {
                    p8_editor_undo_push(sprite_undo_ctx);
                    sprite_save_snap();
                    pan_start_px = px;
                    pan_start_py = py;
                    if (sel_rect_active) {
                        pan_orig_sel_x0 = sel_rect_x0;
                        pan_orig_sel_y0 = sel_rect_y0;
                        pan_orig_sel_x1 = sel_rect_x1;
                        pan_orig_sel_y1 = sel_rect_y1;
                    }
                }
                if (lmb_held && pan_start_px >= 0) {
                    int total_dx = px - pan_start_px;
                    int total_dy = py - pan_start_py;
                    sprite_restore_snap();
                    if (sel_rect_active) {
                        /* Move only the selected region */
                        int x0 = pan_orig_sel_x0 < pan_orig_sel_x1 ? pan_orig_sel_x0 : pan_orig_sel_x1;
                        int x1 = pan_orig_sel_x0 < pan_orig_sel_x1 ? pan_orig_sel_x1 : pan_orig_sel_x0;
                        int y0 = pan_orig_sel_y0 < pan_orig_sel_y1 ? pan_orig_sel_y0 : pan_orig_sel_y1;
                        int y1 = pan_orig_sel_y0 < pan_orig_sel_y1 ? pan_orig_sel_y1 : pan_orig_sel_y0;
                        int tmp[8][8] = {{0}};
                        for (int sy = y0; sy <= y1; sy++)
                            for (int sx = x0; sx <= x1; sx++) {
                                int bsx = (sel_sprite % 16) * 8 + sx;
                                int bsy = (sel_sprite / 16) * 8 + sy;
                                uint8_t bv = m_cart_memory[MEMORY_SPRITES + (bsx >> 1) + bsy * 64];
                                tmp[sy][sx] = (bsx & 1) ? (bv >> 4) : (bv & 0xF);
                                /* Clear source pixel */
                                if (bsx & 1) bv = (bv & 0x0F);
                                else         bv = (bv & 0xF0);
                                m_cart_memory[MEMORY_SPRITES + (bsx >> 1) + bsy * 64] = bv;
                            }
                        for (int sy = y0; sy <= y1; sy++)
                            for (int sx = x0; sx <= x1; sx++) {
                                int nx = sx + total_dx, ny = sy + total_dy;
                                if (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                                    int bsx = (sel_sprite % 16) * 8 + nx;
                                    int bsy = (sel_sprite / 16) * 8 + ny;
                                    uint8_t bv = m_cart_memory[MEMORY_SPRITES + (bsx >> 1) + bsy * 64];
                                    int cv = tmp[sy][sx] & 0xF;
                                    if (bsx & 1) bv = (bv & 0x0F) | (cv << 4);
                                    else         bv = (bv & 0xF0) | cv;
                                    m_cart_memory[MEMORY_SPRITES + (bsx >> 1) + bsy * 64] = bv;
                                }
                            }
                        /* Update selection rect: original + total offset, clamped to 0-7 */
                        sel_rect_x0 = pan_orig_sel_x0 + total_dx;
                        sel_rect_y0 = pan_orig_sel_y0 + total_dy;
                        sel_rect_x1 = pan_orig_sel_x1 + total_dx;
                        sel_rect_y1 = pan_orig_sel_y1 + total_dy;
                        if (sel_rect_x0 < 0) sel_rect_x0 = 0;
                        if (sel_rect_x0 > 7) sel_rect_x0 = 7;
                        if (sel_rect_x1 < 0) sel_rect_x1 = 0;
                        if (sel_rect_x1 > 7) sel_rect_x1 = 7;
                        if (sel_rect_y0 < 0) sel_rect_y0 = 0;
                        if (sel_rect_y0 > 7) sel_rect_y0 = 7;
                        if (sel_rect_y1 < 0) sel_rect_y1 = 0;
                        if (sel_rect_y1 > 7) sel_rect_y1 = 7;
                    } else {
                        sprite_shift_pixels(total_dx, total_dy);
                    }
                }
                if (!lmb_held) { pan_start_px = -1; pan_start_py = -1; }
            } else {
                /* Pencil / fill */
                if (lmb_pressed) {
                    p8_editor_undo_push(sprite_undo_ctx);
                    if (keymod & KMOD_CTRL) {
                        /* CTRL+click: search-and-replace colour under cursor */
                        int sheet_x = (sel_sprite % 16) * 8 + px;
                        int sheet_y = (sel_sprite / 16) * 8 + py;
                        uint8_t b = m_cart_memory[MEMORY_SPRITES + (sheet_x >> 1) + sheet_y * 64];
                        int c = (sheet_x & 1) ? (b >> 4) : (b & 0xF);
                        sprite_replace_color(c, sel_color);
                    } else if (sprite_tool == SPRITE_TOOL_FILL) {
                        sprite_flood_fill(px, py, sel_color);
                    }
                }
                if (lmb_held && sprite_tool == SPRITE_TOOL_PENCIL) {
                    sprite_paint_pixel(px, py, sel_color);
                }
            }
        }

        /* Right panel palette: x=[64,128), y=[cy, cy+16) */
        if (lmb_pressed && mx >= 64 && mx < 128 &&
            my >= cy && my < cy + 16) {
            int row = (my - cy) / 8;
            int col = (mx - 64) / 8;
            sel_color = row * 8 + col;
        }

        /* Right panel flags: x=[64,128), y=[cy+16, cy+16+FLAGS_ROW_H) */
        if (lmb_pressed && mx >= 64 && mx < 128 &&
            my >= cy + 16 && my < cy + 16 + FLAGS_ROW_H) {
            int f = (mx - 64) / 8;
            if (f >= 0 && f < 8)
                toggle_flag(sel_sprite, f);
        }

        /* Mini sprite sheet: y=[cy+64, status bar) */
        if (lmb_pressed && my >= cy + 64 && my < P8_HEIGHT - GLYPH_HEIGHT) {
            int mini_scroll = (sel_sprite / 16) - 3 + sprite_mini_offset;
            if (mini_scroll < 0)  mini_scroll = 0;
            if (mini_scroll > 10) mini_scroll = 10;
            int row_on_screen = (my - (cy + 64)) / 8;
            int col = mx / 8;
            if (col >= 0 && col < 16) {
                int new_sprite = (mini_scroll + row_on_screen) * 16 + col;
                if (new_sprite >= 0 && new_sprite < 256) {
                    sel_sprite = new_sprite;
                    sprite_mini_offset = 0; /* reset offset when selecting new sprite */
                }
            }
        }

    } else {
        /* ── Sheet mode ──────────────────────────────────────────────────── */

        /* Sprite sheet area: y=[cy, cy + SHEET_VISIBLE_ROWS*8) */
        if (lmb_pressed && my >= cy && my < cy + SHEET_VISIBLE_ROWS * 8) {
            int col = mx / 8;
            int row_on_screen = (my - cy) / 8;
            if (col >= 0 && col < 16) {
                int sheet_row = sprite_scroll + row_on_screen;
                int clicked = sheet_row * 16 + col;
                if (clicked >= 0 && clicked < 256) {
                    if (clicked == sel_sprite) {
                        /* Second click on same sprite: enter pixel edit mode */
                        sprite_edit = true;
                        sprite_suppress_click = true;
                        pixel_x = pixel_y = 0;
                    } else {
                        sel_sprite = clicked;
                    }
                }
            }
        }

        /* Flags row click: same y-range as drawn in draw_sheet_mode
         * flags_y_abs = (P8_HEIGHT - GLYPH_HEIGHT) - GLYPH_HEIGHT - 1 - FLAGS_ROW_H */
        {
            int flags_y_abs = (P8_HEIGHT - GLYPH_HEIGHT) - GLYPH_HEIGHT - 1 - FLAGS_ROW_H;
            if (lmb_pressed && my >= cy + flags_y_abs &&
                my < cy + flags_y_abs + FLAGS_ROW_H) {
                int flags_start = 6 * GLYPH_WIDTH;  /* skip "flags:" label */
                int f = (mx - flags_start) / 8;
                if (f >= 0 && f < 8)
                    toggle_flag(sel_sprite, f);
            }
        }
    }
}

static bool sprite_handle_buttons(int button_mask, int buttonp_mask)
{
    if (buttonp_mask & BUTTON_MASK_ESCAPE) {
        if (sel_rect_active) {
            sel_rect_active = false;
            return true;
        } else if (sprite_edit) {
            sprite_edit = false;
            return true;
        }
    }
    return false;
}

static void sprite_init(void)
{
    assert(sprite_undo_ctx == NULL);

    sel_sprite = 0;
    sprite_scroll = 0;
    sel_color = 7;
    sprite_edit = false;
    sprite_grid = false;
    pixel_x = 0;
    pixel_y = 0;
    sprite_tool = SPRITE_TOOL_PENCIL;
    sel_rect_active = false;
    sel_rect_dragging = false;
    sprite_copy_valid = false;
    sprite_shape_dragging = false;
    pan_start_px = -1;
    pan_start_py = -1;
    pan_orig_sel_x0 = 0;
    pan_orig_sel_y0 = 0;
    pan_orig_sel_x1 = 0;
    pan_orig_sel_y1 = 0;
    sprite_suppress_click = false;
    sprite_mini_offset = 0;

    sprite_undo_ctx = p8_editor_undo_create(
            MEMORY_SPRITES,
            MEMORY_SPRITES_SIZE + MEMORY_SPRITES_MAP_SIZE);
}

static void sprite_shutdown(void)
{
    p8_editor_undo_destroy(sprite_undo_ctx);
    sprite_undo_ctx = NULL;
}

p8_editor_tab_t p8_subeditor_sprite = {
    "spr",
    .init=sprite_init,
    .shutdown=sprite_shutdown,
    .draw=sprite_draw,
    .handle_keypress=sprite_handle_keypress,
    .handle_mouse=sprite_handle_mouse,
    .handle_buttons=sprite_handle_buttons,
};
