#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#include "p8_dialog.h"
#include "p8_editor_code.h"
#include "p8_emu.h"
#include "p8_input.h"
#include "p8_lua.h"
#include "p8_browse.h"
#include "p8_overlay_helper.h"
#include "p8_print_helper.h"
#include "p8_repl.h"
#include "strtcpy.h"
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>

/* =========================================================================
 * CONSTANTS
 * ========================================================================= */

#define SCANCODE_UP       82
#define SCANCODE_DOWN     81

#define REPL_HISTORY_MAX   32          /* number of history lines to keep    */
#define REPL_LINE_MAX      96          /* max chars per history line         */
#define REPL_INPUT_MAX     96          /* max chars in input buffer          */
#define REPL_VISIBLE_ROWS  14          /* rows of history visible at once    */

#define COLOUR_COMMAND    10
#define COLOUR_ERROR_TYPE 14
#define COLOUR_ERROR_MSG  7
#define COLOUR_DIR        12

/* =========================================================================
 * STATE
 * ========================================================================= */

static char repl_history[REPL_HISTORY_MAX][REPL_LINE_MAX];
static int  repl_history_count = 0;
static int  repl_history_cursor = 0;

static char repl_input[REPL_INPUT_MAX + 4];  /* +4: room for (, ", ", ) added by converter */
static int  repl_input_len  = 0;
static int  repl_cursor_blink = 0;

/* =========================================================================
 * HELPERS
 * ========================================================================= */

static void repl_history_push(const char *line)
{
    if (repl_history_count == REPL_HISTORY_MAX) {
        /* Scroll up */
        memmove(repl_history[0], repl_history[1],
                (REPL_HISTORY_MAX - 1) * REPL_LINE_MAX);
        repl_history_count--;
    }
    strtcpy(repl_history[repl_history_count], line, REPL_LINE_MAX);
    repl_history[repl_history_count][REPL_LINE_MAX - 1] = '\0';
    repl_history_count++;
    repl_history_cursor = repl_history_count; /* reset cursor to end of history */
}

/* =========================================================================
 * DRAW
 * ========================================================================= */

static void repl_print(const char *line, int colour, bool add_newline)
{
    int x, y, right;
    cursor_get(&x, &y);
    uint8_t old_flags = m_memory[MEMORY_MISCFLAGS];
    m_memory[MEMORY_MISCFLAGS] = 0x80 | (add_newline ? 0 : 0x4);
    draw_text(line, strlen(line), x, y, colour, 0, false, &x, &y, &right);
    m_memory[MEMORY_MISCFLAGS] = old_flags;
    cursor_set(x, y, -1);
}

static void repl_draw(int x, int y, int w, int h)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;

    overlay_clear();

    /* Input line at bottom */
    int input_y = y + h - GLYPH_HEIGHT;
    overlay_draw_text(">", 0, input_y, COLOUR_COMMAND);
    int cursor_x = overlay_draw_text(repl_input, GLYPH_WIDTH, input_y, COLOUR_COMMAND);

    /* Blinking cursor */
    if (repl_cursor_blink & 8) {
        overlay_draw_rectfill(cursor_x + 1, input_y, cursor_x + GLYPH_WIDTH,
                              input_y + GLYPH_HEIGHT - 1, 10);
    }
}

/* =========================================================================
 * EXECUTE
 * ========================================================================= */

 /** Return true if the input looks like a command (i.e. starts with a valid identifier
 *   followed by optional whitespace and then no open parantheses). */
static bool is_command(const char *input, size_t input_len)
{
    if (!((input[0] >= 'a' && input[0] <= 'z') ||
         (input[0] >= 'A' && input[0] <= 'Z') ||
          input[0] == '_'))
        return false;
    int i = 0;
    for (i=1;i<input_len;i++) {
        if (!((input[i] >= 'a' && input[i] <= 'z') ||
              (input[i] >= 'A' && input[i] <= 'Z') ||
              (input[i] >= '0' && input[i] <= '9') ||
              input[i] == '_')) {
            break;
        }
    }
    for (;i<input_len;i++) {
        if (input[i] != ' ' && input[i] != '\t')
            break;
    }
    if (input[i] == '(')
        return false;
    return true;
}

/** Return true if the first word of input is a command whose bare arguments
 *  should be auto-quoted as a string, e.g. "mkdir blah" -> "mkdir(\"blah\")". */
static bool is_string_arg_command(const char *input, size_t input_len)
{
    static const char * const cmds[] = {"ls", "cd", "mkdir", "save", "load", "reboot", NULL};
    int i;
    for (i = 0; i < (int)input_len; i++) {
        if (input[i] == ' ' || input[i] == '\t')
            break;
    }
    for (int j = 0; cmds[j]; j++) {
        if ((int)strlen(cmds[j]) == i && strncmp(input, cmds[j], i) == 0)
            return true;
    }
    return false;
}

static void repl_draw_file_name(const char *str, int x, int y, int col)
{
    int cursor_x = x;
    for (const char *c = str; *c != '\0'; c++) {
        char display_char;
        if (*c >= 'a' && *c <= 'z') {
             display_char = *c - ('a' - 'A');
        } else if (*c >= 'A' && *c <= 'Z') {
            display_char = *c + ('a' - 'A');
        } else if (*c >= 0x20 && *c <= 0x7F) {
            display_char = *c;
        } else {
            display_char = '?';
        }
        draw_char(display_char, cursor_x, y, col);
        cursor_x += GLYPH_WIDTH;
    }
}

/* Small helper used to collect directory entries for sorting. */
typedef struct {
    char *name;
    bool is_dir;
} repl_dir_entry_t;

static int repl_dir_entry_cmp(const void *pa, const void *pb)
{
    const repl_dir_entry_t *a = pa;
    const repl_dir_entry_t *b = pb;
    if (a->is_dir != b->is_dir)
        return a->is_dir ? -1 : 1;
    return strcmp(a->name, b->name);
}

/* Render a directory listing into the REPL overlay with pagination.
 * `arg` is the optional path argument (may be NULL or empty).
 */
void repl_handle_ls(const char *arg)
{
    char resolved[PATH_MAX];
    const char *path;
    if (arg && arg[0]) {
        if (p8_resolve_relative_path(resolved, arg, sizeof(resolved), false) < 0) {
            repl_print("path too long", COLOUR_ERROR_MSG, true);
            return;
        }
        path = resolved;
    } else {
        path = m_current_cart_dir[0] ? m_current_cart_dir : ".";
    }

    DIR *d = opendir(path);
    if (!d) {
        repl_print(path, COLOUR_ERROR_MSG, false);
        repl_print(": ", COLOUR_ERROR_TYPE, false);
        repl_print(strerror(errno), COLOUR_ERROR_MSG, true);
        return;
    }

    /* Leave room for the input line and the 'press any key' prompt at the bottom.
     * Calculate how many text lines fit on the main screen area and reserve two
     * rows (top and bottom) to avoid overlap. */
    int lines_per_page = (P8_HEIGHT / GLYPH_HEIGHT) - 2;
    if (lines_per_page < 4) lines_per_page = 4;

    struct dirent *de;
    int printed = 0;
    char fullpath[PATH_MAX];
    int x, y;
    cursor_get(&x, &y);
    y = scroll(y, GLYPH_HEIGHT);

    /* Collect entries into an array, sort dirs first then names, then render. */
    repl_dir_entry_t *entries = NULL;
    int nentries = 0, cap = 0;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        if (p8_make_full_path(fullpath, sizeof(fullpath), path, de->d_name) < 0)
            continue;
        struct stat st;
        bool is_dir = stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode);
        if (nentries >= cap) {
            int newcap = cap == 0 ? 64 : cap * 2;
            repl_dir_entry_t *tmp = realloc(entries, newcap * sizeof(entries[0]));
            if (!tmp) break;
            entries = tmp;
            cap = newcap;
        }
        entries[nentries].name = strdup(de->d_name);
        if (!entries[nentries].name) break;
        entries[nentries].is_dir = is_dir;
        nentries++;
    }

    if (nentries > 1)
        qsort(entries, nentries, sizeof(entries[0]), repl_dir_entry_cmp);

    for (int ei = 0; ei < nentries; ++ei) {
        repl_draw_file_name(entries[ei].name, 0, y, entries[ei].is_dir ? COLOUR_DIR : 6);
        y += GLYPH_HEIGHT;
        y = scroll_newline(y);
        printed++;

        if (printed >= lines_per_page) {
            cursor_set(0, y, -1);
            p8_wait_for_any_key();
            cursor_get(&x, &y);
            y -= GLYPH_HEIGHT;
            printed = 0;
        }
    }
    cursor_set(0, y, -1);

    for (int ei = 0; ei < nentries; ++ei)
        free(entries[ei].name);
    free(entries);

    closedir(d);
    /* Ensure final page is presented */
    p8_flip();
}

static void convert_command_to_function_call(char *input, int *input_len)
{
    /* Convert command-looking input like "foo bar" to "foo(bar)" so that it can be executed as a Lua function call. */
    /* For shell commands (ls, cd, mkdir, save, load, reboot), the argument is auto-quoted as a string. */
    bool quote_args = is_string_arg_command(input, *input_len);

    int i = 0;
    bool first_space = true;
    for (i=0;i<=*input_len;i++) {
        if (i == *input_len || input[i] == ' ' || input[i] == '\t') {
            if (first_space) {
                input[i] = '(';
                first_space = false;
            } else {
                input[i] = ',';
            }
            if (i == *input_len)
                (*input_len)++;
            for (i=i+1;i<*input_len;i++) {
                if (input[i] != ' ' && input[i] != '\t')
                    break;
            }
            break;
        }
    }
    /* i now points to the start of the argument(s), or equals *input_len if no args */
    if (quote_args && i < *input_len) {
        /* Insert '"' before argument, append '"' then ')' */
        memmove(input + i + 1, input + i, *input_len - i);
        input[i] = '"';
        (*input_len)++;
        input[(*input_len)++] = '"';
        input[(*input_len)++] = ')';
    } else {
        input[(*input_len)++] = ')';
    }
    input[*input_len] = '\0';
}

static void repl_execute(void)
{
    if (repl_input_len == 0) return;

    overlay_clear();

    /* Push prompt line to history */
    repl_history_push(repl_input);
    repl_print(">", COLOUR_COMMAND, false);
    repl_print(repl_input, COLOUR_COMMAND, true);

    if (is_command(repl_input, repl_input_len))
        convert_command_to_function_call(repl_input, &repl_input_len);

    /* Run: only show errors, swallow success results */
    char err_msg[REPL_LINE_MAX];
    const char *err_type = NULL;
    const char *filename = NULL;
    int lineno = 0;
    err_msg[0] = '\0';
    int ret = p8_exec(repl_input, &err_type, err_msg, sizeof(err_msg), &filename, &lineno);
    if (ret != 0) {
        if (err_type) {
            if (lineno > 0) {
                repl_print(err_type, COLOUR_ERROR_TYPE, false);
                char buf[32];
                snprintf(buf, sizeof(buf), " line %d", lineno);
                repl_print(buf, COLOUR_ERROR_TYPE, true);
            } else {
                repl_print(err_type, COLOUR_ERROR_TYPE, true);
            }
        }
        if (err_msg[0])
            repl_print(err_msg, COLOUR_ERROR_MSG, true);
        if (lineno > 0)
            p8_editor_code_set_line(lineno);
    }

    /* Clear input */
    repl_input[0] = '\0';
    repl_input_len = 0;
}

/* =========================================================================
 * MAIN ENTRY
 * ========================================================================= */

static void repl_update(void)
{
    repl_cursor_blink++;
}

static void repl_handle_keypress(int scancode, int keypress, int keymod)
{
    (void)scancode;
    (void)keymod;

    if (scancode == SCANCODE_UP) {
        /* Recall previous history line */
        if (repl_history_cursor > 0) {
            // Cycle through previous history lines
            strcpy(repl_input, repl_history[--repl_history_cursor]);
            repl_input_len = strlen(repl_input);
        }
        return;
    } else if (scancode == SCANCODE_DOWN) {
        // Cycle back the other way
        if (repl_history_cursor < repl_history_count - 1) {
            strcpy(repl_input, repl_history[++repl_history_cursor]);
            repl_input_len = strlen(repl_input);
        } else {
            // Past the end of history: clear input
            repl_history_cursor = repl_history_count;
            repl_input[0] = '\0';
            repl_input_len = 0;
        }
        return;
    } else if (keypress == 13) { /* Enter */
        repl_execute();
    } else if (keypress == 8) { /* Backspace */
        if (repl_input_len > 0) {
            repl_input_len--;
            repl_input[repl_input_len] = '\0';
        }
    } else if (keypress >= 32 && keypress < 128) {
        /* Printable ASCII */
        if (repl_input_len < REPL_INPUT_MAX) {
            repl_input[repl_input_len++] = (char)keypress;
            repl_input[repl_input_len] = '\0';
        }
    }
}

static void repl_init(void)
{
    lua_load_api();
}

p8_screen_t p8_screen_repl = {
    .init = repl_init,
    .draw = repl_draw,
    .update = repl_update,
    .handle_keypress = repl_handle_keypress,
};
