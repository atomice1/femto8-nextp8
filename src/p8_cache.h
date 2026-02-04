/**
 * Copyright (C) 2026 Chris January
 * 
 * BBS cart cache management
 */

#ifndef P8_CACHE_H
#define P8_CACHE_H

/**
 * Download a cart from the BBS and cache it.
 * 
 * @param cart_id BBS cart ID to download
 * @param filename_out Output buffer for cached filename
 * @param max_filename_length Maximum length of filename buffer
 * @return 0 on success, -1 on failure (check errno)
 */
int cache_download(const char *cart_id, char *filename_out, unsigned max_filename_length);

#endif /* P8_CACHE_H */
