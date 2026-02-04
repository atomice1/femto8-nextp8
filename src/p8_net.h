/**
 * Copyright (C) 2026 Chris January
 * 
 * Network interface for POSIX sockets (Linux/Mac) and Winsock (Windows)
 */

#ifndef P8_NET_H
#define P8_NET_H

#include <stddef.h>
#ifndef _WIN32
#include <sys/types.h>
#endif

/**
 * Look up IP address for a domain name.
 * 
 * @param domain_name Domain name to resolve
 * @param ip_address Output buffer for IP address string
 * @param ip_address_len Size of ip_address buffer
 * @return 0 on success, -1 on failure (check errno)
 */
int net_lookup_domain(const char *domain_name, char *ip_address, size_t ip_address_len);

/**
 * Start TCP connection to remote host.
 * 
 * @param remote_ip Remote IP address
 * @param remote_port Remote port number
 * @return 0 on success, -1 on failure (check errno)
 *         -1 with errno=EISCONN if already connected
 */
int net_start_tcp(const char *remote_ip, unsigned remote_port);

/**
 * Start SSL/TLS connection to remote host.
 * 
 * @param remote_ip Remote IP address
 * @param remote_port Remote port number
 * @param hostname Hostname for SNI (Server Name Indication)
 * @return 0 on success, -1 on failure (check errno)
 *         -1 with errno=EISCONN if already connected
 */
int net_start_ssl(const char *remote_ip, unsigned remote_port, const char *hostname);

/**
 * Send data over the current connection.
 * 
 * @param data Data to send
 * @param length Number of bytes to send
 * @return 0 on success, -1 on failure (check errno)
 */
int net_send(const void *data, unsigned length);

/**
 * Receive data from the current connection.
 * 
 * @param data Output buffer for received data
 * @param max_length Maximum bytes to receive
 * @return >0 number of bytes received, 0 on EOF, -1 on failure (check errno)
 */
ssize_t net_recv(void *data, unsigned max_length);

/**
 * Close the current connection.
 * 
 * @return 0 on success, -1 on failure (check errno)
 */
int net_close(void);

#endif /* P8_NET_H */
