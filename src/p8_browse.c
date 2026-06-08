/**
 * Copyright (C) 2025 Chris January
 */

#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#if defined(__APPLE__)
  #include <stdlib.h>
  #include <malloc/malloc.h>
#elif defined(__OpenBSD__)
  #include <stdlib.h>
#else
  #include <malloc.h>
#endif
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include "p8_browse.h"
#ifdef ENABLE_BBS_DOWNLOAD
#include "p8_cache.h"
#endif
#include "p8_dialog.h"
#include "p8_main.h"
#ifdef NEXTP8
#include "wifi/p8_wifi.h"
#include "wifi/p8_wifi_config.h"
#endif
#include "p8_emu.h"
#include "p8_lua_helper.h"
#include "p8_options.h"
#include "p8_overlay_helper.h"
#include "p8_pause_menu.h"
#include "p8_preview.h"
#include "strtcpy.h"

#ifdef NEXTP8
#define FALLBACK_CARTS_PATH "0:/"
#else
#define FALLBACK_CARTS_PATH "."
#endif

#define PREVIEW_LABEL_DISPLAY_SIZE 64
#define PREVIEW_DIALOG_CONTENT_WIDTH PREVIEW_LABEL_DISPLAY_SIZE
#define PREVIEW_BORDER_SIZE 6
#define PREVIEW_LOAD_DELAY_MS 250
#define PREVIEW_SHOW_DELAY_MS 500
#define PREVIEW_CHARS_PER_LINE (PREVIEW_DIALOG_CONTENT_WIDTH / GLYPH_WIDTH)

#define INITIAL_FILENAME_MEM_SIZE 8192
#define INITIAL_CAPACITY 100

static p8_preview_info_t preview_info;
static int preview_item = -1;
static bool preview_loaded = false;
static p8_clock_t preview_highlight_time;
static bool preview_showing = false;
static p8_clock_t preview_last_button_time;
static char preview_title_line1[128];
static char preview_title_line2[128];
static char preview_author_line1[128];
static char preview_author_line2[128];
static char *filename_mem = NULL;
static char *filename_mem_ptr = NULL;
static char *filename_mem_end = NULL;

struct dir_entry {
    char *file_name;
    bool is_dir;
};

static struct dir_entry *dir_contents = NULL;
static int nitems = 0;
static int capacity = 0;
static int selected_index = 0;
static char last_browse_cart_dir[PATH_MAX];

static void clear_dir_contents(void) {
    filename_mem_ptr = filename_mem;
    nitems = 0;
    selected_index = 0;
}
static void append_dir_entry(const char *file_name, bool is_dir)
{
   if (nitems >= capacity) {
        int new_capacity;
        if (capacity == 0)
            new_capacity = 10;
        else
            new_capacity = capacity * 2;
        struct dir_entry *new_contents = realloc(dir_contents, sizeof(dir_contents[0]) * new_capacity);
        if (!new_contents) {
            fputs("Out of memory\n", stderr);
            return;
        }
        dir_contents = new_contents;
        capacity = new_capacity;
    }
    struct dir_entry *dir_entry = &dir_contents[nitems];
    size_t name_len = strlen(file_name);
    if (filename_mem_ptr + name_len + 1 > filename_mem_end) {
        size_t old_size = filename_mem_end - filename_mem;
        size_t new_size = old_size * 2;
        size_t offset = filename_mem_ptr - filename_mem;
        char *new_filename_mem = realloc(filename_mem, new_size);
        if (!new_filename_mem) {
            fputs("Out of memory\n", stderr);
            return;
        }
        filename_mem_ptr = new_filename_mem + offset;
        filename_mem_end = new_filename_mem + new_size;
        filename_mem = new_filename_mem;
    }
    dir_entry->file_name = filename_mem_ptr;
    memcpy(dir_entry->file_name, file_name, name_len + 1);
    filename_mem_ptr += name_len + 1;
    dir_entry->is_dir = is_dir;
    nitems++;
}

static int compare_dir_entry(const void *p1, const void *p2)
{
    struct dir_entry *dir_entry1 = (struct dir_entry *)p1;
    struct dir_entry *dir_entry2 = (struct dir_entry *)p2;
    if (dir_entry1->is_dir != dir_entry2->is_dir)
        return (dir_entry1->is_dir ? 0 : 1) - (dir_entry2->is_dir ? 0 : 1);
    else
        return strcmp(dir_entry1->file_name, dir_entry2->file_name);
}
static void list_dir() {
    selected_index = 0;
#ifdef NEXTP8
    if (m_current_cart_dir[0] == '\0') {
        clear_dir_contents();
        // Show drives
        static const char *volume_names[2] = {"0:/", "1:/"};
        for (int i=0;i<2;++i)
            append_dir_entry(volume_names[i], true);
        return;
    }
#endif
    p8_show_io_icon(true);
    DIR *dir = opendir(m_current_cart_dir);
    if (dir == NULL) {
        fprintf(stderr, "%s: %s\n", m_current_cart_dir, strerror(errno));
    } else {
        clear_dir_contents();
        char full_path[PATH_MAX];
        for (;;) {
            struct dirent *dirent;
            errno = 0;
            dirent = readdir(dir);
            if (dirent == NULL) {
                if (errno != 0)
                    fprintf(stderr, "%s: %s\n", m_current_cart_dir, strerror(errno));
                break;
            }
            if (strcmp(dirent->d_name, ".") == 0)
                continue;
            if (p8_make_full_path(full_path, sizeof(full_path), m_current_cart_dir, dirent->d_name) != 0) {
                fputs("Path too long\n", stderr);
                continue;
            }
            bool is_dir = false;
            if (full_path[0] == '\0' ||
                (full_path[1] == ':' &&
                 (full_path[2] == '/' || full_path[2] == '\\') &&
                 full_path[3] == '\0')) {
                is_dir = true;
            } else {
                struct stat statbuf;
                int res = stat(full_path, &statbuf);
                if (res == 0)
                    is_dir = S_ISDIR(statbuf.st_mode);
                else
                    fprintf(stderr, "%s: %s\n", full_path, strerror(errno));
            }
            append_dir_entry(dirent->d_name, is_dir);
        }
        closedir(dir);
    }
    qsort(dir_contents, nitems, sizeof(dir_contents[0]), compare_dir_entry);
    p8_show_io_icon(false);
}

void draw_file_name(const char *str, int x, int y, int col)
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
        overlay_draw_char(display_char, cursor_x, y, col);
        cursor_x += GLYPH_WIDTH;
    }
}

static int wrap_text(const char *text, int max_chars,
                     char *line1, int line1_size,
                     char *line2, int line2_size)
{
    int len = (int)strlen(text);
    if (len <= max_chars) {
        snprintf(line1, line1_size, "%s", text);
        line2[0] = '\0';
        return 1;
    }

    int break_pos = max_chars;
    for (int i = max_chars - 1; i >= 0; i--) {
        if (text[i] == ' ') {
            break_pos = i;
            break;
        }
    }
    int copy1 = break_pos < line1_size - 1 ? break_pos : line1_size - 1;
    memcpy(line1, text, copy1);
    line1[copy1] = '\0';
    const char *rest = text + break_pos;
    if (*rest == ' ')
        rest++;
    snprintf(line2, line2_size, "%.*s", max_chars, rest);
    return 2;
}

static inline void screen_pixel(int x, int y, int col)
{
    if (x < overlay_clip_x0 || y < overlay_clip_y0 || x >= overlay_clip_x1 || y >= overlay_clip_y1)
        return;
    uint8_t *dest = m_memory + (m_memory[MEMORY_SCREEN_PHYS] << 8) + (x >> 1) + y * 64;
    if (x & 1)
        *dest = ((col & 0xF) << 4) | (*dest & 0x0F);
    else
        *dest = (*dest & 0xF0) | (col & 0x0F);
}

static void draw_preview_label(const p8_dialog_t *dialog, void *user_data, int x, int y, int width, int height)
{
    (void)dialog;
    (void)user_data;
    if (!preview_info.has_label) {
        int draw_w = width < PREVIEW_LABEL_DISPLAY_SIZE ? width : PREVIEW_LABEL_DISPLAY_SIZE;
        int draw_h = height < PREVIEW_LABEL_DISPLAY_SIZE ? height : PREVIEW_LABEL_DISPLAY_SIZE;
        const char *msg = "no label";
        int text_w = overlay_get_text_width(msg);
        int tx = x + (draw_w - text_w) / 2;
        int ty = y + (draw_h - GLYPH_HEIGHT) / 2;
        overlay_draw_simple_text(msg, tx, ty, 7);
        return;
    }

    int draw_w = width < PREVIEW_LABEL_DISPLAY_SIZE ? width : PREVIEW_LABEL_DISPLAY_SIZE;
    int draw_h = height < PREVIEW_LABEL_DISPLAY_SIZE ? height : PREVIEW_LABEL_DISPLAY_SIZE;
    int ox = x + (width - draw_w) / 2;
    memset(m_memory + (m_memory[MEMORY_SCREEN_PHYS] << 8), 0, MEMORY_SCREEN_SIZE);
    for (int dy = 0; dy < draw_h; dy++) {
        for (int dx = 0; dx < draw_w; dx++) {
            int px = ox + dx;
            int py = y + dy;
            int color = preview_info.label[(dy * P8_HEIGHT / PREVIEW_LABEL_DISPLAY_SIZE) * 128 + (dx * P8_WIDTH / PREVIEW_LABEL_DISPLAY_SIZE)];
            if (color >= 16) {
                screen_pixel(px, py, color - 16);
                overlay_pixel(px, py, 0);
            } else {
                overlay_pixel(px, py, color);
            }
        }
    }
}

static void preview_use_filename_as_title(const char *file_name)
{
    size_t len = strlen(file_name);
    const char *dot = strrchr(file_name, '.');
    if (dot)
        len = dot - file_name;
    if (len >= sizeof(preview_info.title))
        len = sizeof(preview_info.title) - 1;
    memcpy(preview_info.title, file_name, len);
    preview_info.title[len] = '\0';
}

static void render_file_item(const p8_dialog_t *dialog, void *user_data, int index, bool selected, int x, int y, int width, int height, int fg_color, int bg_color)
{
    (void)dialog;
    (void)user_data;
    struct dir_entry *dir_entry = &dir_contents[index];

    if (selected)
        overlay_draw_rectfill(x, y - 1, x + width - 1, y + height - 1, bg_color);

    // Clip to avoid drawing filename over " <dir>" suffix
    int clip_x, clip_y, clip_w, clip_h;
    overlay_clip_get(&clip_x, &clip_y, &clip_w, &clip_h);

    int item_w = dir_entry->is_dir ? width - GLYPH_WIDTH * 6 : width;
    overlay_clip_intersect(x, y, item_w, height);

    // Draw filename with case conversion
    draw_file_name(dir_entry->file_name, x, y, fg_color);

    overlay_clip_set(clip_x, clip_y, clip_w, clip_h);

    // Draw " <dir>" suffix for directories
    if (dir_entry->is_dir)
        overlay_draw_simple_text(" <dir>", x + width - GLYPH_WIDTH * 6, y, fg_color);
}

#ifdef ENABLE_BBS_DOWNLOAD
static p8_dialog_result_t show_bbs_download_dialog(char *cart_id_buffer, size_t cart_id_buffer_len)
{
#ifdef NEXTP8
    /* Check if Wi-Fi is connected, show config dialog if not */
    wifi_ap_info_t current_ap;
    if (wifi_get_status(&current_ap) < 0) {
        /* Not connected - show Wi-Fi config dialog */
        if (!wifi_show_config_dialog()) {
            /* User cancelled - don't show BBS dialog */
            return DIALOG_RESULT_CANCELLED;
        }
    }

    /* Wait for connection to be established */
    if (!wifi_wait_for_connected()) {
        /* User cancelled or connection failed */
        return DIALOG_RESULT_CANCELLED;
    }
#endif
    cart_id_buffer[0] = '\0';
    p8_dialog_control_t bbs_controls[] = {
        DIALOG_LABEL("enter bbs cart id:"),
        DIALOG_INPUTBOX("", cart_id_buffer, cart_id_buffer_len),
        DIALOG_SPACING(),
#ifdef NEXTP8
        DIALOG_BUTTON("wi-fi config", 100),
        DIALOG_SPACING(),
#endif
        DIALOG_BUTTONBAR()
    };

    p8_dialog_t bbs_dialog;
#ifdef NEXTP8
    p8_dialog_init(&bbs_dialog, "download from bbs", bbs_controls, 6, 120);
#else
    p8_dialog_init(&bbs_dialog, "download from bbs", bbs_controls, 4, 120);
#endif

    p8_dialog_set_showing(&bbs_dialog, true);
    p8_dialog_action_t bbs_result;
    do {
        p8_dialog_draw(&bbs_dialog);
        p8_flip();

        bbs_result = p8_dialog_update(&bbs_dialog);

#ifdef NEXTP8
        /* Handle Wi-Fi config button */
        if (bbs_result.type == DIALOG_RESULT_BUTTON && bbs_result.action_id == 100) {
            wifi_show_config_dialog();
            bbs_result.type = DIALOG_RESULT_NONE; // Keep BBS dialog open after returning from Wi-Fi config
            continue;
        }
#endif
    } while (bbs_result.type == DIALOG_RESULT_NONE && !p8_is_quit_requested());
    p8_dialog_set_showing(&bbs_dialog, false);
    p8_dialog_cleanup(&bbs_dialog);

    return bbs_result.type;
}
#endif

static int show_menu(void)
{
    p8_dialog_control_t controls[] = {
#ifdef ENABLE_BBS_DOWNLOAD
        DIALOG_MENUITEM("bbs download", 1),
#endif
#ifdef NEXTP8
        DIALOG_MENUITEM("wi-fi config", 2),
#endif
        DIALOG_MENUITEM("show version", 3),
        DIALOG_MENUITEM("controls", 4),
        DIALOG_MENUITEM("back", 5)
    };

    p8_dialog_t dialog;
    p8_dialog_init(&dialog, NULL, controls, sizeof(controls) / sizeof(controls[0]), 0);
    p8_dialog_action_t result = p8_dialog_run(&dialog);
    p8_dialog_cleanup(&dialog);

    if (result.type == DIALOG_RESULT_BUTTON)
        return result.action_id;
    else
        return 0;
}

// Create dialog with custom listbox renderer
static p8_dialog_control_t browse_controls[] = {
    DIALOG_LABEL_INVERTED(NULL),
    DIALOG_LISTBOX_CUSTOM_FULLSCREEN(NULL, 0, &selected_index, render_file_item, NULL),
    DIALOG_LABEL_INVERTED("\216:select file \227:bbs download"),
};

static p8_dialog_t browse_dialog;

static p8_dialog_control_t preview_controls[5] = {
    DIALOG_CUSTOM_CONTROL(PREVIEW_LABEL_DISPLAY_SIZE, PREVIEW_LABEL_DISPLAY_SIZE, draw_preview_label, NULL),
    DIALOG_LABEL(""),
    DIALOG_LABEL(""),
    DIALOG_LABEL(""),
    DIALOG_LABEL(""),
};
static p8_dialog_t preview_dialog;

static void browse_init(void)
{
    assert(dir_contents == NULL);
    assert(capacity == 0);
    assert(nitems == 0);
    assert(browse_controls[0].label == NULL);

    filename_mem = (char *)malloc(INITIAL_FILENAME_MEM_SIZE);
    dir_contents = (struct dir_entry *)malloc(sizeof(struct dir_entry) * INITIAL_CAPACITY);
    if (!filename_mem || !dir_contents) {
        free(filename_mem);
        free(dir_contents);
        fputs("Out of memory\n", stderr);
        abort();
    }
    capacity = INITIAL_CAPACITY;
    filename_mem_end = filename_mem + INITIAL_FILENAME_MEM_SIZE;
    clear_dir_contents();

    if (!m_current_cart_dir[0]) {
        if (access(DEFAULT_CARTS_PATH, F_OK) == 0)
            strcpy(m_current_cart_dir, DEFAULT_CARTS_PATH);
        else
            strcpy(m_current_cart_dir, FALLBACK_CARTS_PATH);
    }
    browse_controls[0].label = m_current_cart_dir;
    browse_controls[1].data.listbox.item_count = nitems;
    
    p8_dialog_init(&browse_dialog, NULL, browse_controls, sizeof(browse_controls) / sizeof(browse_controls[0]), P8_WIDTH);
    browse_dialog.draw_border = false;
    browse_dialog.padding = 0;
}

static void browse_show(void)
{
    /* Relist directory if current_cart_dir changed since last show/hide, or first time */
    bool changed = strcmp(m_current_cart_dir, last_browse_cart_dir) != 0;
    if (changed) {
        list_dir();
        browse_controls[1].data.listbox.item_count = nitems;
    }

    p8_dialog_set_showing(&browse_dialog, true);

    preview_item = -1;
    preview_loaded = false;
    preview_showing = false;
    preview_last_button_time = p8_clock();

    // Remap screen palette to alternate palette so we can display full
    // 32-colour labels.
    for (int i = 0; i < 16; i++)
        color_set(PALTYPE_SCREEN, i, 128 + i);
}

static void browse_hide(void)
{
    if (preview_showing) {
        preview_showing = false;
        p8_dialog_set_showing(&preview_dialog, false);
    }

    p8_dialog_set_showing(&browse_dialog, false);

    /* Record current directory so browse_show can detect external changes (e.g. cd()) */
    strtcpy(last_browse_cart_dir, m_current_cart_dir, sizeof(last_browse_cart_dir));

    reset_color();
    memset(m_memory + (m_memory[MEMORY_SCREEN_PHYS] << 8), 0, MEMORY_SCREEN_SIZE);
}

static void browse_update(void)
{
    p8_dialog_action_t result = p8_dialog_update(&browse_dialog);

    bool any_button = (m_buttonsp[0] != 0);
    bool action2_pressed = ((m_buttonsp[0] & BUTTON_MASK_ACTION2) != 0);

    if (any_button) {
        if (preview_showing) {
            preview_showing = false;
            p8_dialog_set_showing(&preview_dialog, false);
        }

        preview_loaded = false;
        preview_item = -1;
    }

    if (selected_index != preview_item) {
        preview_item = selected_index;
        preview_loaded = false;
        preview_highlight_time = p8_clock();
    }

    // Try to load preview after item has been highlighted long enough
    if (!preview_loaded &&
        !any_button && !preview_showing &&
        selected_index >= 0 && selected_index < nitems &&
        !dir_contents[selected_index].is_dir) {
        unsigned highlight_ms = p8_clock_ms(p8_clock_delta(preview_highlight_time, p8_clock()));
        if (highlight_ms >= PREVIEW_LOAD_DELAY_MS) {
            char full_path[PATH_MAX];
            if (p8_make_full_path(full_path, sizeof(full_path), m_current_cart_dir, dir_contents[selected_index].file_name) == 0) {
                preview_loaded = p8_preview_load(full_path, &preview_info);
                if (preview_loaded && preview_info.title[0] == '\0')
                    preview_use_filename_as_title(dir_contents[selected_index].file_name);
            }
        }
    }

    // Show preview popup if loaded and enough time has elapsed
    if (preview_loaded && !preview_showing && !any_button &&
        (preview_info.has_label || preview_info.title[0] != '\0')) {
        unsigned elapsed_ms = p8_clock_ms(p8_clock_delta(preview_last_button_time, p8_clock()));
        if (elapsed_ms >= PREVIEW_SHOW_DELAY_MS) {
            int ncontrols = 1;
            int n_title = wrap_text(preview_info.title, PREVIEW_CHARS_PER_LINE,
                                   preview_title_line1, sizeof(preview_title_line1),
                                   preview_title_line2, sizeof(preview_title_line2));
            preview_controls[ncontrols++].label = preview_title_line1;
            if (n_title == 2)
                preview_controls[ncontrols++].label = preview_title_line2;
            if (preview_info.author[0] != '\0') {
                int n_author = wrap_text(preview_info.author, PREVIEW_CHARS_PER_LINE,
                                        preview_author_line1, sizeof(preview_author_line1),
                                        preview_author_line2, sizeof(preview_author_line2));
                preview_controls[ncontrols++].label = preview_author_line1;
                if (n_author == 2)
                    preview_controls[ncontrols++].label = preview_author_line2;
            }
            p8_dialog_init(&preview_dialog, NULL, preview_controls, ncontrols,
                           PREVIEW_DIALOG_CONTENT_WIDTH + PREVIEW_BORDER_SIZE);
            preview_dialog.x = P8_WIDTH - preview_dialog.width;
            preview_showing = true;
            p8_dialog_set_showing(&preview_dialog, true);
        }
    }

    if (result.type == DIALOG_RESULT_NONE && action2_pressed) {
        char cart_id_buffer[64] = {'\0'};
        int action_id = show_menu();
        switch (action_id) {
#ifdef ENABLE_BBS_DOWNLOAD
            case 1:
                if (show_bbs_download_dialog(cart_id_buffer, sizeof(cart_id_buffer)) == DIALOG_RESULT_ACCEPTED) {
                    const char *cart_id = (cart_id_buffer[0] == '#') ? (cart_id_buffer + 1) : cart_id_buffer;
                    char cart_path[PATH_MAX];
                    if (p8_download_bbs_cart(cart_id, cart_path, sizeof(cart_path)) == 0) {
                        /* Hide browse screen before running cart */
                        browse_hide();
                        if (p8_load(cart_path, NULL, cart_id, NULL) == 0) {
                            p8_run();
                            p8_reset();
                        }
                        /* Show browse screen again after cart exits */
                        browse_show();
                    }
                }
                break;
#endif
            case 2: {
                /* "Edit file": switch to editor with selected file */
                struct dir_entry *dir_entry = &dir_contents[selected_index];
                char full_path[PATH_MAX];
                if (p8_make_full_path(full_path, sizeof(full_path), m_current_cart_dir, dir_entry->file_name) < 0) {
                    fputs("Path too long\n", stderr);
                    break;
                }
                /* Load cart for editing, then switch to editor */
                p8_load(full_path, NULL, NULL, NULL);
                p8_set_active_screen(P8_SCREEN_EDIT);
                break;
            }
#ifdef NEXTP8
            case 3:
                wifi_show_config_dialog();
                break;
#endif
            case 4:
                p8_show_version_dialog();
                break;
            case 5:
                p8_show_controls_dialog();
                break;
            default:
                break;
        }
    }

    if (result.type == DIALOG_RESULT_ACCEPTED && selected_index >= 0 && selected_index < nitems) {
        struct dir_entry *dir_entry = &dir_contents[selected_index];
        char full_path[PATH_MAX];
        if (p8_make_full_path(full_path, sizeof(full_path), m_current_cart_dir, dir_entry->file_name) < 0) {
            fputs("Path too long\n", stderr);
            return;
        }
        if (dir_entry->is_dir) {
            if (strtcpy(m_current_cart_dir, full_path, sizeof(m_current_cart_dir)) < 0) {
                fputs("Path too long\n", stderr);
                return;
            }
            list_dir();
            browse_controls[1].data.listbox.item_count = nitems;
            
            // Reset preview state when directory changes
            preview_item = -1;
            preview_loaded = false;
            preview_showing = false;
            p8_preview_cache_clear();
        } else {
            /* Load and run cart */
            browse_hide();
            if (p8_load(full_path, NULL, NULL, NULL) == 0)
                p8_run();
            browse_show();
        }
    }

    if (any_button)
        preview_last_button_time = p8_clock();
}

static void browse_draw(int x, int y, int w, int h)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    
    p8_dialog_draw(&browse_dialog);

    if (preview_showing)
        p8_dialog_draw(&preview_dialog);
}

void browse_shutdown(void)
{
    p8_dialog_cleanup(&browse_dialog);

    clear_dir_contents();

    free(filename_mem);
    free(dir_contents);
    filename_mem = NULL;
    dir_contents = NULL;
    nitems = 0;
    capacity = 0;
    selected_index = 0;
    last_browse_cart_dir[0] = '\0';
}

p8_screen_t p8_screen_browse = {
    .init=browse_init,
    .shutdown=browse_shutdown,
    .show=browse_show,
    .hide=browse_hide,
    .draw=browse_draw,
    .update=browse_update
};
