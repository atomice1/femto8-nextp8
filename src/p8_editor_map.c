/**
 * Copyright (C) 2026 Chris January
 *
 * Map editor — read-only stub that renders the 128×32 tile map using sprites
 * from cart memory. Navigation scrolls a 16×15-tile viewport.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "p8_dialog.h"
#include "p8_editor.h"
#include "p8_editor_map.h"
#include "p8_editor_undo.h"
#include "p8_emu.h"
#include "p8_input.h"
#include "p8_overlay_helper.h"
#include "p8_editor_tab.h"

/* --- USB HID scancodes ---------------------------------------------------- */
#define SCANCODE_A         4
#define SCANCODE_C         6
#define SCANCODE_Q        20
#define SCANCODE_R        21
#define SCANCODE_S        22
#define SCANCODE_V        25
#define SCANCODE_W        26
#define SCANCODE_Y        28
#define SCANCODE_Z        29
#define SCANCODE_UP       82
#define SCANCODE_DOWN     81
#define SCANCODE_LEFT     80
#define SCANCODE_RIGHT    79
#define SCANCODE_SPACE    44
#define SCANCODE_PAGEUP   75
#define SCANCODE_PAGEDOWN 78

/* MAP dimensions */
#define MAP_COLS 128
#define MAP_ROWS  32
/* Viewport: 12 tile rows (96 px) + 16 px tile strip + 8 px status = 120 px */
#define MAP_VIEW_COLS 16
#define MAP_VIEW_ROWS 12

/* --- State ----------------------------------------------------------------- */
static int  map_cursor_x   = 0;   /* selected tile column 0..127  */
static int  map_cursor_y   = 0;   /* selected tile row    0..31   */
static int  map_pan_last_mx = -1; /* mouse x at last pan frame, -1=none */
static int  map_pan_last_my = -1; /* mouse y at last pan frame */
static int  map_scroll_x   = 0;   /* leftmost visible column      */
static int  map_scroll_y   = 0;   /* topmost visible row          */
static int  map_sel_sprite = 0;   /* tile to paint (0..255)       */
static int  map_strip_scroll = 0; /* base sprite row shown in tile strip (0-14) */
/* Tile selection rectangle (map tile coordinates) */
static bool map_sel_active   = false;
static int  map_sel_x0       = 0;
static int  map_sel_y0       = 0;
static int  map_sel_x1       = 0;
static int  map_sel_y1       = 0;
static bool map_sel_dragging = false;
/* Tile clipboard */
static uint8_t map_copy_buf[MAP_COLS * MAP_ROWS];
static int     map_copy_w    = 0;
static int     map_copy_h    = 0;

static p8_editor_undo_ctx_t *map_undo_ctx = NULL;

/* --- Helpers --------------------------------------------------------------- */

static void map_ensure_visible(void)
{
    if (map_cursor_x < map_scroll_x)           map_scroll_x = map_cursor_x;
    if (map_cursor_x >= map_scroll_x + MAP_VIEW_COLS) map_scroll_x = map_cursor_x - MAP_VIEW_COLS + 1;
    if (map_cursor_y < map_scroll_y)           map_scroll_y = map_cursor_y;
    if (map_cursor_y >= map_scroll_y + MAP_VIEW_ROWS) map_scroll_y = map_cursor_y - MAP_VIEW_ROWS + 1;
}

/* Draw one 8×8 sprite from the sprite sheet at screen position (sx, sy) */
static void draw_tile_sprite(int sx, int sy, int sprite_idx)
{
    /* Sprite (sprite_idx) top-left pixel in the sprite sheet */
    int sheet_px = (sprite_idx % 16) * 8;
    int sheet_py = (sprite_idx / 16) * 8;
    for (int dy = 0; dy < 8; dy++) {
        int oy = sy + dy;
        if (oy < overlay_clip_y0 || oy >= overlay_clip_y1) continue;
        int sheet_row = sheet_py + dy;
        for (int dx = 0; dx < 8; dx++) {
            int ox = sx + dx;
            if (ox < overlay_clip_x0 || ox >= overlay_clip_x1) continue;
            int sheet_col = sheet_px + dx;
            uint8_t b = m_cart_memory[MEMORY_SPRITES + (sheet_col >> 1) + sheet_row * 64];
            int col = (sheet_col & 1) ? (b >> 4) : (b & 0xF);
            uint8_t *dest = m_overlay_memory + (ox >> 1) + oy * 64;
            if (ox & 1) *dest = (*dest & 0x0F) | ((col & 0xF) << 4);
            else        *dest = (*dest & 0xF0) |  (col & 0xF);
        }
    }
}

/* Draw the tile selector strip (2 rows = 16 px) at the given position.
 * Uses map_strip_scroll to determine which sprite rows to show. */
static void draw_tile_strip(int x, int y)
{
    int base_row = map_strip_scroll;
    for (int sr = 0; sr < 2; sr++) {
        int sheet_row = base_row + sr;
        if (sheet_row > 15) break;
        for (int col = 0; col < 16; col++) {
            draw_tile_sprite(x + col * 8, y + sr * 8, sheet_row * 16 + col);
        }
    }
    /* Highlight selected sprite if it is visible in the strip */
    int sel_row = map_sel_sprite / 16;
    int sel_col = map_sel_sprite % 16;
    if (sel_row >= base_row && sel_row < base_row + 2) {
        int sy = y + (sel_row - base_row) * 8;
        overlay_draw_rect(x + sel_col * 8 - 1, sy - 1, x + sel_col * 8 + 8, sy + 8, 7);
    }
}

/* --- Subeditor callbacks --------------------------------------------------- */

static void map_draw(const p8_dialog_t *dialog, void *user_data,
                     int x, int y, int w, int h)
{
    (void)dialog; (void)user_data;
    overlay_draw_rectfill(x, y, x + w - 1, y + h - 1, 0);

    /* Map tiles (12 rows = 96 px) */
    for (int ty = 0; ty < MAP_VIEW_ROWS; ty++) {
        int map_y = map_scroll_y + ty;
        if (map_y >= MAP_ROWS) break;
        for (int tx = 0; tx < MAP_VIEW_COLS; tx++) {
            int map_x = map_scroll_x + tx;
            if (map_x >= MAP_COLS) break;
            int sprite_idx = m_cart_memory[MEMORY_MAP + map_y * MAP_COLS + map_x];
            draw_tile_sprite(x + tx * 8, y + ty * 8, sprite_idx);
        }
    }

    /* Cursor outline */
    int cx = x + (map_cursor_x - map_scroll_x) * 8;
    int cy = y + (map_cursor_y - map_scroll_y) * 8;
    overlay_draw_rect(cx - 1, cy - 1, cx + 8, cy + 8, 7);

    /* Selection rectangle overlay */
    if (map_sel_active) {
        int sx0 = map_sel_x0 < map_sel_x1 ? map_sel_x0 : map_sel_x1;
        int sx1 = map_sel_x0 < map_sel_x1 ? map_sel_x1 : map_sel_x0;
        int sy0 = map_sel_y0 < map_sel_y1 ? map_sel_y0 : map_sel_y1;
        int sy1 = map_sel_y0 < map_sel_y1 ? map_sel_y1 : map_sel_y0;
        int vx0 = x + (sx0 - map_scroll_x) * 8 - 1;
        int vy0 = y + (sy0 - map_scroll_y) * 8 - 1;
        int vx1 = x + (sx1 - map_scroll_x + 1) * 8;
        int vy1 = y + (sy1 - map_scroll_y + 1) * 8;
        overlay_draw_rect(vx0, vy0, vx1, vy1, 10);
        overlay_draw_rect(vx0 + 1, vy0 + 1, vx1 - 1, vy1 - 1, 0);
    }

    /* Tile selector strip (2 rows = 16 px) — just above the status bar */
    int strip_y = y + h - GLYPH_HEIGHT - 16;
    overlay_draw_rectfill(x, strip_y, x + w - 1, strip_y + 15, 1);
    draw_tile_strip(x, strip_y);

    /* Status bar */
    char buf[40];
    snprintf(buf, sizeof(buf), "m:%03d,%03d spr:%03d",
             map_cursor_x, map_cursor_y, map_sel_sprite);
    overlay_draw_rectfill(x, y + h - GLYPH_HEIGHT, x + w - 1, y + h - 1, 1);
    overlay_draw_simple_text(buf, x + 1, y + h - GLYPH_HEIGHT, 6);
}

static void map_handle_keypress(int scancode, int keypress, int keymod)
{
    if (keymod & KMOD_CTRL) {
        if (scancode == SCANCODE_Z) { if (p8_editor_undo_do_undo(map_undo_ctx)) return; }
        if (scancode == SCANCODE_Y) { if (p8_editor_undo_do_redo(map_undo_ctx)) return; }
        if (scancode == SCANCODE_C) {
            if (map_sel_active) {
                int sx0 = map_sel_x0 < map_sel_x1 ? map_sel_x0 : map_sel_x1;
                int sx1 = map_sel_x0 < map_sel_x1 ? map_sel_x1 : map_sel_x0;
                int sy0 = map_sel_y0 < map_sel_y1 ? map_sel_y0 : map_sel_y1;
                int sy1 = map_sel_y0 < map_sel_y1 ? map_sel_y1 : map_sel_y0;
                map_copy_w = sx1 - sx0 + 1;
                map_copy_h = sy1 - sy0 + 1;
                for (int row = 0; row < map_copy_h; row++)
                    for (int col = 0; col < map_copy_w; col++)
                        map_copy_buf[row * map_copy_w + col] =
                            m_cart_memory[MEMORY_MAP + (sy0 + row) * MAP_COLS + (sx0 + col)];
            }
            return;
        }
        if (scancode == SCANCODE_V) {
            if (map_copy_w > 0 && map_copy_h > 0) {
                p8_editor_undo_push(map_undo_ctx);
                for (int row = 0; row < map_copy_h; row++) {
                    int dy = map_cursor_y + row;
                    if (dy >= MAP_ROWS) break;
                    for (int col = 0; col < map_copy_w; col++) {
                        int dx = map_cursor_x + col;
                        if (dx >= MAP_COLS) break;
                        m_cart_memory[MEMORY_MAP + dy * MAP_COLS + dx] =
                            map_copy_buf[row * map_copy_w + col];
                    }
                }
            }
            return;
        }
        return;
    }

    /* Q/W scroll the strip; [ ] fine-tune the selected sprite */
    if (scancode == SCANCODE_Q) {
        if (map_strip_scroll > 0) map_strip_scroll--;
        /* Jump selected sprite to the top strip row */
        if (map_sel_sprite / 16 != map_strip_scroll)
            map_sel_sprite = map_strip_scroll * 16 + (map_sel_sprite % 16);
    }
    if (scancode == SCANCODE_W) {
        if (map_strip_scroll < 14) map_strip_scroll++;
        if (map_sel_sprite / 16 != map_strip_scroll)
            map_sel_sprite = map_strip_scroll * 16 + (map_sel_sprite % 16);
    }
    if (keypress == '[' && map_sel_sprite > 0)   map_sel_sprite--;
    if (keypress == ']' && map_sel_sprite < 255) map_sel_sprite++;

    /* Paint tile at cursor (Enter only; Space is reserved for pan-while-drag) */
    if (keypress == 13) {
        p8_editor_undo_push(map_undo_ctx);
        m_cart_memory[MEMORY_MAP + map_cursor_y * MAP_COLS + map_cursor_x] =
            (uint8_t)map_sel_sprite;
    }

    switch (scancode) {
        case SCANCODE_LEFT:  if (map_cursor_x > 0)            map_cursor_x--; break;
        case SCANCODE_RIGHT: if (map_cursor_x < MAP_COLS - 1) map_cursor_x++; break;
        case SCANCODE_UP:    if (map_cursor_y > 0)            map_cursor_y--; break;
        case SCANCODE_DOWN:  if (map_cursor_y < MAP_ROWS - 1) map_cursor_y++; break;
        case SCANCODE_PAGEUP:
            map_scroll_y -= MAP_VIEW_ROWS;
            if (map_scroll_y < 0) map_scroll_y = 0;
            map_cursor_y = map_scroll_y;
            break;
        case SCANCODE_PAGEDOWN:
            map_scroll_y += MAP_VIEW_ROWS;
            if (map_scroll_y > MAP_ROWS - MAP_VIEW_ROWS) map_scroll_y = MAP_ROWS - MAP_VIEW_ROWS;
            map_cursor_y = map_scroll_y;
            break;
        default: break;
    }
    map_ensure_visible();
}

/* --- Mouse handler -------------------------------------------------------- */

static void map_handle_mouse(int mx, int my, int buttons, int buttonsp, int keymod)
{
    int strip_y = GLYPH_HEIGHT + (P8_HEIGHT - GLYPH_HEIGHT) - GLYPH_HEIGHT - 16;
    /* Mousewheel over tile strip scrolls the strip rows; elsewhere scrolls the map. */
    if (m_mouse_wheel != 0) {
        if (my >= strip_y && my < strip_y + 16) {
            /* Scroll strip independently */
            map_strip_scroll -= m_mouse_wheel;
            if (map_strip_scroll < 0)  map_strip_scroll = 0;
            if (map_strip_scroll > 14) map_strip_scroll = 14;
        } else {
            map_scroll_y -= m_mouse_wheel;
            if (map_scroll_y < 0) map_scroll_y = 0;
            if (map_scroll_y > MAP_ROWS - MAP_VIEW_ROWS) map_scroll_y = MAP_ROWS - MAP_VIEW_ROWS;
        }
    }

    int lmb_held    = buttons & 1;
    int lmb_pressed = (buttonsp & 1);

    int cy = GLYPH_HEIGHT;                         /* content area y = 6   */
    int map_area_h = MAP_VIEW_ROWS * 8;            /* = 96 px              */

    /* Space held + LMB drag: pan the viewport */
    bool space_held = m_scancodes[SCANCODE_SPACE];
    if (space_held && lmb_held && mx >= 0 && mx < MAP_VIEW_COLS * 8 &&
        my >= cy && my < cy + map_area_h) {
        if (map_pan_last_mx >= 0) {
            int dx = mx - map_pan_last_mx;
            int dy = my - map_pan_last_my;
            /* Accumulate pixel offsets; shift view by one tile per 8 pixels */
            map_scroll_x -= dx / 8;
            map_scroll_y -= dy / 8;
            if (map_scroll_x < 0) map_scroll_x = 0;
            if (map_scroll_x > MAP_COLS - MAP_VIEW_COLS) map_scroll_x = MAP_COLS - MAP_VIEW_COLS;
            if (map_scroll_y < 0) map_scroll_y = 0;
            if (map_scroll_y > MAP_ROWS - MAP_VIEW_ROWS) map_scroll_y = MAP_ROWS - MAP_VIEW_ROWS;
        }
        map_pan_last_mx = mx;
        map_pan_last_my = my;
    } else {
        map_pan_last_mx = -1;
        map_pan_last_my = -1;

        /* SHIFT+LMB drag in map area: rubber-band tile selection */
        if ((keymod & KMOD_SHIFT) && (lmb_held || lmb_pressed) &&
            mx >= 0 && mx < MAP_VIEW_COLS * 8 &&
            my >= cy && my < cy + map_area_h) {
            int tx    = mx / 8;
            int ty    = (my - cy) / 8;
            int map_x = map_scroll_x + tx;
            int map_y = map_scroll_y + ty;
            if (map_x >= 0 && map_x < MAP_COLS && map_y >= 0 && map_y < MAP_ROWS) {
                if (lmb_pressed) {
                    map_sel_x0 = map_sel_x1 = map_x;
                    map_sel_y0 = map_sel_y1 = map_y;
                    map_sel_active   = true;
                    map_sel_dragging = true;
                }
                if (lmb_held && map_sel_dragging) {
                    map_sel_x1 = map_x;
                    map_sel_y1 = map_y;
                }
            }
            if (!lmb_held) map_sel_dragging = false;
        } else {
            if (!lmb_held) map_sel_dragging = false;

            /* Map viewport: LMB places selected tile, RMB selects sprite from cell */
            if (lmb_held && mx >= 0 && mx < MAP_VIEW_COLS * 8 &&
                my >= cy && my < cy + map_area_h) {
                /* Normal click/drag clears selection */
                if (lmb_pressed) { map_sel_active = false; map_sel_dragging = false; }
                int tx    = mx / 8;
                int ty    = (my - cy) / 8;
                int map_x = map_scroll_x + tx;
                int map_y = map_scroll_y + ty;
                if (map_x >= 0 && map_x < MAP_COLS &&
                    map_y >= 0 && map_y < MAP_ROWS) {
                    map_cursor_x = map_x;
                    map_cursor_y = map_y;
                    int lmb_pressed_now = (buttonsp & 1);
                    if (lmb_pressed_now) { p8_editor_undo_push(map_undo_ctx); }
                    m_cart_memory[MEMORY_MAP + map_y * MAP_COLS + map_x] =
                        (uint8_t)map_sel_sprite;
                }
            }
            /* RMB press on map cell: select the sprite in that cell */
            int rmb_pressed_flag = (buttonsp & 2);
            if (rmb_pressed_flag && mx >= 0 && mx < MAP_VIEW_COLS * 8 &&
                my >= cy && my < cy + map_area_h) {
                int tx    = mx / 8;
                int ty    = (my - cy) / 8;
                int map_x = map_scroll_x + tx;
                int map_y = map_scroll_y + ty;
                if (map_x >= 0 && map_x < MAP_COLS && map_y >= 0 && map_y < MAP_ROWS) {
                    int sprite_at_cell = m_cart_memory[MEMORY_MAP + map_y * MAP_COLS + map_x];
                    map_sel_sprite = sprite_at_cell;
                    /* Scroll strip to show newly selected sprite */
                    int new_row = map_sel_sprite / 16;
                    if (new_row < map_strip_scroll) map_strip_scroll = new_row;
                    if (new_row >= map_strip_scroll + 2) map_strip_scroll = new_row - 1;
                }
            }
        }
    }

    /* Tile selector strip: 2 rows of 16 sprites each */
    if (lmb_pressed && mx >= 0 && mx < P8_WIDTH &&
        my >= strip_y && my < strip_y + 16) {
        int sr  = (my - strip_y) / 8;   /* 0 = top row, 1 = bottom row */
        int col = mx / 8;
        if (col >= 0 && col < 16) {
            int new_sel = (map_strip_scroll + sr) * 16 + col;
            if (new_sel >= 0 && new_sel < 256)
                map_sel_sprite = new_sel;
        }
    }
}

static bool map_handle_buttons(int button_mask, int buttonp_mask)
{
    /* ESC: clear selection */
    if (buttonp_mask & BUTTON_MASK_ESCAPE) {
        if (map_sel_active) {
            map_sel_active = false;
            return true;
        }
    }
    return false;
}

static void map_init(void)
{
    assert(map_undo_ctx == NULL);

    map_cursor_x = 0;
    map_cursor_y = 0;
    map_scroll_x = 0;
    map_scroll_y = 0;
    map_sel_sprite = 0;
    map_strip_scroll = 0;
    map_sel_active  = false;
    map_sel_dragging = false;
    map_copy_w = 0;
    map_copy_h = 0;

    map_undo_ctx = p8_editor_undo_create(MEMORY_MAP, MEMORY_MAP_SIZE);
}

static void map_shutdown(void)
{
    p8_editor_undo_destroy(map_undo_ctx);
    map_undo_ctx = NULL;
}

p8_editor_tab_t p8_subeditor_map = {
    "map",
    .init=map_init,
    .shutdown=map_shutdown,
    .draw=map_draw,
    .handle_keypress=map_handle_keypress,
    .handle_mouse=map_handle_mouse,
    .handle_buttons=map_handle_buttons
};
