/**
 * Copyright (C) 2026 Chris January
 * 
 * BBS cart cache management
 */

#ifndef P8_CACHE_H
#define P8_CACHE_H

#include <stdbool.h>

/**
 * Download a cart from the BBS and cache it.
 *
 * @param cart_id BBS cart ID to download
 * @param filename_out Output buffer for cached filename
 * @param max_filename_length Maximum length of filename buffer
 * @return 0 on success, -1 on failure (check errno)
 */
int cache_download(const char *cart_id, char *filename_out, unsigned max_filename_length);

/**
 * Download a cart from the BBS to a specific directory.
 *
 * @param cart_id BBS cart ID to download
 * @param download_dir Target directory for download
 * @param filename_out Output buffer for cached filename (full path)
 * @param max_filename_length Maximum length of filename buffer
 * @param force_download If true, download the cart even if it is already cached
 * @return 0 on success, -1 on failure (check errno)
 */
int cache_download_to_dir(const char *cart_id, const char *download_dir, char *filename_out, unsigned max_filename_length, bool force_download);

#endif /* P8_CACHE_H */
