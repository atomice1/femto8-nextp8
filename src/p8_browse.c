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

struct dir_entry {
    const char *file_name;
    bool is_dir;
};

static const char *pwd = NULL;
static struct dir_entry *dir_contents = NULL;
static int nitems = 0;
static int capacity = 0;
static int current_item = 0;
static void clear_dir_contents(void) {
    for (int i=0;i<nitems;++i)
        free((char *)dir_contents[i].file_name);
    nitems = 0;
    current_item = 0;
}
static void append_dir_entry(const char *file_name, bool is_dir)
{
    if (nitems == capacity) {
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
    dir_entry->file_name = strdup(file_name);
    if (!dir_entry->file_name) {
        fputs("Out of memory\n", stderr);
        return;
    }
    dir_entry->is_dir = is_dir;
    nitems++;
}
static const char *make_full_path(const char *dir_path, const char *file_name)
{
    if (!dir_path || !file_name)
        return NULL;

    size_t dir_len = strlen(dir_path);
    size_t file_len = strlen(file_name);
    if (dir_len > 256 || file_len > 256)
        return NULL;

    char *ret = malloc(dir_len + 1 + file_len + 1);
    if (!ret)
        return NULL;

    if (strcmp(file_name, "..") == 0) {
        strcpy(ret, dir_path);
        char *slash = strrchr(ret, '/');
        if (!slash)
            slash = strrchr(ret, '\\');
        if (slash) {
            if (slash[1] == '\0' && ret[1] == ':' && slash==ret+2) {
                // If going up a directory from the root of a drive,
                // go up to the list of drives rather than the current
                // directory of the drive.
                // i.e. "C:\" -> "" rather than "C:\" -> "C:"
                ret[0] = '\0';
            } else if (ret[1] == ':' && slash==ret+2) {
                // If going up to the root of the drive don't erase
                // the trailing slash.
                slash[1] = '\0';
            } else {
                slash[0] = '\0';
            }
        }
    } else {
        strcpy(ret, dir_path);
        if (dir_len > 0 &&
            ret[dir_len - 1] != '/' &&
            ret[dir_len - 1] != '\\')
            strcat(ret, "/");
        strcat(ret, file_name);
    }
    return ret;
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
static void list_dir(const char* path) {
#ifdef NEXTP8
    if (path[0] == '\0') {
        free((char *)pwd);
        pwd = strdup(path);
        clear_dir_contents();
        // Show drives
        static const char *volume_names[2] = {"0:/", "1:/"};
        for (int i=0;i<2;++i)
            append_dir_entry(volume_names[i], true);
        return;
    }
#endif
    p8_show_disk_icon(true);
    DIR *dir = opendir(path);
    if (dir == NULL) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
    } else {
        free((char *)pwd);
        pwd = strdup(path);
        clear_dir_contents();
        for (;;) {
            struct dirent *dirent;
            errno = 0;
            dirent = readdir(dir);
            if (dirent == NULL) {
                if (errno != 0)
                    fprintf(stderr, "%s: %s\n", path, strerror(errno));
                break;
            }
            const char *full_path = make_full_path(path, dirent->d_name);
            if (!full_path) {
                fputs("Out of memory\n", stderr);
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
            free((char *)full_path);
            append_dir_entry(dirent->d_name, is_dir);
        }
        closedir(dir);
    }
    qsort(dir_contents, nitems, sizeof(dir_contents[0]), compare_dir_entry);
    p8_show_disk_icon(false);
}

static void draw_file_name(const char *str, int x, int y, int col)
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

static void draw_preview_label(int x, int y, int width, int height)
{
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

static void render_file_item(void *user_data, int index, bool selected, int x, int y, int width, int height, int fg_color, int bg_color)
{
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
    p8_dialog_init(&bbs_dialog, "download from bbs", bbs_controls, 7, 120);
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
            continue;
        }
#endif
    } while (bbs_result.type == DIALOG_RESULT_NONE);
    p8_dialog_set_showing(&bbs_dialog, false);
    p8_dialog_cleanup(&bbs_dialog);

    printf("result: %d\n", bbs_result.type);
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

const char *browse_for_cart(void)
{
    p8_init();
    p8_reset();
    // Remap screen palette to alternate palette so we can display full
    // 32-colour labels.
    for (int i = 0; i < 16; i++)
        color_set(PALTYPE_SCREEN, i, 128 + i);

    if (setjmp(jmpbuf_restart)) {
        return NULL;
    }

    if (access(DEFAULT_CARTS_PATH, F_OK) == 0)
        list_dir(DEFAULT_CARTS_PATH);
    else
        list_dir(FALLBACK_CARTS_PATH);

    const char *cart_path = NULL;
    int selected_index = 0;

    // Create dialog with custom listbox renderer
    p8_dialog_control_t controls[] = {
        DIALOG_LABEL_INVERTED(""),
        DIALOG_LISTBOX_CUSTOM_FULLSCREEN(NULL, NULL, nitems, &selected_index, render_file_item),
        DIALOG_LABEL_INVERTED("\216: select file  \227: bbs download"),
    };

    p8_dialog_t dialog;
    p8_dialog_init(&dialog, NULL, controls, sizeof(controls) / sizeof(controls[0]), P8_WIDTH);
    dialog.draw_border = false;
    dialog.padding = 0;

    p8_dialog_control_t preview_controls[5] = {
        DIALOG_CUSTOM_CONTROL(PREVIEW_LABEL_DISPLAY_SIZE, PREVIEW_LABEL_DISPLAY_SIZE, draw_preview_label),
        DIALOG_LABEL(""),
        DIALOG_LABEL(""),
        DIALOG_LABEL(""),
        DIALOG_LABEL(""),
    };
    p8_dialog_t preview_dialog;

    preview_item = -1;
    preview_loaded = false;
    preview_showing = false;
    preview_last_button_time = p8_clock();

    char cart_id_buffer[64] = {'\0'};
    p8_dialog_action_t result = { DIALOG_RESULT_NONE, 0 };
    p8_dialog_set_showing(&dialog, true);

    for (;;) {
        selected_index = 0;
        controls[0].label = pwd;
        controls[1].data.listbox.item_count = nitems;

        // Reset preview state when directory changes
        preview_item = -1;
        preview_loaded = false;
        preview_showing = false;
        p8_preview_cache_clear();

        // Main dialog loop
        do {
            p8_dialog_draw(&dialog);

            if (preview_showing)
                p8_dialog_draw(&preview_dialog);

            p8_flip();

            bool any_button = (m_buttonsp[0] != 0);

            if (any_button) {
                preview_last_button_time = p8_clock();

                if (preview_showing) {
                    preview_showing = false;
                    p8_dialog_set_showing(&preview_dialog, false);
                }

                preview_loaded = false;
                preview_item = -1;
            }

            result = p8_dialog_update(&dialog);

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
                    const char *full_path = make_full_path(pwd, dir_contents[selected_index].file_name);
                    if (full_path) {
                        preview_loaded = p8_preview_load(full_path, &preview_info);
                        if (preview_loaded && preview_info.title[0] == '\0')
                            preview_use_filename_as_title(dir_contents[selected_index].file_name);
                        free((char *)full_path);
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

            if (result.type == DIALOG_RESULT_NONE && ((m_buttonsp[0] & BUTTON_MASK_ACTION2) != 0)) {
                int action_id = show_menu();
                switch (action_id) {
#ifdef ENABLE_BBS_DOWNLOAD
                    case 1:
                        if (show_bbs_download_dialog(cart_id_buffer, sizeof(cart_id_buffer)) == DIALOG_RESULT_ACCEPTED) {
                            const char *cart_id = (cart_id_buffer[0] == '#') ? (cart_id_buffer + 1) : cart_id_buffer;
                            cart_path = p8_download_bbs_cart(cart_id);
                        }
                        break;
#endif
#ifdef NEXTP8
                    case 2:
                        wifi_show_config_dialog();
                        break;
#endif
                    case 3:
                        p8_show_version_dialog();
                        break;
                    case 4:
                        p8_show_controls_dialog();
                        break;
                    default:
                        break;
                }
                if (action_id == 2 && cart_path)
                    break;
            }
        } while (result.type == DIALOG_RESULT_NONE);

        if (preview_showing) {
            preview_showing = false;
            p8_dialog_set_showing(&preview_dialog, false);
        }

        if (cart_path)
            break;

        if (result.type == DIALOG_RESULT_CANCELLED)
            break;

        if (result.type == DIALOG_RESULT_ACCEPTED && selected_index >= 0 && selected_index < nitems) {
            struct dir_entry *dir_entry = &dir_contents[selected_index];
            const char *full_path = make_full_path(pwd, dir_entry->file_name);
            if (!full_path) {
                fputs("Out of memory\n", stderr);
                continue;
            }
            if (dir_entry->is_dir) {
                list_dir(full_path);
                free((char *)full_path);
            } else {
                cart_path = full_path;
                break;
            }
        }
    }

    p8_dialog_set_showing(&dialog, false);
    p8_dialog_cleanup(&dialog);

    overlay_clear();
    memset(m_memory + (m_memory[MEMORY_SCREEN_PHYS] << 8), 0, MEMORY_SCREEN_SIZE);
    reset_color();
    p8_flip();

    clear_dir_contents();
    free(dir_contents);
    free((char *)pwd);

    return cart_path;
}
