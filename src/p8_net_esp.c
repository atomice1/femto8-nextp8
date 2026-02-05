/**
 * Copyright (C) 2026 Chris January
 *
 * Network interface for ESP8266 via AT commands (nextp8 platform)
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "p8_emu.h"
#include "p8_net.h"

#ifdef NEXTP8

#include "mmio.h"
#include "nextp8.h"

/* AT command timeout (in microseconds) */
#define AT_TIMEOUT_US 5000000  /* 5 seconds */

/* Buffer sizes */
#define AT_RESPONSE_BUF_SIZE 512
#define AT_COMMAND_BUF_SIZE  256

/* Connection state */
static bool connection_active = false;
static bool is_ssl_connection = false;

int net_lookup_domain(const char *domain_name, char *ip_address, size_t ip_address_len)
{
    char cmd[AT_COMMAND_BUF_SIZE];
    char response[AT_RESPONSE_BUF_SIZE];

    if (_esp_init() < 0) {
        return -1;
    }

    /* Send DNS query: AT+CIPDOMAIN="domain" */
    snprintf(cmd, sizeof(cmd), "AT+CIPDOMAIN=\"%s\"", domain_name);

    if (_esp_write_string(cmd) < 0) {
        return -1;
    }
    if (_esp_write_string("\r\n") < 0) {
        return -1;
    }

    /* Read response: +CIPDOMAIN:<IP address> */
    uint64_t start_time = MMIO_REG64(_UTIMER_1MHZ);

    while (1) {
        uint64_t elapsed = MMIO_REG64(_UTIMER_1MHZ) - start_time;
        if (elapsed >= AT_TIMEOUT_US) {
            errno = ETIMEDOUT;
            return -1;
        }

        if (_esp_read_line(response, sizeof(response), AT_TIMEOUT_US - elapsed) < 0) {
            return -1;
        }

        /* Parse +CIPDOMAIN:<IP> */
        if (strncmp(response, "+CIPDOMAIN:", 11) == 0) {
            const char *ip_start = response + 11;
            size_t ip_len = strlen(ip_start);

            if (ip_len >= ip_address_len) {
                errno = EOVERFLOW;
                return -1;
            }

            strcpy(ip_address, ip_start);

            /* Wait for OK */
            _esp_send_at_command("", "OK", AT_TIMEOUT_US);
            return 0;
        }

        if (strcmp(response, "ERROR") == 0 || strcmp(response, "FAIL") == 0) {
            errno = EINVAL;
            return -1;
        }
    }
}

int net_start_tcp(const char *remote_ip, unsigned remote_port)
{
    char cmd[AT_COMMAND_BUF_SIZE];

    if (connection_active) {
        errno = EISCONN;
        return -1;
    }

    if (_esp_init() < 0) {
        return -1;
    }

    /* AT+CIPSTART="TCP","<IP>",<port> */
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u", remote_ip, remote_port);

    if (_esp_send_at_command(cmd, "CONNECT", AT_TIMEOUT_US) < 0) {
        errno = ECONNREFUSED;
        return -1;
    }

    connection_active = true;
    is_ssl_connection = false;
    return 0;
}

int net_start_ssl(const char *remote_ip, unsigned remote_port, const char *hostname)
{
    char cmd[AT_COMMAND_BUF_SIZE];

    if (connection_active) {
        errno = EISCONN;
        return -1;
    }

    if (_esp_init() < 0) {
        return -1;
    }

    /* Set SSL buffer size */
    if (_esp_send_at_command("AT+CIPSSLSIZE=4096", "OK", AT_TIMEOUT_US) < 0) {
        return -1;
    }

    /* AT+CIPSTART="SSL","<hostname>",<port> - use hostname for SNI */
    const char *host = hostname ? hostname : remote_ip;
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"SSL\",\"%s\",%u", host, remote_port);

    if (_esp_send_at_command(cmd, "CONNECT", AT_TIMEOUT_US) < 0) {
        errno = ECONNREFUSED;
        return -1;
    }

    connection_active = true;
    is_ssl_connection = true;
    return 0;
}

int net_send(const void *data, unsigned length)
{
    char cmd[AT_COMMAND_BUF_SIZE];
    const unsigned char *ptr = (const unsigned char *)data;

    if (!connection_active) {
        errno = ENOTCONN;
        return -1;
    }

    /* AT+CIPSEND=<length> */
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", length);

    if (_esp_write_string(cmd) < 0) {
        return -1;
    }
    if (_esp_write_string("\r\n") < 0) {
        return -1;
    }

    /* Wait for ">" prompt */
    if (_esp_wait_for_prompt(">", AT_TIMEOUT_US) < 0) {
        return -1;
    }

    /* Send data */
    for (unsigned i = 0; i < length; i++) {
        if (_esp_write_byte(ptr[i]) < 0) {
            return -1;
        }
    }

    /* Wait for SEND OK */
    if (_esp_send_at_command("", "SEND OK", AT_TIMEOUT_US) < 0) {
        return -1;
    }

    return 0;
}

ssize_t net_recv(void *data, unsigned max_length)
{
    static unsigned char overflow_buffer[8192];
    static size_t overflow_len = 0;
    static size_t overflow_pos = 0;
    static bool pending_eof = false;  /* Connection closed, need to report EOF */

    unsigned char *ptr = (unsigned char *)data;
    size_t received = 0;

    if (!connection_active) {
        errno = ENOTCONN;
        return -1;
    }

    /* First, copy any buffered overflow data from previous call */
    if (overflow_pos < overflow_len) {
        size_t available = overflow_len - overflow_pos;
        size_t to_copy = (available < max_length) ? available : max_length;
        memcpy(ptr, overflow_buffer + overflow_pos, to_copy);
        overflow_pos += to_copy;
        received = to_copy;

        /* Reset buffer if fully consumed */
        if (overflow_pos >= overflow_len) {
            overflow_pos = 0;
            overflow_len = 0;
        }

        /* If we've filled the caller's buffer, return now */
        if (received >= max_length) {
            return received;
        }
        /* Otherwise continue reading to fill the rest of the buffer */
    }

    /* If we have a pending EOF and no buffered data, return 0 now */
    if (pending_eof) {
        if (received == 0) {
            connection_active = false;
            pending_eof = false;
        }
        return received;
    }

    /* State machine to find +IPD,<length>: pattern */
    uint64_t start_time = MMIO_REG64(_UTIMER_1MHZ);
    enum { LOOKING_FOR_PLUS, LOOKING_FOR_I, LOOKING_FOR_P, LOOKING_FOR_D, LOOKING_FOR_COMMA,
           READING_LENGTH, READING_DATA } state = LOOKING_FOR_PLUS;
    unsigned data_len = 0;
    unsigned data_read = 0;  /* Bytes read from current +IPD message */
    int closed_pos = 0;

    while (1) {
        uint64_t elapsed = MMIO_REG64(_UTIMER_1MHZ) - start_time;
        if (elapsed >= AT_TIMEOUT_US) {
            /* No data available - this is not an error for recv */
            return 0;
        }

        unsigned char ch;
        if (_esp_read_byte(&ch, AT_TIMEOUT_US - elapsed) < 0) {
            continue;
        }

        /* Check for CLOSED pattern in parallel */
        if (closed_pos < 6) {
            const char *closed = "CLOSED";
            if (ch == closed[closed_pos]) {
                closed_pos++;
                if (closed_pos == 6) {
                    if (received > 0)
                        pending_eof = true;
                    else
                        connection_active = false;
                    assert(overflow_pos == 0);
                    return received;
                }
            } else if (ch == 'C') {
                closed_pos = 1;
            } else {
                closed_pos = 0;
            }
        }

        /* State machine for +IPD,<len>: */
        switch (state) {
            case LOOKING_FOR_PLUS:
                if (ch == '+') state = LOOKING_FOR_I;
                break;
            case LOOKING_FOR_I:
                if (ch == 'I') state = LOOKING_FOR_P;
                else if (ch == '+') state = LOOKING_FOR_I;
                else state = LOOKING_FOR_PLUS;
                break;
            case LOOKING_FOR_P:
                if (ch == 'P') state = LOOKING_FOR_D;
                else if (ch == '+') state = LOOKING_FOR_I;
                else state = LOOKING_FOR_PLUS;
                break;
            case LOOKING_FOR_D:
                if (ch == 'D') state = LOOKING_FOR_COMMA;
                else if (ch == '+') state = LOOKING_FOR_I;
                else state = LOOKING_FOR_PLUS;
                break;
            case LOOKING_FOR_COMMA:
                if (ch == ',') {
                    state = READING_LENGTH;
                    data_len = 0;
                } else if (ch == '+') {
                    state = LOOKING_FOR_I;
                } else {
                    state = LOOKING_FOR_PLUS;
                }
                break;
            case READING_LENGTH:
                if (ch >= '0' && ch <= '9') {
                    data_len = data_len * 10 + (ch - '0');
                } else if (ch == ':') {
                    if (data_len == 0) {
                        errno = EIO;
                        return -1;
                    }
                    state = READING_DATA;
                    data_read = 0;  /* Reset for new message */
                } else {
                    state = LOOKING_FOR_PLUS;
                }
                break;
            case READING_DATA:
                if (received < max_length) {
                    ptr[received++] = ch;
                    data_read++;
                } else if (overflow_len < sizeof(overflow_buffer)) {
                    /* Caller's buffer is full, save to overflow buffer */
                    overflow_buffer[overflow_len++] = ch;
                    data_read++;
                } else {
                    /* Both buffers full - this is an error */
                    errno = ENOBUFS;
                    return -1;
                }

                if (data_read >= data_len) {
                    /* Check if we have read enough data */
                    if (received >= max_length) {
                        /* Buffer is full, return what we have (overflow will be returned next call) */
                        assert(overflow_pos == 0);
                        return received;
                    }

                    /* Reset state variables after completing a message */
                    state = LOOKING_FOR_PLUS;
                    data_len = 0;
                    data_read = 0;
                    start_time = MMIO_REG64(_UTIMER_1MHZ);  /* Reset timeout for next message */
                }
                break;
        }
    }
}

int net_close(void)
{
    if (!connection_active) {
        errno = ENOTCONN;
        return -1;
    }

    /* AT+CIPCLOSE */
    _esp_send_at_command("AT+CIPCLOSE", "CLOSED", AT_TIMEOUT_US);

    connection_active = false;
    is_ssl_connection = false;
    return 0;
}

#endif /* ENABLE_BBS_DOWNLOAD */
