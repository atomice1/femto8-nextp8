/**
 * Copyright (C) 2026 Chris January
 *
 * Network interface for POSIX sockets (Linux/Mac) and Winsock (Windows)
 */

#include <stdbool.h>
#include <string.h>
#include "p8_emu.h"
#include "p8_net.h"

#if defined(ENABLE_BBS_DOWNLOAD) && !defined(NEXTP8)

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#define close(s) closesocket(s)
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <openssl/err.h>
#include <openssl/ssl.h>

/* Connection state */
static int sockfd = -1;
static SSL_CTX *ssl_ctx = NULL;
static SSL *ssl = NULL;
static bool is_ssl_connection = false;

/* Initialize SSL library (called once) */
static void net_ssl_init(void)
{
    static bool initialized = false;
    if (!initialized) {
#ifdef _WIN32
        WSADATA wsa_data;
        WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
        initialized = true;
    }
}

int net_lookup_domain(const char *domain_name, char *ip_address, size_t ip_address_len)
{
    struct addrinfo hints, *res;
    int status;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(domain_name, NULL, &hints, &res);
    if (status != 0) {
        errno = EINVAL;
        return -1;
    }

    struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
    const char *ip = inet_ntoa(ipv4->sin_addr);

    if (strlen(ip) >= ip_address_len) {
        freeaddrinfo(res);
        errno = EOVERFLOW;
        return -1;
    }

    strcpy(ip_address, ip);
    freeaddrinfo(res);

    return 0;
}

int net_start_tcp(const char *remote_ip, unsigned remote_port)
{
    struct sockaddr_in server_addr;

    if (sockfd >= 0) {
        errno = EISCONN;
        return -1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(remote_port);

    if (inet_pton(AF_INET, remote_ip, &server_addr.sin_addr) <= 0) {
        close(sockfd);
        sockfd = -1;
        errno = EINVAL;
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        sockfd = -1;
        return -1;
    }

    is_ssl_connection = false;
    return 0;
}

int net_start_ssl(const char *remote_ip, unsigned remote_port, const char *hostname)
{
    if (sockfd >= 0) {
        errno = EISCONN;
        return -1;
    }

    net_ssl_init();

    /* Create TCP connection first */
    if (net_start_tcp(remote_ip, remote_port) < 0) {
        return -1;
    }

    /* Create SSL context */
    ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!ssl_ctx) {
        close(sockfd);
        sockfd = -1;
        errno = ENOMEM;
        return -1;
    }

    /* Disable certificate verification for now (insecure but avoids need for CA) */
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);

    /* Create SSL object */
    ssl = SSL_new(ssl_ctx);
    if (!ssl) {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = NULL;
        close(sockfd);
        sockfd = -1;
        errno = ENOMEM;
        return -1;
    }

    /* Attach socket to SSL */
    if (SSL_set_fd(ssl, sockfd) != 1) {
        SSL_free(ssl);
        ssl = NULL;
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = NULL;
        close(sockfd);
        sockfd = -1;
        errno = EIO;
        return -1;
    }

    /* Set hostname for SNI (Server Name Indication) */
    if (SSL_set_tlsext_host_name(ssl, hostname) != 1) {
        /* Continue anyway, SNI is optional in theory */
    }

    /* Perform SSL handshake */
    if (SSL_connect(ssl) != 1) {
        SSL_free(ssl);
        ssl = NULL;
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = NULL;
        close(sockfd);
        sockfd = -1;
        errno = ECONNREFUSED;
        return -1;
    }

    is_ssl_connection = true;
    return 0;
}

int net_send(const void *data, unsigned length)
{
    if (sockfd < 0) {
        errno = ENOTCONN;
        return -1;
    }

    ssize_t sent = 0;
    const unsigned char *ptr = (const unsigned char *)data;

    while (sent < length) {
        ssize_t n;

        if (is_ssl_connection) {
            n = SSL_write(ssl, ptr + sent, length - sent);
            if (n <= 0) {
                errno = EIO;
                return -1;
            }
        } else {
            n = send(sockfd, ptr + sent, length - sent, 0);
            if (n < 0) {
                return -1;
            }
        }

        sent += n;
    }

    return 0;
}

ssize_t net_recv(void *data, unsigned max_length)
{
    if (sockfd < 0) {
        errno = ENOTCONN;
        return -1;
    }

    ssize_t n;

    if (is_ssl_connection) {
        n = SSL_read(ssl, data, max_length);
        if (n < 0) {
            errno = EIO;
            return -1;
        }
    } else {
        n = recv(sockfd, data, max_length, 0);
        if (n < 0) {
            return -1;
        }
    }

    return n;
}

int net_close(void)
{
    if (sockfd < 0) {
        errno = ENOTCONN;
        return -1;
    }

    if (is_ssl_connection && ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = NULL;
    }

    if (ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = NULL;
    }

    close(sockfd);
    sockfd = -1;
    is_ssl_connection = false;

    return 0;
}

#endif
