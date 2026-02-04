/**
 * Copyright (C) 2026 Chris January
 *
 * BBS interface
 */

#include "p8_bbs.h"
#include "p8_emu.h"
#include "p8_http.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ENABLE_BBS_DOWNLOAD

#define DEFAULT_BBS_BASE_URL "https://www.lexaloffle.com/bbs/"

int bbs_start_get_cart(int cat, int play_src, const char *lid)
{
    const char *bbs_base_url;
    char url[512];

    /* Validate parameters */
    if (cat < 0 || cat > 7) {
        errno = EINVAL;
        return -1;
    }

    if (!lid || strlen(lid) == 0) {
        errno = EINVAL;
        return -1;
    }

    bbs_base_url = getenv("BBS_BASE_URL");
    if (!bbs_base_url) bbs_base_url = DEFAULT_BBS_BASE_URL;

    /* Construct BBS URL */
    int len = snprintf(url, sizeof(url),
                      "%sget_cart.php?cat=%d&play_src=%d&lid=%s",
                      bbs_base_url, cat, play_src, lid);

    if (len >= sizeof(url)) {
        errno = EOVERFLOW;
        return -1;
    }

    /* Start HTTP GET request */
    return http_start_get(url);
}

ssize_t bbs_recv(void *data, unsigned max_length)
{
    return http_recv(data, max_length);
}

int bbs_close(void)
{
    return http_close();
}

#endif

