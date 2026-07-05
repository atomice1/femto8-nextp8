/**
 * Copyright (C) 2026 Chris January
 *
 * Code editor.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "p8_cstore.h"
#include "p8_dialog.h"
#include "p8_editor.h"
#include "p8_editor_code.h"
#include "p8_editor_syntax.h"
#include "p8_emu.h"
#include "p8_input.h"
#include "p8_overlay_helper.h"
#include "p8_editor_tab.h"

#define EDITOR_BG_NORMAL      DIALOG_BG_NORMAL
#define EDITOR_BG_HIGHLIGHT   DIALOG_BG_INVERTED
#define EDITOR_BG_CURSOR      DIALOG_BG_HIGHLIGHT
#define EDITOR_TEXT_NORMAL    DIALOG_TEXT_NORMAL
#define EDITOR_TEXT_HIGHLIGHT DIALOG_TEXT_HIGHLIGHT

#define SCANCODE_A         4
#define SCANCODE_B         5
#define SCANCODE_C         6
#define SCANCODE_D         7
#define SCANCODE_E         8
#define SCANCODE_F         9
#define SCANCODE_G        10
#define SCANCODE_L        15
#define SCANCODE_P        19
#define SCANCODE_R        21
#define SCANCODE_S        22
#define SCANCODE_V        25
#define SCANCODE_W        26
#define SCANCODE_X        27
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
#define SCANCODE_DEL      76
#define SCANCODE_END      77
static char **lines = NULL;
static int *line_lengths = NULL;
static int line_count = 0;
static int scroll_offset_x = 0;
static int scroll_offset_y = 0;
static int cursor_line = 0;
static int cursor_col = 0;
static int select_start_line = -1;
static int select_start_col = -1;
static int select_end_line = -1;
static int select_end_col = -1;
static bool puny_mode = false;
static bool sync_required = false;
static bool showing = false;

/* --- Clipboard, search, and undo state ------------------------------------- */

static char *clipboard = NULL;

static char search_str[128] = "";

#define UNDO_LIMIT 8
static char *undo_stack[UNDO_LIMIT];
static int   undo_depth = 0;
static char *redo_stack[UNDO_LIMIT];
static int   redo_depth = 0;

/* --- Selection anchor (set when SHIFT is held during movement) -------------- */

static int select_anchor_line = -1;
static int select_anchor_col  = -1;

/* Syntax highlighting colour cache.
 *
 * syn_color_cache[i]     – heap buffer of colour indices for line i
 * syn_color_cache_cap[i] – allocated capacity of syn_color_cache[i]
 * syntax_contexts[i]     – p8_syn_context_t at the start of line i
 * syn_valid_lines        – lines 0..syn_valid_lines-1 have up-to-date colours
 * syn_cache_alloc        – current allocated length of the three arrays
 *
 * Invariant: syn_valid_lines <= line_count <= syn_cache_alloc (after first
 * allocation).  To invalidate from line N onwards:
 *   syn_valid_lines = MIN(syn_valid_lines, N);
 * draw_editor extends the cache to line_count before rendering. */
static uint8_t          **syn_color_cache     = NULL;
static int               *syn_color_cache_cap = NULL;
static p8_syn_context_t  *syntax_contexts     = NULL;
static int                syn_valid_lines     = 0;
static int                syn_cache_alloc     = 0;

static void free_lines(void)
{
    /* Free the colour cache.  Iterate syn_cache_alloc so we cover entries
     * allocated beyond the current line_count (e.g. after line deletions). */
    if (syn_color_cache) {
        for (int i = 0; i < syn_cache_alloc; i++)
            free(syn_color_cache[i]);
        free(syn_color_cache);
        syn_color_cache = NULL;
    }
    free(syn_color_cache_cap);
    syn_color_cache_cap = NULL;
    free(syntax_contexts);
    syntax_contexts     = NULL;
    syn_valid_lines     = 0;
    syn_cache_alloc     = 0;

    if (lines) {
        for (int i = 0; i < line_count; i++)
            free(lines[i]);
        free(lines);
        free(line_lengths);
        lines = NULL;
        line_lengths = NULL;
        line_count = 0;
    }

    sync_required = true;
}

/* Extend the per-line colour cache from syn_valid_lines up to line_count.
 * Called lazily from draw_editor when syn_valid_lines < line_count. */
static void syn_extend_cache(void)
{
    /* Grow the three arrays when line_count exceeds the current allocation. */
    if (line_count > syn_cache_alloc) {
        uint8_t         **tc = realloc(syn_color_cache,     line_count * sizeof(*tc));
        int              *ti = realloc(syn_color_cache_cap, line_count * sizeof(*ti));
        p8_syn_context_t *tx = realloc(syntax_contexts,   (line_count + 1) * sizeof(*tx));
        if (!tc || !ti || !tx) {
            /* Partial allocation: keep whichever succeeded and bail out. */
            if (tc) syn_color_cache     = tc;
            if (ti) syn_color_cache_cap = ti;
            if (tx) syntax_contexts     = tx;
            return;
        }
        syn_color_cache     = tc;
        syn_color_cache_cap = ti;
        syntax_contexts     = tx;
        /* Zero-initialise newly added slots so realloc below is safe. */
        for (int i = syn_cache_alloc; i < line_count; i++) {
            syn_color_cache[i]     = NULL;
            syn_color_cache_cap[i] = 0;
        }
        syn_cache_alloc = line_count;
    }

    /* Seed the starting context when rebuilding from scratch. */
    if (syn_valid_lines == 0)
        syntax_contexts[0] = (p8_syn_context_t){ 0, 0 };

    for (int i = syn_valid_lines; i < line_count; i++) {
        p8_syn_context_t ctx = syntax_contexts[i];
        int llen = line_lengths[i];

        if (llen > syn_color_cache_cap[i]) {
            uint8_t *tmp = realloc(syn_color_cache[i], (size_t)(llen + 1));
            if (tmp) {
                syn_color_cache[i]     = tmp;
                syn_color_cache_cap[i] = llen;
            } else {
                free(syn_color_cache[i]);
                syn_color_cache[i]     = NULL;
                syn_color_cache_cap[i] = 0;
            }
        }

        if (syn_color_cache[i])
            p8_syn_highlight_line(lines[i], llen, syn_color_cache[i], &ctx);
        syntax_contexts[i + 1] = ctx;
    }
    syn_valid_lines = line_count;
}

static void split_lines(const char *script)
{
    assert(lines == NULL);

    // Count lines - a trailing newline doesn't create an extra empty line
    line_count = 1;
    int len = strlen(script);

    // Count newline characters
    for (int i = 0; i < len - 1; i++) {
        if (script[i] == '\n')
            line_count++;
    }

    // Allocate line pointers
    lines = malloc(line_count * sizeof(char *));
    if (!lines)
        return;
    line_lengths = malloc(line_count * sizeof(int));
    if (!line_lengths) {
        free(lines);
        lines = NULL;
        return;
    }

    // Split lines
    int line_index = 0;
    const char *start_of_line = script;
    for (const char *p = script; ; p++) {
        if (*p == '\n' || *p == '\0') {
            bool eof = *p == '\0';
            // Don't create an empty line at the end if the string ends with \n
            int line_len = p - start_of_line;
            if (line_len > 0 || len == 0 || !eof) {
                lines[line_index] = malloc(line_len + 1);
                memcpy(lines[line_index], start_of_line, line_len);
                lines[line_index][line_len] = '\0';
                line_lengths[line_index] = line_len;
                line_index++;
            }
            start_of_line = p + 1;
            if (eof)
                break;
        }
    }
}

static void join_lines(char **out_script)
{
    int total_length = 1;
    for (int i = 0; i < line_count; i++) {
        total_length += line_lengths[i] + 1;
    }

    char *p;
    if (!*out_script) {
        char *script = malloc(total_length);
        if (!script)
            return;
        *out_script = script;
        p = script;
    } else {
        p = *out_script;
    }

    for (int i = 0; i < line_count; i++) {
        memcpy(p, lines[i], line_lengths[i]);
        p += line_lengths[i];
        *p++ = '\n';
    }
    *p = '\0';
}

/* --- Selection helpers ----------------------------------------------------- */

static void clear_selection(void)
{
    select_anchor_line = -1;
    select_anchor_col  = -1;
    select_start_line  = -1;
    select_start_col   = -1;
    select_end_line    = -1;
    select_end_col     = -1;
}

static void update_selection(void)
{
    if (select_anchor_line < cursor_line ||
        (select_anchor_line == cursor_line && select_anchor_col <= cursor_col)) {
        select_start_line = select_anchor_line;
        select_start_col  = select_anchor_col;
        select_end_line   = cursor_line;
        select_end_col    = cursor_col;
    } else {
        select_start_line = cursor_line;
        select_start_col  = cursor_col;
        select_end_line   = select_anchor_line;
        select_end_col    = select_anchor_col;
    }
    if (select_start_line == select_end_line && select_start_col == select_end_col)
        clear_selection();
}

static char *get_selected_text(void)
{
    if (select_start_line == -1) return NULL;
    if (select_start_line == select_end_line) {
        int len = select_end_col - select_start_col;
        char *text = malloc(len + 1);
        if (!text) return NULL;
        memcpy(text, lines[select_start_line] + select_start_col, len);
        text[len] = '\0';
        return text;
    }
    int total = (line_lengths[select_start_line] - select_start_col) + 1;
    for (int l = select_start_line + 1; l < select_end_line; l++)
        total += line_lengths[l] + 1;
    total += select_end_col + 1;
    char *text = malloc(total);
    if (!text) return NULL;
    char *p = text;
    int len = line_lengths[select_start_line] - select_start_col;
    memcpy(p, lines[select_start_line] + select_start_col, len); p += len; *p++ = '\n';
    for (int l = select_start_line + 1; l < select_end_line; l++) {
        memcpy(p, lines[l], line_lengths[l]); p += line_lengths[l]; *p++ = '\n';
    }
    memcpy(p, lines[select_end_line], select_end_col); p += select_end_col; *p = '\0';
    return text;
}

static void delete_selected_text(void)
{
    if (select_start_line == -1) return;
    cursor_line = select_start_line;
    cursor_col  = select_start_col;
    if (select_start_line == select_end_line) {
        int del = select_end_col - select_start_col;
        memmove(lines[cursor_line] + cursor_col,
                lines[cursor_line] + select_end_col,
                line_lengths[cursor_line] - select_end_col + 1);
        line_lengths[cursor_line] -= del;
    } else {
        int new_len = select_start_col + (line_lengths[select_end_line] - select_end_col);
        char *new_line = malloc(new_len + 1);
        if (new_line) {
            memcpy(new_line, lines[select_start_line], select_start_col);
            memcpy(new_line + select_start_col,
                   lines[select_end_line] + select_end_col,
                   line_lengths[select_end_line] - select_end_col + 1);
            free(lines[select_start_line]);
            lines[select_start_line] = new_line;
            line_lengths[select_start_line] = new_len;
        }
        int rm = select_end_line - select_start_line;
        for (int l = select_start_line + 1; l <= select_end_line; l++) free(lines[l]);
        memmove(lines + select_start_line + 1, lines + select_end_line + 1,
                (line_count - select_end_line - 1) * sizeof(char *));
        memmove(line_lengths + select_start_line + 1, line_lengths + select_end_line + 1,
                (line_count - select_end_line - 1) * sizeof(int));
        line_count -= rm;
    }
    syn_valid_lines = MIN(syn_valid_lines, cursor_line);
    sync_required = true;
    p8_editor_mark_modified();
    clear_selection();
}

static void insert_text_at_cursor(const char *text)
{
    if (!text || !*text) return;
    for (const char *p = text; *p; ) {
        const char *nl = strchr(p, '\n');
        int slen = nl ? (int)(nl - p) : (int)strlen(p);
        int old_len = line_lengths[cursor_line];
        lines[cursor_line] = realloc(lines[cursor_line], old_len + slen + 1);
        memmove(lines[cursor_line] + cursor_col + slen,
                lines[cursor_line] + cursor_col, old_len - cursor_col + 1);
        memcpy(lines[cursor_line] + cursor_col, p, slen);
        line_lengths[cursor_line] = old_len + slen;
        cursor_col += slen;
        syn_valid_lines = MIN(syn_valid_lines, cursor_line);
        if (nl) {
            line_count++;
            lines = realloc(lines, line_count * sizeof(char *));
            line_lengths = realloc(line_lengths, line_count * sizeof(int));
            memmove(&lines[cursor_line + 2], &lines[cursor_line + 1],
                    (line_count - cursor_line - 2) * sizeof(char *));
            memmove(&line_lengths[cursor_line + 2], &line_lengths[cursor_line + 1],
                    (line_count - cursor_line - 2) * sizeof(int));
            lines[cursor_line + 1] = strdup(&lines[cursor_line][cursor_col]);
            line_lengths[cursor_line + 1] = line_lengths[cursor_line] - cursor_col;
            lines[cursor_line][cursor_col] = '\0';
            line_lengths[cursor_line] = cursor_col;
            cursor_line++;
            cursor_col = 0;
            p = nl + 1;
        } else {
            break;
        }
    }
    sync_required = true;
    p8_editor_mark_modified();
}

/* --- Undo / redo ----------------------------------------------------------- */

static void push_undo(void)
{
    char *snapshot = NULL;
    join_lines(&snapshot);
    if (!snapshot) return;
    if (undo_depth == UNDO_LIMIT) {
        free(undo_stack[0]);
        memmove(undo_stack, undo_stack + 1, (UNDO_LIMIT - 1) * sizeof(char *));
        undo_depth--;
    }
    undo_stack[undo_depth++] = snapshot;
    for (int i = 0; i < redo_depth; i++) { free(redo_stack[i]); redo_stack[i] = NULL; }
    redo_depth = 0;
}

static void restore_snapshot(char *snapshot)
{
    int save_line = cursor_line;
    int save_col  = cursor_col;
    free_lines();
    split_lines(snapshot);
    free(snapshot);
    syn_valid_lines = 0;
    cursor_line = (save_line < line_count) ? save_line : line_count - 1;
    if (cursor_line < 0) cursor_line = 0;
    cursor_col  = (save_col <= line_lengths[cursor_line]) ? save_col
                                                           : line_lengths[cursor_line];
}

static void do_undo(void)
{
    if (undo_depth == 0) return;
    char *cur = NULL; join_lines(&cur);
    if (cur) {
        if (redo_depth == UNDO_LIMIT) {
            free(redo_stack[0]);
            memmove(redo_stack, redo_stack + 1, (UNDO_LIMIT - 1) * sizeof(char *));
            redo_depth--;
        }
        redo_stack[redo_depth++] = cur;
    }
    restore_snapshot(undo_stack[--undo_depth]);
    undo_stack[undo_depth] = NULL;
}

static void do_redo(void)
{
    if (redo_depth == 0) return;
    char *cur = NULL; join_lines(&cur);
    if (cur) {
        if (undo_depth == UNDO_LIMIT) {
            free(undo_stack[0]);
            memmove(undo_stack, undo_stack + 1, (UNDO_LIMIT - 1) * sizeof(char *));
            undo_depth--;
        }
        undo_stack[undo_depth++] = cur;
    }
    restore_snapshot(redo_stack[--redo_depth]);
    redo_stack[redo_depth] = NULL;
}

/* --- Search ---------------------------------------------------------------- */

static void find_next(void)
{
    if (search_str[0] == '\0') return;
    int slen = (int)strlen(search_str);
    /* Start searching after current cursor position */
    for (int l = cursor_line; l < line_count; l++) {
        int c_start = (l == cursor_line) ? cursor_col + 1 : 0;
        for (int c = c_start; c + slen <= line_lengths[l]; c++) {
            if (memcmp(lines[l] + c, search_str, (size_t)slen) == 0) {
                cursor_line       = l;
                cursor_col        = c + slen;
                select_start_line = l;
                select_start_col  = c;
                select_end_line   = l;
                select_end_col    = c + slen;
                return;
            }
        }
    }
}

/* --- Word movement --------------------------------------------------------- */

static void move_word_left(void)
{
    if (cursor_col == 0) {
        if (cursor_line > 0) { cursor_line--; cursor_col = line_lengths[cursor_line]; }
        return;
    }
    const char *s = lines[cursor_line];
    int c = cursor_col;
    while (c > 0 && s[c - 1] == ' ') c--;
    while (c > 0 && s[c - 1] != ' ') c--;
    cursor_col = c;
}

static void move_word_right(void)
{
    const char *s = lines[cursor_line];
    int len = line_lengths[cursor_line];
    int c = cursor_col;
    if (c >= len) {
        if (cursor_line < line_count - 1) { cursor_line++; cursor_col = 0; }
        return;
    }
    while (c < len && s[c] != ' ') c++;
    while (c < len && s[c] == ' ') c++;
    cursor_col = c;
}

/* --- Duplicate line -------------------------------------------------------- */

static void duplicate_line(void)
{
    line_count++;
    lines = realloc(lines, line_count * sizeof(char *));
    line_lengths = realloc(line_lengths, line_count * sizeof(int));
    memmove(&lines[cursor_line + 2], &lines[cursor_line + 1],
            (line_count - cursor_line - 2) * sizeof(char *));
    memmove(&line_lengths[cursor_line + 2], &line_lengths[cursor_line + 1],
            (line_count - cursor_line - 2) * sizeof(int));
    lines[cursor_line + 1] = strdup(lines[cursor_line]);
    line_lengths[cursor_line + 1] = line_lengths[cursor_line];
    cursor_line++;
    syn_valid_lines = MIN(syn_valid_lines, cursor_line);
    sync_required = true;
    p8_editor_mark_modified();
}

/* --- Comment / uncomment --------------------------------------------------- */

static void comment_uncomment(void)
{
    int sl = (select_start_line != -1) ? select_start_line : cursor_line;
    int el = (select_end_line   != -1) ? select_end_line   : cursor_line;
    bool all = true;
    for (int l = sl; l <= el; l++)
        if (line_lengths[l] < 2 || lines[l][0] != '-' || lines[l][1] != '-')
            { all = false; break; }
    for (int l = sl; l <= el; l++) {
        if (all) {
            if (line_lengths[l] >= 2) {
                memmove(lines[l], lines[l] + 2, line_lengths[l] - 1);
                line_lengths[l] -= 2;
            }
        } else {
            lines[l] = realloc(lines[l], line_lengths[l] + 3);
            memmove(lines[l] + 2, lines[l], line_lengths[l] + 1);
            lines[l][0] = '-'; lines[l][1] = '-';
            line_lengths[l] += 2;
        }
    }
    syn_valid_lines = MIN(syn_valid_lines, sl);
    sync_required = true;
    p8_editor_mark_modified();
}

/* --- Indent / unindent ----------------------------------------------------- */

static void indent_lines(bool unindent)
{
    int sl = (select_start_line != -1) ? select_start_line : cursor_line;
    int el = (select_end_line   != -1) ? select_end_line   : cursor_line;
    for (int l = sl; l <= el; l++) {
        if (!unindent) {
            lines[l] = realloc(lines[l], line_lengths[l] + 2);
            memmove(lines[l] + 1, lines[l], line_lengths[l] + 1);
            lines[l][0] = ' ';
            line_lengths[l]++;
        } else if (line_lengths[l] > 0 && lines[l][0] == ' ') {
            memmove(lines[l], lines[l] + 1, line_lengths[l]);
            line_lengths[l]--;
        }
    }
    if (!unindent) {
        if (select_start_col != -1) select_start_col = MIN(select_start_col + 1, line_lengths[sl]);
        if (select_end_col   != -1) select_end_col   = MIN(select_end_col   + 1, line_lengths[el]);
        cursor_col = MIN(cursor_col + 1, line_lengths[cursor_line]);
    } else {
        if (select_start_col > 0) select_start_col--;
        if (select_end_col   > 0) select_end_col--;
        if (cursor_col > line_lengths[cursor_line]) cursor_col = line_lengths[cursor_line];
    }
    syn_valid_lines = MIN(syn_valid_lines, sl);
    sync_required = true;
    p8_editor_mark_modified();
}

/* Draw one line of source text with syntax highlighting.
 *
 * syn_colors: per-character PICO-8 colour index array of length `len`
 *             (may be NULL to use the plain default colour).
 * len:        strlen(str).
 * Cursor and selection colours always override syntax colours. */
static inline void draw_line(const p8_dialog_t *dialog,
                             const char *str, int len,
                             const uint8_t *syn_colors,
                             int x, int y,
                             int cursor_col,
                             int select_start_col, int select_end_col)
{
    int cursor_x = x;
    int col = 0;
    for (const char *c = str; ; c++, col++) {
        int glyph_width = ((uint8_t)*c >= 0x80) ? GLYPH_WIDTH * 2 : GLYPH_WIDTH;
        int fg, bg;
        if (col == cursor_col && (dialog->cursor_blink & 8)) {
            fg = EDITOR_TEXT_HIGHLIGHT;
            bg = EDITOR_BG_CURSOR;
        } else if (select_start_col != -1 && select_end_col != -1 &&
                   col >= select_start_col && col < select_end_col) {
            fg = EDITOR_TEXT_HIGHLIGHT;
            bg = EDITOR_BG_HIGHLIGHT;
        } else {
            fg = (syn_colors && col < len) ? (int)syn_colors[col] : EDITOR_TEXT_NORMAL;
            bg = EDITOR_BG_NORMAL;
        }
        if (bg != EDITOR_BG_NORMAL)
            overlay_draw_rectfill(cursor_x, y, cursor_x + glyph_width - (col == cursor_col?2:1), y + GLYPH_HEIGHT - 1, bg);
        if (*c == '\0')
            break;
        overlay_draw_char((uint8_t)*c, cursor_x, y, fg);
        cursor_x += glyph_width;
    }
}

static void code_draw(const p8_dialog_t *dialog, void *user_data, int x, int y, int width, int height)
{
    /* Reserve one row at the bottom for the status bar. */
    int lines_per_page = (height - GLYPH_HEIGHT) / GLYPH_HEIGHT;
    int clip_x, clip_y, clip_w, clip_h;

    /* Extend colour cache to cover all lines if it is out of date. */
    if (syn_valid_lines < line_count)
        syn_extend_cache();

    overlay_clip_get(&clip_x, &clip_y, &clip_w, &clip_h);
    overlay_clip_set(x, y + 1, width, height - GLYPH_HEIGHT - 1);

    for (int i = 0; i < lines_per_page && i + scroll_offset_y < line_count; i++) {
        int line = i + scroll_offset_y;
        draw_line(dialog, lines[line], line_lengths[line],
                  (syn_color_cache && line < syn_valid_lines) ? syn_color_cache[line] : NULL,
                  x - scroll_offset_x, y + 1 + i * GLYPH_HEIGHT,
                  (line == cursor_line) ? cursor_col : -1,
                  (line == select_start_line) ? select_start_col
                      : ((line > select_start_line && line <= select_end_line) ? 0 : -1),
                  (line == select_end_line) ? select_end_col
                      : ((line >= select_start_line && line < select_end_line) ? INT_MAX : -1));
    }

    overlay_clip_set(clip_x, clip_y, clip_w, clip_h);

    /* Status bar: last row of the content area. */
    {
        int sy = y + height - GLYPH_HEIGHT;
        char status[32];
        snprintf(status, sizeof(status), "ln:%d col:%d",
                 cursor_line + 1, cursor_col + 1);
        overlay_draw_rectfill(x, sy, x + width - 1, sy + GLYPH_HEIGHT - 1, EDITOR_BG_HIGHLIGHT);
        overlay_draw_simple_text(status, x, sy, EDITOR_TEXT_HIGHLIGHT);
    }
}

static void code_handle_keypress(int scancode, int keypress, int keymod)
{
    const int width = P8_WIDTH;
    const int height = P8_HEIGHT - GLYPH_HEIGHT - 1;
    const int lines_per_page = (height - GLYPH_HEIGHT) / GLYPH_HEIGHT;
    int old_cursor_col = cursor_col;
    int old_cursor_line = cursor_line;
    bool shift = (keymod & KMOD_SHIFT) != 0;

#define BEGIN_SHIFT_MOVE() do { if (shift && select_anchor_line == -1) { select_anchor_line = cursor_line; select_anchor_col = cursor_col; } } while (0)
#define END_SHIFT_MOVE()   do { if (shift) update_selection(); else clear_selection(); } while (0)

    switch (scancode) {
        case SCANCODE_UP:
            BEGIN_SHIFT_MOVE();
            if (keymod & KMOD_CTRL) { cursor_line = 0; cursor_col = 0; }
            else if (keymod & KMOD_ALT) {
                /* Jump to previous function definition */
                for (int l = cursor_line - 1; l >= 0; l--) {
                    if (strncmp(lines[l], "function ", 9) == 0 ||
                        strncmp(lines[l], "local function ", 15) == 0) {
                        cursor_line = l; cursor_col = 0; break;
                    }
                }
            }
            else if (cursor_line > 0) cursor_line--;
            END_SHIFT_MOVE();
            break;
        case SCANCODE_DOWN:
            BEGIN_SHIFT_MOVE();
            if (keymod & KMOD_CTRL) { cursor_line = line_count - 1; cursor_col = line_lengths[cursor_line]; }
            else if (keymod & KMOD_ALT) {
                /* Jump to next function definition */
                for (int l = cursor_line + 1; l < line_count; l++) {
                    if (strncmp(lines[l], "function ", 9) == 0 ||
                        strncmp(lines[l], "local function ", 15) == 0) {
                        cursor_line = l; cursor_col = 0; break;
                    }
                }
            }
            else if (cursor_line < line_count - 1) cursor_line++;
            END_SHIFT_MOVE();
            break;
        case SCANCODE_LEFT:
            BEGIN_SHIFT_MOVE();
            if (keymod & KMOD_CTRL) { move_word_left(); }
            else if (cursor_col > 0) { cursor_col--; }
            else if (cursor_line > 0) { cursor_line--; cursor_col = line_lengths[cursor_line]; }
            END_SHIFT_MOVE();
            break;
        case SCANCODE_RIGHT:
            BEGIN_SHIFT_MOVE();
            if (keymod & KMOD_CTRL) { move_word_right(); }
            else if (cursor_col < line_lengths[cursor_line]) { cursor_col++; }
            else if (cursor_line < line_count - 1) { cursor_line++; cursor_col = 0; }
            END_SHIFT_MOVE();
            break;
        case SCANCODE_PAGEUP:
            BEGIN_SHIFT_MOVE();
            cursor_line = MAX(cursor_line - MAX(lines_per_page - 1, 1), 0);
            END_SHIFT_MOVE();
            break;
        case SCANCODE_PAGEDOWN:
            BEGIN_SHIFT_MOVE();
            cursor_line = MIN(cursor_line + MAX(lines_per_page - 1, 1), line_count - 1);
            END_SHIFT_MOVE();
            break;
        case SCANCODE_HOME:
            BEGIN_SHIFT_MOVE();
            cursor_col = 0;
            scroll_offset_x = 0;
            END_SHIFT_MOVE();
            break;
        case SCANCODE_END:
            BEGIN_SHIFT_MOVE();
            cursor_col = line_lengths[cursor_line];
            END_SHIFT_MOVE();
            break;
        case SCANCODE_DEL:
            // Delete forward
            if (select_start_line != -1) {
                push_undo(); delete_selected_text();
            } else if (cursor_col < line_lengths[cursor_line]) {
                push_undo();
                memmove(&lines[cursor_line][cursor_col],
                        &lines[cursor_line][cursor_col + 1],
                        line_lengths[cursor_line] - cursor_col);
                line_lengths[cursor_line]--;
                syn_valid_lines = MIN(syn_valid_lines, cursor_line);
            } else if (cursor_line < line_count - 1) {
                push_undo();
                lines[cursor_line] = realloc(lines[cursor_line],
                    line_lengths[cursor_line] + line_lengths[cursor_line + 1] + 1);
                memmove(&lines[cursor_line][line_lengths[cursor_line]],
                        lines[cursor_line + 1], line_lengths[cursor_line + 1] + 1);
                line_lengths[cursor_line] += line_lengths[cursor_line + 1];
                memmove(&lines[cursor_line + 1], &lines[cursor_line + 2],
                        (line_count - cursor_line - 2) * sizeof(char *));
                memmove(&line_lengths[cursor_line + 1], &line_lengths[cursor_line + 2],
                        (line_count - cursor_line - 2) * sizeof(int));
                line_count--;
                syn_valid_lines = MIN(syn_valid_lines, cursor_line);
            }
            sync_required = true;
            p8_editor_mark_modified();
            break;
        default:
            if (keymod & KMOD_CTRL) {
                switch (scancode) {
                    case SCANCODE_P: puny_mode = !puny_mode; break;
                    case SCANCODE_W: /* start of line */
                        clear_selection(); cursor_col = 0; scroll_offset_x = 0;
                        break;
                    case SCANCODE_E: /* end of line */
                        clear_selection(); cursor_col = line_lengths[cursor_line];
                        break;
                    case SCANCODE_D: /* duplicate line */
                        push_undo(); duplicate_line(); break;
                    case SCANCODE_B: /* comment / uncomment */
                        push_undo(); comment_uncomment(); break;
                    case SCANCODE_X: { /* cut */
                        char *sel = get_selected_text();
                        if (sel) { push_undo(); free(clipboard); clipboard = sel; delete_selected_text(); }
                        break; }
                    case SCANCODE_C: { /* copy */
                        char *sel = get_selected_text();
                        if (sel) { free(clipboard); clipboard = sel; }
                        break; }
                    case SCANCODE_V: /* paste */
                        if (clipboard) {
                            push_undo();
                            if (select_start_line != -1) delete_selected_text();
                            insert_text_at_cursor(clipboard);
                        }
                        break;
                    case SCANCODE_Z: do_undo(); break;
                    case SCANCODE_Y: do_redo(); break;
                    case SCANCODE_F: { /* find */
                        p8_dialog_t dlg;
                        p8_dialog_control_t ctrls[] = {
                            DIALOG_INPUTBOX("find", search_str, sizeof(search_str)),
                            DIALOG_SPACING(), DIALOG_BUTTONBAR(),
                        };
                        p8_dialog_init(&dlg, "find", ctrls, 3, 100);
                        p8_dialog_action_t res = p8_dialog_run(&dlg);
                        p8_dialog_cleanup(&dlg);
                        if (res.type == DIALOG_RESULT_ACCEPTED) find_next();
                        break; }
                    case SCANCODE_G: find_next(); break; /* find next */
                    case SCANCODE_L: { /* goto line */
                        char linebuf[8] = "";
                        p8_dialog_t dlg;
                        p8_dialog_control_t ctrls[] = {
                            DIALOG_INPUTBOX("line", linebuf, sizeof(linebuf)),
                            DIALOG_SPACING(), DIALOG_BUTTONBAR(),
                        };
                        p8_dialog_init(&dlg, "goto line", ctrls, 3, 80);
                        p8_dialog_action_t res = p8_dialog_run(&dlg);
                        p8_dialog_cleanup(&dlg);
                        if (res.type == DIALOG_RESULT_ACCEPTED) {
                            int line = atoi(linebuf);
                            if (line < 1) line = 1;
                            if (line > line_count) line = line_count;
                            cursor_line = line - 1;
                            if (cursor_col > line_lengths[cursor_line])
                                cursor_col = line_lengths[cursor_line];
                        }
                        break; }
                    default:
                        goto text_input;
                }
            } else {
text_input:;
                if (scancode == SCANCODE_TAB) {
                    push_undo();
                    indent_lines(shift);
                    break;
                }
                switch (keypress) {
                    case 8: /* backspace */
                        if (select_start_line != -1) {
                            push_undo(); delete_selected_text();
                        } else if (cursor_col > 0) {
                            push_undo();
                            memmove(&lines[cursor_line][cursor_col - 1],
                                    &lines[cursor_line][cursor_col],
                                    line_lengths[cursor_line] - cursor_col + 1);
                            cursor_col--;
                            line_lengths[cursor_line]--;
                            syn_valid_lines = MIN(syn_valid_lines, cursor_line);
                        } else if (cursor_line > 0) {
                            push_undo();
                            cursor_col = line_lengths[cursor_line - 1];
                            lines[cursor_line - 1] = realloc(lines[cursor_line - 1],
                                line_lengths[cursor_line - 1] + line_lengths[cursor_line] + 1);
                            memmove(&lines[cursor_line - 1][line_lengths[cursor_line - 1]],
                                    lines[cursor_line], line_lengths[cursor_line] + 1);
                            line_lengths[cursor_line - 1] += line_lengths[cursor_line];
                            memmove(&lines[cursor_line], &lines[cursor_line + 1],
                                    (line_count - cursor_line - 1) * sizeof(char *));
                            memmove(&line_lengths[cursor_line], &line_lengths[cursor_line + 1],
                                    (line_count - cursor_line - 1) * sizeof(int));
                            line_count--;
                            cursor_line--;
                            syn_valid_lines = MIN(syn_valid_lines, cursor_line);
                        }
                        sync_required = true;
                        p8_editor_mark_modified();
                        break;
                    case '\n': /* newline */
                    case 13: /* return */
                        clear_selection();
                        push_undo();
                        line_count++;
                        lines = realloc(lines, line_count * sizeof(char *));
                        line_lengths = realloc(line_lengths, line_count * sizeof(int));
                        memmove(&lines[cursor_line + 2], &lines[cursor_line + 1],
                                (line_count - cursor_line - 2) * sizeof(char *));
                        memmove(&line_lengths[cursor_line + 2], &line_lengths[cursor_line + 1],
                                (line_count - cursor_line - 2) * sizeof(int));
                        lines[cursor_line + 1] = strdup(&lines[cursor_line][cursor_col]);
                        line_lengths[cursor_line + 1] = line_lengths[cursor_line] - cursor_col;
                        lines[cursor_line][cursor_col] = '\0';
                        line_lengths[cursor_line] = cursor_col;
                        cursor_line++;
                        cursor_col = 0;
                        syn_valid_lines = MIN(syn_valid_lines, cursor_line - 1);
                        sync_required = true;
                        p8_editor_mark_modified();
                        break;
                    default:
                        if (keypress >= 32) {
                            push_undo();
                            if (select_start_line != -1) delete_selected_text();
                            if (keypress >= 'A' && keypress <= 'Z')
                                keypress = keypress - 'A' + 128;
                            else if (keypress >= 'a' && keypress <= 'z' && puny_mode)
                                keypress = keypress - 'a' + 'A';
                            line_lengths[cursor_line]++;
                            lines[cursor_line] = realloc(lines[cursor_line],
                                                         line_lengths[cursor_line] + 1);
                            memmove(&lines[cursor_line][cursor_col + 1],
                                    &lines[cursor_line][cursor_col],
                                    line_lengths[cursor_line] - cursor_col);
                            lines[cursor_line][cursor_col] = (char)keypress;
                            cursor_col++;
                            syn_valid_lines = MIN(syn_valid_lines, cursor_line);
                            sync_required = true;
                            p8_editor_mark_modified();
                        }
                }
            }
    }

#undef BEGIN_SHIFT_MOVE
#undef END_SHIFT_MOVE

    if (old_cursor_line != cursor_line) {
        if (cursor_col > line_lengths[cursor_line])
            cursor_col = line_lengths[cursor_line];
        if (cursor_line < scroll_offset_y + 4)
            scroll_offset_y = MAX(MIN(cursor_line - 4, line_count - lines_per_page), 0);
        if (cursor_line >= scroll_offset_y + lines_per_page - 4)
            scroll_offset_y = MAX(cursor_line - lines_per_page + 5, 0);
    }
    if (old_cursor_col != cursor_col) {
        int cursor_x = overlay_get_text_width_len(lines[cursor_line], cursor_col);
        if (cursor_x < scroll_offset_x + GLYPH_WIDTH * 4)
            scroll_offset_x = MAX(cursor_x - GLYPH_WIDTH * 4, 0);
        if (cursor_x >= scroll_offset_x + width - GLYPH_WIDTH * 4)
            scroll_offset_x = MAX(cursor_x - width + GLYPH_WIDTH * 4, 0);
    }
}

/* --- Mouse handler --------------------------------------------------------- */

/* Map a logical x-pixel position (scroll-adjusted) to the nearest column. */
static int col_from_logical_x(int line_idx, int logical_x)
{
    const char *s = lines[line_idx];
    int len = line_lengths[line_idx];
    int x = 0;
    for (int c = 0; c < len; c++) {
        int w = ((uint8_t)s[c] >= 0x80) ? GLYPH_WIDTH * 2 : GLYPH_WIDTH;
        if (x + w > logical_x)
            return c;
        x += w;
    }
    return len;
}

static void code_handle_mouse(int mx, int my, int buttons, int buttonsp, int keymod)
{
    /* Mousewheel scrolls vertically (3 lines per tick). */
    if (m_mouse_wheel != 0) {
        scroll_offset_y -= m_mouse_wheel * 3;
        if (scroll_offset_y < 0) scroll_offset_y = 0;
        if (scroll_offset_y >= line_count) scroll_offset_y = line_count - 1;
    }

    int lmb_pressed = (buttonsp & 1);
    int lmb_held    = buttons & 1;

    if (!lmb_pressed && !lmb_held)
        return;

    /* draw_editor is called with y = GLYPH_HEIGHT (content area origin).
     * Lines are rendered at screen y = GLYPH_HEIGHT + 1 + i * GLYPH_HEIGHT. */
    int content_y_start = GLYPH_HEIGHT + 1;  /* first pixel of line 0 = 7 */
    if (my < content_y_start)
        return;  /* click in title-bar area */

    int line_on_screen = (my - content_y_start) / GLYPH_HEIGHT;
    int clicked_line   = scroll_offset_y + line_on_screen;
    if (clicked_line >= line_count) clicked_line = line_count - 1;
    if (clicked_line < 0)          clicked_line = 0;

    /* Logical x = screen x + horizontal scroll offset */
    int col = col_from_logical_x(clicked_line, mx + scroll_offset_x);

    if (lmb_pressed) {
        /* Fresh click: move cursor, start a new drag anchor */
        cursor_line = clicked_line;
        cursor_col  = col;
        clear_selection();
        select_anchor_line = clicked_line;
        select_anchor_col  = col;
    } else if (lmb_held && select_anchor_line >= 0) {
        /* Drag: extend selection from anchor to current position */
        cursor_line = clicked_line;
        cursor_col  = col;
        update_selection();
    }

    return;
}

static void code_show(void)
{
    if (!lines) {
        assert(m_lua_script);
        split_lines(m_lua_script);
        scroll_offset_x = 0;
        scroll_offset_y = 0;
        cursor_line = 0;
        cursor_col = 0;
        select_start_line = -1;
        select_start_col  = -1;
        select_end_line   = -1;
        select_end_col    = -1;
        select_anchor_line = -1;
        select_anchor_col  = -1;
    }
    showing = true;
}

static void code_hide(void)
{
    showing = false;
}

static void code_init(void)
{
    assert(lines == NULL);
    assert(line_lengths == NULL);
    assert(line_count == 0);
    assert(clipboard == NULL);
    assert(syn_color_cache == NULL);
    assert(syn_color_cache_cap == NULL);
    assert(syntax_contexts == NULL);
    assert(syn_valid_lines == 0);
    assert(syn_cache_alloc == 0);
    assert(redo_depth == 0);
    assert(undo_depth == 0);

    scroll_offset_x = 0;
    scroll_offset_y = 0;
    cursor_line = 0;
    cursor_col = 0;
    select_start_line = -1;
    select_start_col  = -1;
    select_end_line   = -1;
    select_end_col    = -1;
    puny_mode = false;
    sync_required = false;
}

static void code_shutdown(void)
{
    free_lines();
    free(clipboard);
    clipboard = NULL;
    for (int i = 0; i < redo_depth; i++) { free(redo_stack[i]); redo_stack[i] = NULL; }
    redo_depth = 0;
    for (int i = 0; i < undo_depth; i++) { free(undo_stack[i]); undo_stack[i] = NULL; }
    undo_depth = 0;
}

static void code_invalidate(void)
{
    free_lines();
    if (showing)
        code_show();
}

static void code_sync(void)
{
    if (sync_required) {
        join_lines(&m_lua_script);
        sync_required = false;
    }
}

/* --- Public API --- */

void p8_editor_code_set_line(int line)
{
    if (line < 1) line = 1;
    if (line > line_count) line = line_count;
    if (cursor_line != line) {
        cursor_line = line - 1;
        cursor_col = 0;
        scroll_offset_x = 0;
        const int height = P8_HEIGHT - GLYPH_HEIGHT - 1;
        const int lines_per_page = (height - GLYPH_HEIGHT) / GLYPH_HEIGHT;
        if (cursor_line < scroll_offset_y + 4)
            scroll_offset_y = MAX(MIN(cursor_line - 4, line_count - lines_per_page), 0);
        if (cursor_line >= scroll_offset_y + lines_per_page - 4)
            scroll_offset_y = MAX(cursor_line - lines_per_page + 5, 0);
    }
}

p8_editor_tab_t p8_subeditor_code = {
    "cod",
    .init=code_init,
    .shutdown=code_shutdown,
    .show=code_show,
    .hide=code_hide,
    .draw=code_draw,
    .sync=code_sync,
    .invalidate=code_invalidate,
    .handle_keypress=code_handle_keypress,
    .handle_mouse=code_handle_mouse
};
