/**
 * Copyright (C) 2026 Chris January
 *
 * Cart preview loading and caching for the file browser.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "p8_preview.h"
#include "p8_emu.h"
#include "p8_parser.h"

#define PREVIEW_CACHE_SIZE 64

typedef struct {
    char *path;
    p8_preview_info_t info;
} preview_cache_entry_t;

static preview_cache_entry_t preview_cache[PREVIEW_CACHE_SIZE];
static int preview_cache_count = 0;

static p8_preview_info_t *cache_lookup(const char *path)
{
    for (int i = 0; i < preview_cache_count; i++) {
        if (strcmp(preview_cache[i].path, path) == 0)
            return &preview_cache[i].info;
    }
    return NULL;
}

static p8_preview_info_t *cache_insert(const char *path)
{
    preview_cache_entry_t *entry;
    if (preview_cache_count < PREVIEW_CACHE_SIZE) {
        entry = &preview_cache[preview_cache_count++];
    } else {
        // Evict oldest entry
        free(preview_cache[0].path);
        memmove(&preview_cache[0], &preview_cache[1],
                (PREVIEW_CACHE_SIZE - 1) * sizeof(preview_cache[0]));
        entry = &preview_cache[PREVIEW_CACHE_SIZE - 1];
    }
    entry->path = strdup(path);
    if (!entry->path) {
        if (preview_cache_count > 0)
            preview_cache_count--;
        return NULL;
    }
    return &entry->info;
}

void p8_preview_cache_clear(void)
{
    for (int i = 0; i < preview_cache_count; i++)
        free(preview_cache[i].path);
    preview_cache_count = 0;
}

static void extract_title_author(const char *lua_script,
                                 char *title, size_t title_size,
                                 char *author, size_t author_size)
{
    title[0] = '\0';
    author[0] = '\0';

    if (!lua_script)
        return;

    // First line: --[game title]
    if (lua_script[0] == '-' && lua_script[1] == '-') {
        const char *start = lua_script + 2;
        while (*start == ' ')
            start++;
        const char *end = strchr(start, '\n');
        if (end) {
            size_t len = end - start;
            if (len >= title_size)
                len = title_size - 1;
            memcpy(title, start, len);
            title[len] = '\0';
            while (len > 0 && (title[len-1] == ' ' || title[len-1] == '\r'))
                title[--len] = '\0';

            // Second line: --by [author name]
            const char *line2 = end + 1;
            if (line2[0] == '-' && line2[1] == '-') {
                const char *author_start = line2 + 2;
                while (*author_start == ' ')
                    author_start++;
                const char *author_end = strchr(author_start, '\n');
                if (!author_end)
                    author_end = author_start + strlen(author_start);
                size_t alen = author_end - author_start;
                if (alen >= author_size)
                    alen = author_size - 1;
                memcpy(author, author_start, alen);
                author[alen] = '\0';
                while (alen > 0 && (author[alen-1] == ' ' || author[alen-1] == '\r'))
                    author[--alen] = '\0';
            }
        }
    }
}

static bool load_p8_text_preview(uint8_t *buffer, long size,
                                 p8_preview_info_t *info)
{
    static const char label_token[] = "__label__";
    static const char lua_token[] = "__lua__";
    static const char *hex_chars = "0123456789abcdefghijklmnopqrstuv";

    (void)size;

    char *label_start = strstr((char *)buffer, label_token);
    char *lua_start = strstr((char *)buffer, lua_token);

    // Extract label
    if (label_start) {
        info->has_label = true;
        char *line = label_start + strlen(label_token);
        line = strchr(line, '\n');
        if (line) line++;

        int row = 0;
        while (line && row < 128 && *line != '_' && *line != '\0') {
            int x = 0;
            while (*line != '\n' && *line != '\0' && x < 128) {
                const char *pos = strchr(hex_chars, *line);
                if (pos) {
                    info->label[row * 128 + x] = (uint8_t)(pos - hex_chars);
                    x++;
                }
                line++;
            }
            if (*line == '\n') line++;
            if (x > 0) row++;
        }
    }

    if (lua_start) {
        char *lua = lua_start + strlen(lua_token);
        lua = strchr(lua, '\n');
        if (lua) {
            lua++;
            extract_title_author(lua,
                                 info->title, sizeof(info->title),
                                 info->author, sizeof(info->author));
        }
    }

    return true;
}

static bool load_p8_png_preview(uint8_t *buffer, long size,
                                p8_preview_info_t *info)
{
    uint8_t *temp_memory = calloc(1, 0x8000);
    if (!temp_memory)
        return false;

    const char *lua_script = NULL;
    uint8_t *decompression_buffer = NULL;
    parse_cart_ram(buffer, (int)size, temp_memory, &lua_script,
                   &decompression_buffer, info->label);
    info->has_label = true;

    extract_title_author(lua_script,
                         info->title, sizeof(info->title),
                         info->author, sizeof(info->author));

    free(decompression_buffer);
    free(temp_memory);
    return true;
}

bool p8_preview_load(const char *path, p8_preview_info_t *info_out)
{
    // Check cache first
    p8_preview_info_t *cached = cache_lookup(path);
    if (cached) {
        *info_out = *cached;
        return true;
    }

    p8_show_disk_icon(true);

    p8_preview_info_t info;
    memset(&info, 0, sizeof(info));

    FILE *f = fopen(path, "rb");
    if (!f)
        return false;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    rewind(f);

    if (file_size <= 0 || file_size > 128 * 1024) {
        fclose(f);
        return false;
    }

    uint8_t *buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(f);
        return false;
    }

    long total_read = 0;
    while (total_read < file_size) {
        long chunk = file_size - total_read;
        if (chunk > 4096)
            chunk = 4096;

        size_t n = fread(buffer + total_read, 1, chunk, f);
        if (n == 0)
            break;
        total_read += n;
    }
    fclose(f);
    buffer[total_read] = '\0';

    static const uint8_t PNG_SIG[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    bool ok;
    if (total_read >= 8 && memcmp(buffer, PNG_SIG, 8) == 0)
        ok = load_p8_png_preview(buffer, total_read, &info);
    else
        ok = load_p8_text_preview(buffer, total_read, &info);

    free(buffer);

    if (ok) {
        p8_preview_info_t *entry = cache_insert(path);
        if (entry)
            *entry = info;
        *info_out = info;
    }

    p8_show_disk_icon(false);

    return ok;
}
