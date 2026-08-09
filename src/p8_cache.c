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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "p8_bbs.h"
#include "p8_cache.h"
#include "p8_emu.h"

#ifdef ENABLE_BBS_DOWNLOAD

#define CACHE_BUFFER_SIZE 8192

/* Ensure cache directory exists */
static int ensure_dir(const char *path)
{
    struct stat st;

    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        } else {
            errno = ENOTDIR;
            return -1;
        }
    }

    if (MKDIR(path) < 0) {
        fprintf(stderr, "Failed to create directory '%s': %s\n", path, strerror(errno));
        return -1;
    }

    return 0;
}

/* Build cache filename for cart ID in specified directory */
static int build_cache_filename(const char *cart_id, const char *dir, char *buffer, size_t buffer_size)
{
    int len = snprintf(buffer, buffer_size, "%s/%s.p8", dir, cart_id);
    if (len >= buffer_size) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}

int cache_download(const char *cart_id, char *filename_out, unsigned max_filename_length)
{
    return cache_download_to_dir(cart_id, CACHE_PATH, filename_out, max_filename_length, false);
}

int cache_download_to_dir(const char *cart_id, const char *download_dir, char *filename_out, unsigned max_filename_length, bool force_download)
{
    char temp_filename[PATH_MAX+1];
    FILE *fp = NULL;
    unsigned char buffer[CACHE_BUFFER_SIZE];
    ssize_t bytes_read;
    size_t total_bytes_read;

    assert(sizeof(temp_filename) >= max_filename_length);

    if (!cart_id || strlen(cart_id) == 0) {
        printf("cache_download: invalid cart_id\n");
        errno = EINVAL;
        return -1;
    }

    /* Ensure download directory exists */
    if (ensure_dir(download_dir) < 0)
        return -1;

    /* Build cache filename directly in output buffer */
    if (build_cache_filename(cart_id, download_dir, filename_out, max_filename_length) < 0) {
        return -1;
    }

    /* Ensure filename_out has room for .tmp suffix (4 bytes) */
    if (strlen(filename_out) + 4 > PATH_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }

    /* Check if already cached */
    if (!force_download && access(filename_out, R_OK) == 0) {
        printf("Using cached cart %s\n", filename_out);
        return 0;
    }

    /* Download from BBS */
    /* BBS cat=7 is for carts, play_src=2 is for direct cart download */
    if (bbs_start_get_cart(7, 2, cart_id) < 0)
        return -1;

    /* Create temporary file */
    snprintf(temp_filename, sizeof(temp_filename), "%s.tmp", filename_out);
    fp = fopen(temp_filename, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to open temporary file '%s' for writing: %s\n", temp_filename, strerror(errno));
        bbs_close();
        return -1;
    }

    /* Download data */
    total_bytes_read = 0;
    while ((bytes_read = bbs_recv(buffer, CACHE_BUFFER_SIZE)) > 0) {
        total_bytes_read += bytes_read;
        if (fwrite(buffer, 1, bytes_read, fp) != bytes_read) {
            fclose(fp);
            unlink(temp_filename);
            bbs_close();
            errno = EIO;
            return -1;
        }
    }

    if (bytes_read < 0 || total_bytes_read == 0) {
        fclose(fp);
        unlink(temp_filename);
        bbs_close();
        return -1;
    }

    fclose(fp);
    bbs_close();

    /* Rename temp file to final name */
    if (unlink(filename_out) < 0 && errno != ENOENT) {
        fprintf(stderr, "Failed to unlink '%s': %s\n", filename_out, strerror(errno));
        unlink(temp_filename);
        return -1;
    }
    if (rename(temp_filename, filename_out) < 0) {
        fprintf(stderr, "Failed to rename '%s' to '%s': %s\n", temp_filename, filename_out, strerror(errno));
        unlink(temp_filename);
        return -1;
    }

    printf("Downloaded cart to %s\n", filename_out);

    return 0;
}

#endif
