/**
 * Copyright (C) 2026 Chris January
 *
 * Wi-Fi configuration UI based on p8_dialog
 */

#ifndef P8_WIFI_CONFIG_H
#define P8_WIFI_CONFIG_H

#include <stdbool.h>

/**
 * Show Wi-Fi configuration dialog.
 *
 * Allows user to:
 * - Scan for available access points
 * - Select an access point from list
 * - Enter password (if required)
 * - Connect to selected network
 *
 * @return true if connected successfully, false if cancelled or failed
 */
bool wifi_show_config_dialog(void);

/**
 * Wait for Wi-Fi connection to be established.
 *
 * Checks if an AP is configured, and if so, waits for:
 * - Connection to the AP
 * - IP address from DHCP
 *
 * Shows a dialog while waiting that auto-accepts when connected.
 *
 * @return true if connected successfully, false if cancelled or not configured
 */
bool wifi_wait_for_connected(void);

#endif /* P8_WIFI_CONFIG_H */
