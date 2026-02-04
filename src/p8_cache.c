/**
 * Copyright (C) 2026 Chris January
 *
 * BBS cart cache management
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <direct.h>   // _mkdir
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir((p), 0777)
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "p8_bbs.h"
#include "p8_cache.h"
#include "p8_emu.h"

#ifdef ENABLE_BBS_DOWNLOAD

#define CACHE_BUFFER_SIZE 8192

/* Ensure cache directory exists */
static int ensure_cache_dir(void)
{
    struct stat st;

    if (stat(CACHE_PATH, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        } else {
            errno = ENOTDIR;
            return -1;
        }
    }

    if (MKDIR(CACHE_PATH) < 0) {
        fprintf(stderr, "Failed to create cache directory '%s': %s\n", CACHE_PATH, strerror(errno));
        return -1;
    }

    return 0;
}

/* Build cache filename for cart ID */
static int build_cache_filename(const char *cart_id, char *buffer, size_t buffer_size)
{
    int len = snprintf(buffer, buffer_size, "%s/%s.p8", CACHE_PATH, cart_id);
    if (len >= buffer_size) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}

int cache_download(const char *cart_id, char *filename_out, unsigned max_filename_length)
{
    char temp_filename[256];
    FILE *fp = NULL;
    unsigned char buffer[CACHE_BUFFER_SIZE];
    ssize_t bytes_read;

    assert(sizeof(temp_filename) >= max_filename_length);

    if (!cart_id || strlen(cart_id) == 0) {
        errno = EINVAL;
        return -1;
    }

    /* Ensure cache directory exists */
    if (ensure_cache_dir() < 0) {
        return -1;
    }

    /* Build cache filename directly in output buffer */
    if (build_cache_filename(cart_id, filename_out, max_filename_length) < 0) {
        return -1;
    }

    /* Check if already cached */
    if (access(filename_out, R_OK) == 0) {
        return 0;
    }

    /* Download from BBS */
    /* BBS cat=7 is for carts, play_src=2 is for direct cart download */
    if (bbs_start_get_cart(7, 2, cart_id) < 0) {
        printf("fail 3\n");
        return -1;
    }

    /* Create temporary file */
    snprintf(temp_filename, sizeof(temp_filename), "%s.tmp", filename_out);
    fp = fopen(temp_filename, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to open temporary file '%s' for writing: %s\n", temp_filename, strerror(errno));
        bbs_close();
        return -1;
    }

    /* Download data */
    while ((bytes_read = bbs_recv(buffer, CACHE_BUFFER_SIZE)) > 0) {
        if (fwrite(buffer, 1, bytes_read, fp) != bytes_read) {
            fclose(fp);
            unlink(temp_filename);
            bbs_close();
            errno = EIO;
            return -1;
        }
    }

    if (bytes_read < 0) {
        fclose(fp);
        unlink(temp_filename);
        bbs_close();
        return -1;
    }

    fclose(fp);
    bbs_close();

    /* Rename temp file to final name */
    if (rename(temp_filename, filename_out) < 0) {
        fprintf(stderr, "Failed to rename '%s' to '%s': %s\n", temp_filename, filename_out, strerror(errno));
        unlink(temp_filename);
        return -1;
    }

    return 0;
}

#endif
