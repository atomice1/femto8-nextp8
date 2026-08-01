/**
 * http_stubs.c - Stub implementations for external dependencies
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* Stub for net_lookup_domain (used by http_start_get) */
int net_lookup_domain(const char *domain, char *ip, size_t ip_len)
{
    (void)domain;
    /* Return a fake IP address for testing */
    snprintf(ip, ip_len, "127.0.0.1");
    return 0;
}

/* Stub for net_start_tcp (used by http_start_get) */
int net_start_tcp(const char *ip, unsigned port)
{
    (void)ip;
    (void)port;
    /* Return success for testing */
    return 0;
}

/* Stub for net_start_ssl (used by http_start_get) */
int net_start_ssl(const char *ip, unsigned port, const char *host)
{
    (void)ip;
    (void)port;
    (void)host;
    /* Return success for testing */
    return 0;
}

/* Stub for net_send (used by http_start_get) */
int net_send(const void *data, size_t len)
{
    (void)data;
    (void)len;
    /* Return success for testing */
    return len;
}

/* Stub for net_recv (used by http_recv) */
ssize_t net_recv(void *data, size_t len)
{
    (void)data;
    (void)len;
    /* Return 0 (EOF) for testing */
    return 0;
}

/* Stub for net_close (used by http_close) */
int net_close(void)
{
    /* Return success for testing */
    return 0;
}
