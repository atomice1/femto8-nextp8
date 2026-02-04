/**
 * Copyright (C) 2026 Chris January
 *
 * HTTP client interface
 */

#include "p8_emu.h"
#include "p8_http.h"
#include "p8_net.h"
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ENABLE_BBS_DOWNLOAD

/* HTTP response state */
static bool headers_received = false;
static bool chunked_encoding = false;
static size_t content_length = 0;
static size_t bytes_received = 0;
static size_t current_chunk_remaining = 0;
static int http_status_code = 0;

/* Buffer for partial reads */
#define READ_BUFFER_SIZE 4096
static unsigned char read_buffer[READ_BUFFER_SIZE];
static size_t read_buffer_pos = 0;
static size_t read_buffer_len = 0;

/* Parse URL into components */
static int parse_url(const char *url, bool *use_ssl, char *host, size_t host_len,
                     unsigned *port, char *path, size_t path_len)
{
    const char *p = url;

    /* Check protocol */
    if (strncmp(p, "http://", 7) == 0) {
        *use_ssl = false;
        *port = 80;
        p += 7;
    } else if (strncmp(p, "https://", 8) == 0) {
        *use_ssl = true;
        *port = 443;
        p += 8;
    } else {
        errno = EINVAL;
        return -1;
    }

    /* Extract host */
    const char *host_start = p;
    const char *host_end = p;
    while (*host_end && *host_end != ':' && *host_end != '/')
        host_end++;

    size_t host_size = host_end - host_start;
    if (host_size == 0 || host_size >= host_len) {
        errno = EINVAL;
        return -1;
    }

    strncpy(host, host_start, host_size);
    host[host_size] = '\0';
    p = host_end;

    /* Extract port if present */
    if (*p == ':') {
        p++;
        *port = 0;
        while (isdigit(*p)) {
            *port = *port * 10 + (*p - '0');
            p++;
        }
        if (*port == 0 || *port > 65535) {
            errno = EINVAL;
            return -1;
        }
    }

    /* Extract path */
    if (*p == '\0' || *p == '/') {
        const char *path_start = *p ? p : "/";
        if (strlen(path_start) >= path_len) {
            errno = EINVAL;
            return -1;
        }
        strcpy(path, path_start);
    } else {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

/* Map HTTP status codes to errno values */
static int http_status_to_errno(int status_code)
{
    switch (status_code) {
        case 400: /* Bad Request */
            return EINVAL;
        case 401: /* Unauthorized */
        case 403: /* Forbidden */
            return EACCES;
        case 404: /* Not Found */
            return ENOENT;
        case 408: /* Request Timeout */
        case 504: /* Gateway Timeout */
            return ETIMEDOUT;
        case 500: /* Internal Server Error */
            return EIO;
        case 502: /* Bad Gateway */
            return EHOSTUNREACH;
        case 503: /* Service Unavailable */
            return EAGAIN;
        default:
            /* For other errors, use generic error */
            if (status_code >= 400 && status_code < 500)
                return EINVAL;  /* Client errors */
            else if (status_code >= 500)
                return EIO;     /* Server errors */
            else
                return EINVAL;  /* Unexpected status */
    }
}

/* Read a line from the connection */
static ssize_t read_line(char *buffer, size_t buffer_size)
{
    size_t pos = 0;

    while (pos < buffer_size - 1) {
        unsigned char ch;

        if (read_buffer_pos >= read_buffer_len) {
            /* Refill buffer */
            ssize_t n = net_recv(read_buffer, READ_BUFFER_SIZE);
            if (n <= 0) {
                return n;
            }
            read_buffer_len = n;
            read_buffer_pos = 0;
        }

        ch = read_buffer[read_buffer_pos++];

        if (ch == '\n') {
            buffer[pos] = '\0';
            /* Strip trailing \r if present */
            if (pos > 0 && buffer[pos - 1] == '\r')
                buffer[pos - 1] = '\0';
            return pos;
        }

        buffer[pos++] = ch;
    }

    errno = EOVERFLOW;
    return -1;
}

int http_start_get(const char *url)
{
    char host[256];
    char path[1024];
    char ip[64];
    unsigned port;
    bool use_ssl;

    /* Reset state */
    headers_received = false;
    chunked_encoding = false;
    content_length = 0;
    bytes_received = 0;
    current_chunk_remaining = 0;
    read_buffer_pos = 0;
    read_buffer_len = 0;
    http_status_code = 0;

    /* Parse URL */
    if (parse_url(url, &use_ssl, host, sizeof(host), &port, path, sizeof(path)) < 0) {
        return -1;
    }

    /* Resolve hostname */
    if (net_lookup_domain(host, ip, sizeof(ip)) < 0) {
        return -1;
    }

    /* Connect */
    if (use_ssl) {
        if (net_start_ssl(ip, port, host) < 0) {
            return -1;
        }
    } else {
        if (net_start_tcp(ip, port) < 0) {
            return -1;
        }
    }

    /* Send HTTP GET request */
    char request[2048];
    int len = snprintf(request, sizeof(request),
                      "GET %s HTTP/1.1\r\n"
                      "Host: %s\r\n"
                      "Connection: close\r\n"
                      "User-Agent: PICO-8\r\n"
                      "\r\n",
                      path, host);

    if (len < 0 || (size_t)len >= sizeof(request)) {
        net_close();
        errno = EOVERFLOW;
        return -1;
    }

    if (net_send(request, len) < 0) {
        net_close();
        return -1;
    }

    return 0;
}

ssize_t http_recv(void *data, unsigned max_length)
{
    /* Parse headers if not yet received */
    if (!headers_received) {
        char line[1024];
        ssize_t n;

        /* Skip any leading blank lines before the status line */
        do {
            n = read_line(line, sizeof(line));
            if (n < 0) {
                return -1;
            }
        } while (n == 0 || line[0] == '\0');

        /* Parse HTTP version and status code */
        if (sscanf(line, "HTTP/%*d.%*d %d", &http_status_code) != 1) {
            errno = EINVAL;
            return -1;
        }

        if (http_status_code < 200 || http_status_code >= 300) {
            errno = http_status_to_errno(http_status_code);
            return -1;
        }

        /* Read headers */
        while (1) {
            n = read_line(line, sizeof(line));
            if (n < 0) {
                return -1;
            }

            if (n == 0 || line[0] == '\0') {
                /* Empty line marks end of headers */
                break;
            }

            /* Check for Content-Length */
            if (strncasecmp(line, "Content-Length:", 15) == 0) {
                content_length = atol(line + 15);
            }

            /* Check for Transfer-Encoding: chunked */
            if (strncasecmp(line, "Transfer-Encoding:", 18) == 0) {
                const char *value = line + 18;
                while (*value == ' ') value++;
                if (strcasecmp(value, "chunked") == 0) {
                    chunked_encoding = true;
                }
            }
        }

        headers_received = true;
    }

    /* Read body data */
    if (chunked_encoding) {
        /* Handle chunked transfer encoding */
        if (current_chunk_remaining == 0) {
            /* Read chunk size */
            char line[64];
            ssize_t n = read_line(line, sizeof(line));
            if (n < 0) {
                return -1;
            }

            /* Parse hex chunk size */
            current_chunk_remaining = strtol(line, NULL, 16);

            if (current_chunk_remaining == 0) {
                /* Last chunk */
                return 0;
            }
        }

        /* Read data from current chunk */
        size_t to_read = max_length;
        if (to_read > current_chunk_remaining)
            to_read = current_chunk_remaining;

        ssize_t n = 0;
        unsigned char *ptr = (unsigned char *)data;

        while (n < (ssize_t)to_read) {
            if (read_buffer_pos >= read_buffer_len) {
                ssize_t recv_len = net_recv(read_buffer, READ_BUFFER_SIZE);
                if (recv_len <= 0) {
                    return recv_len;
                }
                read_buffer_len = recv_len;
                read_buffer_pos = 0;
            }
            size_t available = read_buffer_len - read_buffer_pos;
            size_t to_copy = (available < (to_read - n)) ? available : (to_read - n);
            memcpy(ptr + n, read_buffer + read_buffer_pos, to_copy);
            read_buffer_pos += to_copy;
            n += to_copy;
        }

        current_chunk_remaining -= n;

        /* If chunk is complete, read trailing CRLF */
        if (current_chunk_remaining == 0) {
            char crlf[2];
            if (read_buffer_pos < read_buffer_len) {
                read_buffer_pos++;
            } else {
                net_recv(crlf, 1);
            }
            if (read_buffer_pos < read_buffer_len) {
                read_buffer_pos++;
            } else {
                net_recv(crlf, 1);
            }
        }

        return n;
    } else {
        /* Normal content-length based transfer */
        size_t to_read = max_length;
        if (content_length > 0 && bytes_received + to_read > content_length) {
            to_read = content_length - bytes_received;
        }

        if (to_read == 0) {
            return 0;
        }

        ssize_t n = 0;
        unsigned char *ptr = (unsigned char *)data;

        /* First use buffered data */
        if (read_buffer_pos < read_buffer_len) {
            size_t available = read_buffer_len - read_buffer_pos;
            size_t to_copy = (available < to_read) ? available : to_read;
            memcpy(ptr, read_buffer + read_buffer_pos, to_copy);
            read_buffer_pos += to_copy;
            n = to_copy;
        }

        /* Then read directly */
        if (n < (ssize_t)to_read) {
            ssize_t recv_len = net_recv(ptr + n, to_read - n);
            if (recv_len < 0) {
                return -1;
            }
            n += recv_len;
        }

        bytes_received += n;
        return n;
    }
}

int http_get_status_code(void)
{
    return http_status_code;
}

int http_close(void)
{
    return net_close();
}

#endif
