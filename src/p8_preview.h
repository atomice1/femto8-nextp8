/**
 * Copyright (C) 2026 Chris January
 *
 * Cart preview loading and caching for the file browser.
 */

#ifndef P8_PREVIEW_H
#define P8_PREVIEW_H

#include <stdbool.h>
#include <stdint.h>

#define PREVIEW_LABEL_SIZE (128 * 128)

typedef struct {
    uint8_t label[PREVIEW_LABEL_SIZE];
    char title[128];
    char author[128];
    bool has_label;
} p8_preview_info_t;

/**
 * Load cart preview info (label, title, author) for the given path.
 * Results are cached in memory; subsequent calls for the same path
 * return the cached data without re-reading the file.
 *
 * @param path       Path to the .p8 or .p8.png cart file.
 * @param info_out   Receives the preview data on success.
 * @return true on success, false if aborted or on error.
 */
bool p8_preview_load(const char *path, p8_preview_info_t *info_out);

/**
 * Flush the entire preview cache (e.g. when changing directories).
 */
void p8_preview_cache_clear(void);

#endif /* P8_PREVIEW_H */
