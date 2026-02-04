/**
 * Copyright (C) 2026 Chris January
 * 
 * BBS interface
 */

#ifndef P8_BBS_H
#define P8_BBS_H

#include <sys/types.h>

/**
 * Start BBS GET request.
 * 
 * @param cat Category (0-7)
 * @param play_src Play source
 * @param lid Level ID string
 * @return 0 on success, -1 on failure (check errno)
 */
int bbs_start_get_cart(int cat, int play_src, const char *lid);

/**
 * Receive BBS response data.
 * 
 * @param data Output buffer for received data
 * @param max_length Maximum bytes to receive
 * @return >0 number of bytes received, 0 on EOF, -1 on failure (check errno)
 */
ssize_t bbs_recv(void *data, unsigned max_length);

/**
 * Close BBS connection.
 * 
 * @return 0 on success, -1 on failure (check errno)
 */
int bbs_close(void);

#endif /* P8_BBS_H */
