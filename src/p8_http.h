/**
 * Copyright (C) 2026 Chris January
 *
 * HTTP client interface
 */

#ifndef P8_HTTP_H
#define P8_HTTP_H

#include <sys/types.h>

/**
 * Start HTTP GET request.
 * Supports both http:// and https:// URLs.
 *
 * @param url URL to fetch
 * @return 0 on success, -1 on failure (check errno)
 */
int http_start_get(const char *url);

/**
 * Receive HTTP response data.
 *
 * @param data Output buffer for received data
 * @param max_length Maximum bytes to receive
 * @return >0 number of bytes received, 0 on EOF, -1 on failure (check errno)
 */
ssize_t http_recv(void *data, unsigned max_length);

/**
 * Get HTTP status code from last request.
 *
 * @return HTTP status code (e.g., 200, 404), or 0 if not yet received
 */
int http_get_status_code(void);

/**
 * Close HTTP connection.
 *
 * @return 0 on success, -1 on failure (check errno)
 */
int http_close(void);

#endif /* P8_HTTP_H */
