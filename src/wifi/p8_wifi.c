/**
 * Copyright (C) 2026 Chris January
 *
 * Wi-Fi configuration interface for ESP8266 AT commands
 */

#include "p8_wifi.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

#ifdef NEXTP8

#include "nextp8.h"
#include "mmio.h"

/* AT command timeout (in microseconds) */
#define AT_TIMEOUT_US 10000000  /* 10 seconds for Wi-Fi operations */

/* Buffer sizes */
#define AT_RESPONSE_BUF_SIZE 256

/* Scan for available Wi-Fi access points */
int wifi_scan_aps(wifi_ap_info_t *aps, int max_aps, int *count)
{
    char response[AT_RESPONSE_BUF_SIZE];
    int ap_count = 0;

    if (_esp_init() < 0) {
        return -1;
    }

    /* Send AT+CWLAP command to list APs */
    if (_esp_write_string("AT+CWLAP\r\n") < 0) {
        return -1;
    }

    /* Read response lines */
    uint64_t start_time = MMIO_REG64(_UTIMER_1MHZ);

    while (ap_count < max_aps) {
        uint64_t elapsed = MMIO_REG64(_UTIMER_1MHZ) - start_time;
        if (elapsed >= AT_TIMEOUT_US) {
            errno = ETIMEDOUT;
            break;
        }

        if (_esp_read_line(response, sizeof(response), AT_TIMEOUT_US - elapsed) < 0) {
            break;
        }

        /* Check for end of list */
        if (strcmp(response, "OK") == 0) {
            break;
        }

        if (strcmp(response, "ERROR") == 0 || strcmp(response, "FAIL") == 0) {
            errno = EIO;
            return -1;
        }

        /* Parse +CWLAP:<ecn>,"<ssid>",<rssi>,"<mac>",<ch> */
        if (strncmp(response, "+CWLAP:", 7) == 0) {
            wifi_ap_info_t *ap = &aps[ap_count];
            const char *p = response + 7;

            /* Parse encryption type */
            int ecn = 0;
            while (*p && isdigit(*p)) {
                ecn = ecn * 10 + (*p - '0');
                p++;
            }
            ap->encrypt = (wifi_encrypt_t)ecn;

            /* Skip comma */
            if (*p == ',') p++;

            /* Parse SSID (quoted string) */
            if (*p == '"') {
                p++;
                int i = 0;
                while (*p && *p != '"' && i < WIFI_MAX_SSID_LEN) {
                    /* Handle escape sequences */
                    if (*p == '\\' && *(p + 1)) {
                        p++;
                    }
                    ap->ssid[i++] = *p++;
                }
                ap->ssid[i] = '\0';
                if (*p == '"') p++;
            }

            /* Skip comma */
            if (*p == ',') p++;

            /* Parse RSSI */
            int rssi = 0;
            bool negative = false;
            if (*p == '-') {
                negative = true;
                p++;
            }
            while (*p && isdigit(*p)) {
                rssi = rssi * 10 + (*p - '0');
                p++;
            }
            ap->rssi = negative ? -rssi : rssi;

            /* Skip comma */
            if (*p == ',') p++;

            /* Parse BSSID (quoted string) */
            if (*p == '"') {
                p++;
                int i = 0;
                while (*p && *p != '"' && i < WIFI_MAX_BSSID_LEN) {
                    ap->bssid[i++] = *p++;
                }
                ap->bssid[i] = '\0';
                if (*p == '"') p++;
            }

            /* Skip comma */
            if (*p == ',') p++;

            /* Parse channel */
            int ch = 0;
            while (*p && isdigit(*p)) {
                ch = ch * 10 + (*p - '0');
                p++;
            }
            ap->channel = ch;

            ap_count++;
        }
    }

    if (count) {
        *count = ap_count;
    }

    return 0;
}

/* Connect to Wi-Fi access point */
int wifi_connect(const char *ssid, const char *password, const char *bssid)
{
    char cmd[256];
    char response[AT_RESPONSE_BUF_SIZE];

    if (_esp_init() < 0) {
        return -1;
    }

    /* Build AT+CWJAP_DEF command */
    if (bssid && bssid[0]) {
        snprintf(cmd, sizeof(cmd), "AT+CWJAP_DEF=\"%s\",\"%s\",\"%s\"",
                 ssid, password ? password : "", bssid);
    } else {
        snprintf(cmd, sizeof(cmd), "AT+CWJAP_DEF=\"%s\",\"%s\"",
                 ssid, password ? password : "");
    }

    /* Send command */
    if (_esp_write_string(cmd) < 0) {
        return -1;
    }
    if (_esp_write_string("\r\n") < 0) {
        return -1;
    }

    /* Wait for response (connection can take several seconds) */
    uint64_t start_time = MMIO_REG64(_UTIMER_1MHZ);

    while (1) {
        uint64_t elapsed = MMIO_REG64(_UTIMER_1MHZ) - start_time;
        if (elapsed >= AT_TIMEOUT_US) {
            errno = ETIMEDOUT;
            return -1;
        }

        if (_esp_read_line(response, sizeof(response), AT_TIMEOUT_US - elapsed) < 0) {
            continue;
        }

        /* Check for success */
        if (strcmp(response, "OK") == 0) {
            return 0;
        }

        /* Check for error with code */
        if (strncmp(response, "+CWJAP:", 7) == 0) {
            /* Parse error code */
            int error_code = 0;
            const char *p = response + 7;
            while (*p && isdigit(*p)) {
                error_code = error_code * 10 + (*p - '0');
                p++;
            }

            /* Set errno based on error code */
            switch (error_code) {
                case 1: errno = ETIMEDOUT; break;   /* Connection timeout */
                case 2: errno = EACCES; break;      /* Wrong password */
                case 3: errno = ENOENT; break;      /* AP not found */
                case 4:
                default: errno = ECONNREFUSED; break; /* Connection failed */
            }

            /* Wait for FAIL */
            _esp_read_line(response, sizeof(response), AT_TIMEOUT_US - elapsed);
            return -1;
        }

        if (strcmp(response, "FAIL") == 0 || strcmp(response, "ERROR") == 0) {
            printf("EIO: %s:%d\n", __FILE__, __LINE__);
            errno = EIO;
            return -1;
        }
    }
}

/* Disconnect from Wi-Fi */
int wifi_disconnect(void)
{
    if (_esp_init() < 0) {
        return -1;
    }

    return _esp_send_at_command("AT+CWQAP", "OK", AT_TIMEOUT_US);
}

/* Get current Wi-Fi status */
int wifi_get_status(wifi_ap_info_t *info)
{
    char response[AT_RESPONSE_BUF_SIZE];

    if (_esp_init() < 0) {
        return -1;
    }

    /* Query current connection: AT+CWJAP? */
    if (_esp_write_string("AT+CWJAP?\r\n") < 0) {
        return -1;
    }

    /* Read response */
    uint64_t start_time = MMIO_REG64(_UTIMER_1MHZ);
    bool no_ap = false;

    while (1) {
        uint64_t elapsed = MMIO_REG64(_UTIMER_1MHZ) - start_time;
        if (elapsed >= AT_TIMEOUT_US) {
            return -1;
        }

        if (_esp_read_line(response, sizeof(response), AT_TIMEOUT_US - elapsed) < 0) {
            continue;
        }

        /* No AP connected returns "No AP" */
        if (strncmp(response, "No AP", 5) == 0) {
            no_ap = true;
            continue;
        }

        /* Parse +CWJAP:"<ssid>","<bssid>",<channel>,<rssi> */
        if (strncmp(response, "+CWJAP:", 7) == 0 && info) {
            const char *p = response + 7;

            /* Parse SSID */
            if (*p == '"') {
                p++;
                int i = 0;
                while (*p && *p != '"' && i < WIFI_MAX_SSID_LEN) {
                    if (*p == '\\' && *(p + 1)) p++;
                    info->ssid[i++] = *p++;
                }
                info->ssid[i] = '\0';
                if (*p == '"') p++;
            }

            if (*p == ',') p++;

            /* Parse BSSID */
            if (*p == '"') {
                p++;
                int i = 0;
                while (*p && *p != '"' && i < WIFI_MAX_BSSID_LEN) {
                    info->bssid[i++] = *p++;
                }
                info->bssid[i] = '\0';
                if (*p == '"') p++;
            }

            if (*p == ',') p++;

            /* Parse channel */
            int ch = 0;
            while (*p && isdigit(*p)) {
                ch = ch * 10 + (*p - '0');
                p++;
            }
            info->channel = ch;

            if (*p == ',') p++;

            /* Parse RSSI */
            int rssi = 0;
            bool negative = false;
            if (*p == '-') {
                negative = true;
                p++;
            }
            while (*p && isdigit(*p)) {
                rssi = rssi * 10 + (*p - '0');
                p++;
            }
            info->rssi = negative ? -rssi : rssi;

            /* Encryption type not provided by CWJAP? - set to unknown */
            info->encrypt = WIFI_ENCRYPT_WPA2_PSK;
        }

        if (strcmp(response, "OK") == 0) {
            return no_ap ? -1 : 0;
        }

        if (strcmp(response, "ERROR") == 0) {
            return -1;
        }
    }
}

/* Get current station IP address */
int wifi_get_ip_address(char *ip_address, size_t ip_address_len)
{
    char response[AT_RESPONSE_BUF_SIZE];

    if (_esp_init() < 0) {
        return -1;
    }

    /* Query IP address: AT+CIFSR */
    if (_esp_write_string("AT+CIFSR\r\n") < 0) {
        return -1;
    }

    /* Read response */
    uint64_t start_time = MMIO_REG64(_UTIMER_1MHZ);
    bool found_ip = false;

    while (1) {
        uint64_t elapsed = MMIO_REG64(_UTIMER_1MHZ) - start_time;
        if (elapsed >= AT_TIMEOUT_US) {
            return -1;
        }

        if (_esp_read_line(response, sizeof(response), AT_TIMEOUT_US - elapsed) < 0) {
            continue;
        }

        /* Parse +CIFSR:STAIP,"<ip>" */
        if (strncmp(response, "+CIFSR:STAIP,\"", 14) == 0) {
            const char *ip_start = response + 14;
            const char *ip_end = strchr(ip_start, '"');
            if (ip_end) {
                size_t ip_len = ip_end - ip_start;
                if (ip_len < ip_address_len) {
                    memcpy(ip_address, ip_start, ip_len);
                    ip_address[ip_len] = '\0';

                    /* Check if IP is valid (not 0.0.0.0) */
                    if (strcmp(ip_address, "0.0.0.0") != 0) {
                        found_ip = true;
                    }
                }
            }
        }

        if (strcmp(response, "OK") == 0) {
            return found_ip ? 0 : -1;
        }

        if (strcmp(response, "ERROR") == 0) {
            return -1;
        }
    }
}

/* Get encryption type string */
const char *wifi_encrypt_to_string(wifi_encrypt_t encrypt)
{
    switch (encrypt) {
        case WIFI_ENCRYPT_OPEN: return "Open";
        case WIFI_ENCRYPT_WEP: return "WEP";
        case WIFI_ENCRYPT_WPA_PSK: return "WPA-PSK";
        case WIFI_ENCRYPT_WPA2_PSK: return "WPA2-PSK";
        case WIFI_ENCRYPT_WPA_WPA2_PSK: return "WPA/WPA2-PSK";
        default: return "Unknown";
    }
}

#endif /* NEXTP8 */
