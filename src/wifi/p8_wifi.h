/**
 * Copyright (C) 2026 Chris January
 *
 * Wi-Fi configuration interface for ESP8266 AT commands
 */

#ifndef P8_WIFI_H
#define P8_WIFI_H

#include <stddef.h>
#include <stdbool.h>

/* Maximum SSID length (standard Wi-Fi max is 32) */
#define WIFI_MAX_SSID_LEN   32

/* Maximum password length (WPA/WPA2 max is 64) */
#define WIFI_MAX_PASSWORD_LEN 64

/* Maximum BSSID string length (MAC address as xx:xx:xx:xx:xx:xx) */
#define WIFI_MAX_BSSID_LEN  18

/* Wi-Fi encryption types */
typedef enum {
    WIFI_ENCRYPT_OPEN = 0,
    WIFI_ENCRYPT_WEP = 1,
    WIFI_ENCRYPT_WPA_PSK = 2,
    WIFI_ENCRYPT_WPA2_PSK = 3,
    WIFI_ENCRYPT_WPA_WPA2_PSK = 4
} wifi_encrypt_t;

/* Wi-Fi access point information */
typedef struct {
    char ssid[WIFI_MAX_SSID_LEN + 1];      // Network name
    char bssid[WIFI_MAX_BSSID_LEN + 1];    // MAC address
    wifi_encrypt_t encrypt;                 // Encryption type
    int rssi;                               // Signal strength (dBm)
    int channel;                            // Channel number
} wifi_ap_info_t;

/**
 * Scan for available Wi-Fi access points.
 *
 * @param aps Output buffer for AP information
 * @param max_aps Maximum number of APs to return
 * @param count Output parameter for actual number of APs found
 * @return 0 on success, -1 on failure (check errno)
 */
int wifi_scan_aps(wifi_ap_info_t *aps, int max_aps, int *count);

/**
 * Connect to a Wi-Fi access point.
 *
 * @param ssid Network name to connect to
 * @param password Network password (NULL or empty for open networks)
 * @param bssid Optional MAC address to select specific AP (can be NULL)
 * @return 0 on success, -1 on failure (check errno)
 *         Error codes: 1 = timeout, 2 = wrong password, 3 = AP not found, 4 = connect fail
 */
int wifi_connect(const char *ssid, const char *password, const char *bssid);

/**
 * Disconnect from current Wi-Fi access point.
 *
 * @return 0 on success, -1 on failure (check errno)
 */
int wifi_disconnect(void);

/**
 * Get current Wi-Fi connection status and info.
 *
 * @param info Output buffer for current AP info (can be NULL)
 * @return 0 if connected, -1 if disconnected
 */
int wifi_get_status(wifi_ap_info_t *info);

/**
 * Get current station IP address.
 *
 * @param ip_address Output buffer for IP address string (minimum 16 bytes)
 * @param ip_address_len Size of output buffer
 * @return 0 if has valid IP, -1 if no IP or error
 */
int wifi_get_ip_address(char *ip_address, size_t ip_address_len);

/**
 * Get encryption type name as string.
 *
 * @param encrypt Encryption type
 * @return String representation (e.g., "WPA2-PSK")
 */
const char *wifi_encrypt_to_string(wifi_encrypt_t encrypt);

#endif /* P8_WIFI_H */
