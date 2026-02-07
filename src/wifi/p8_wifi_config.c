/**
 * Copyright (C) 2026 Chris January
 *
 * Wi-Fi configuration UI based on p8_dialog
 */

#include "p8_dialog.h"
#include "p8_emu.h"
#include "p8_overlay_helper.h"
#include "p8_wifi.h"
#include "p8_wifi_config.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef NEXTP8

/* Maximum number of APs to display in list */
#define MAX_AP_LIST 20

/* UI state for Wi-Fi config */
typedef struct {
    wifi_ap_info_t aps[MAX_AP_LIST];
    int ap_count;
    int selected_ap;
    char password[WIFI_MAX_PASSWORD_LEN + 1];
    bool scanning;
    bool connecting;
} wifi_config_state_t;

static wifi_config_state_t wifi_state;

/* Custom render callback for AP list items */
static void render_ap_item(void *user_data, int index, bool selected, int x, int y, int width, int height, int fg_color, int bg_color)
{
    (void)user_data;
    (void)width;
    (void)height;

    if (index >= wifi_state.ap_count) {
        return;
    }

    const wifi_ap_info_t *ap = &wifi_state.aps[index];

    /* Draw background if selected */
    if (selected) {
        overlay_draw_rectfill(x, y - 1, x + width - 1, y + height - 1, bg_color);
    }

    /* Draw SSID */
    char display_text[64];
    snprintf(display_text, sizeof(display_text), "%s", ap->ssid);
    overlay_draw_simple_text(display_text, x, y, fg_color);

    /* Draw signal strength indicator (bars) */
    int bars = 0;
    if (ap->rssi >= -50) bars = 4;
    else if (ap->rssi >= -60) bars = 3;
    else if (ap->rssi >= -70) bars = 2;
    else if (ap->rssi >= -80) bars = 1;

    int bar_x = x + width - 11;
    for (int i = 0; i < 4; i++) {
        if (i < bars)
            overlay_draw_rectfill(bar_x + i * 3, y + height - (i == 0 ? 1 : i * 2),
                                  bar_x + i * 3 + 1, y + height - 1, fg_color);
    }

    /* Draw lock icon if encrypted */
    if (ap->encrypt != WIFI_ENCRYPT_OPEN) {
        int lock_x = bar_x - 6;
        overlay_draw_rectfill(lock_x, y + height - 4, lock_x + 4, y + height - 1, fg_color);
        overlay_draw_rect(lock_x + 1, y + height - 6, lock_x + 3, y + height - 4, fg_color);
    }
}

/* Scan for Wi-Fi networks */
static bool scan_wifi_networks(char *status_out, size_t status_len)
{
    /* Show scanning dialog */
    const char *scanning_msg = "scanning...";
    p8_dialog_control_t scanning_controls[] = {
        DIALOG_LABEL(scanning_msg)
    };

    p8_dialog_t scanning_dialog;
    p8_dialog_init(&scanning_dialog, "wi-fi configuration", scanning_controls,
                  sizeof(scanning_controls) / sizeof(scanning_controls[0]), 0);
    p8_dialog_set_showing(&scanning_dialog, true);
    p8_dialog_draw(&scanning_dialog);
    p8_flip();

    wifi_state.scanning = true;
    wifi_state.ap_count = 0;

    int result = wifi_scan_aps(wifi_state.aps, MAX_AP_LIST, &wifi_state.ap_count);

    wifi_state.scanning = false;

    p8_dialog_set_showing(&scanning_dialog, false);
    p8_dialog_cleanup(&scanning_dialog);

    if (result < 0) {
        snprintf(status_out, status_len, "scan failed");
        return false;
    }

    if (wifi_state.ap_count == 0) {
        snprintf(status_out, status_len, "no networks found");
        return false;
    }

    snprintf(status_out, status_len, "found %d network%s", wifi_state.ap_count,
             wifi_state.ap_count == 1 ? "" : "s");

    return true;
}

/* Wait for Wi-Fi connection and IP address */
static bool wifi_wait_for_connected_internal(const char *ssid, const char *password)
{
    wifi_ap_info_t ap_info;
    char ip_address[16];

    if (!ssid) {
        /* Check if we're already connected AND have IP */
        if (wifi_get_status(&ap_info) == 0 && wifi_get_ip_address(ip_address, sizeof(ip_address)) == 0) {
            /* Already connected and have IP */
            return true;
        }
    }

    /* Query if an AP is configured (AT+CWJAP?) */
    /* If wifi_get_status returns -1, could mean not connected or no AP configured */
    /* We'll show a waiting dialog and poll for connection */

    char status_message[64];
    strcpy(status_message, "waiting for connection...");

    p8_dialog_control_t waiting_controls[] = {
        DIALOG_LABEL(status_message),
        DIALOG_SPACING(),
        DIALOG_BUTTONBAR_CANCEL_ONLY()
    };

    p8_dialog_t waiting_dialog;
    p8_dialog_init(&waiting_dialog, "wi-fi connection", waiting_controls,
                  sizeof(waiting_controls) / sizeof(waiting_controls[0]), 0);
    p8_dialog_set_showing(&waiting_dialog, true);

    bool called_wifi_connect = false;
    bool connected = false;
    int poll_count = 0;
    const int max_polls = 100;  /* ~10 seconds at 100ms per poll */

    while (poll_count < max_polls) {
        /* Draw dialog */
        p8_dialog_draw(&waiting_dialog);
        p8_flip();

        if (!called_wifi_connect) {
            int result = wifi_connect(ssid, password, NULL);
            called_wifi_connect = true;
            if (result != 0)
                break;
        }

        /* Check for cancel button */
        p8_dialog_action_t action = p8_dialog_update(&waiting_dialog);
        if (action.type == DIALOG_RESULT_CANCELLED) {
            break;
        }

        /* Poll connection status - need both AP connection AND IP address */
        if (wifi_get_status(&ap_info) == 0 && wifi_get_ip_address(ip_address, sizeof(ip_address)) == 0) {
            connected = true;
            break;
        }

        /* Update status every 10 polls */
        if (poll_count % 10 == 0) {
            snprintf(status_message, sizeof(status_message),
                     "waiting for connection%s",
                     (poll_count / 10) % 4 == 0 ? "..." :
                     (poll_count / 10) % 4 == 1 ? ".  " :
                     (poll_count / 10) % 4 == 2 ? ".. " : "...");
        }

        /* Sleep for 100ms */
        usleep(100000);

        poll_count++;
    }

    p8_dialog_set_showing(&waiting_dialog, false);
    p8_dialog_cleanup(&waiting_dialog);

    return connected;
}

/* Show Wi-Fi configuration dialog */
bool wifi_show_config_dialog(void)
{
    /* Initialize state */
    memset(&wifi_state, 0, sizeof(wifi_state));
    wifi_state.selected_ap = 0;

    /* Initial scan */
    char status_message[64];
    if (!scan_wifi_networks(status_message, sizeof(status_message))) {
        /* Show error dialog */
        p8_dialog_control_t error_controls[] = {
            DIALOG_LABEL(status_message),
            DIALOG_SPACING(),
            DIALOG_BUTTONBAR_OK_ONLY()
        };

        p8_dialog_t error_dialog;
        p8_dialog_init(&error_dialog, "wi-fi configuration", error_controls,
                      sizeof(error_controls) / sizeof(error_controls[0]), 0);
        p8_dialog_set_showing(&error_dialog, true);

        p8_dialog_run(&error_dialog);

        p8_dialog_set_showing(&error_dialog, false);
        p8_dialog_cleanup(&error_dialog);
        return false;
    }

    /* Create network selection dialog */
    p8_dialog_control_t scan_controls[] = {
        {
            .type = DIALOG_LISTBOX,
            .label = NULL,
            .data = {
                .listbox = {
                    .user_data = NULL,
                    .item_count = wifi_state.ap_count,
                    .selected_index = &wifi_state.selected_ap,
                    .visible_lines = 8,
                    .scroll_offset = 0,
                    .draw_border = true,
                    .render_callback = render_ap_item
                }
            },
            .selectable = true,
            .enabled = true,
            .inverted = false
        },
        DIALOG_SPACING(),
        DIALOG_LABEL(status_message),
        DIALOG_SPACING(),
        DIALOG_BUTTONBAR()
    };

    p8_dialog_t scan_dialog;
    p8_dialog_init(&scan_dialog, "select wi-fi network", scan_controls,
                  sizeof(scan_controls) / sizeof(scan_controls[0]), 0);

    /* Show network selection */
    p8_dialog_action_t scan_result = p8_dialog_run(&scan_dialog);
    bool selected = (scan_result.type == DIALOG_RESULT_ACCEPTED);

    p8_dialog_cleanup(&scan_dialog);

    if (!selected) {
        return false;
    }

    /* Get selected AP */
    const wifi_ap_info_t *selected_ap = &wifi_state.aps[wifi_state.selected_ap];

    /* If network is encrypted, show password dialog */
    if (selected_ap->encrypt != WIFI_ENCRYPT_OPEN) {
        p8_dialog_control_t password_controls[] = {
            DIALOG_LABEL(selected_ap->ssid),
            DIALOG_SPACING(),
            {
                .type = DIALOG_INPUTBOX,
                .label = NULL,
                .data = {
                    .inputbox = {
                        .buffer = wifi_state.password,
                        .buffer_size = sizeof(wifi_state.password),
                        .cursor_pos = 0
                    }
                },
                .selectable = true,
                .enabled = true,
                .inverted = false
            },
            DIALOG_SPACING(),
            DIALOG_BUTTONBAR()
        };

        p8_dialog_t password_dialog;
        p8_dialog_init(&password_dialog, "enter password", password_controls,
                      sizeof(password_controls) / sizeof(password_controls[0]), 0);

        p8_dialog_action_t password_result = p8_dialog_run(&password_dialog);
        bool password_entered = (password_result.type == DIALOG_RESULT_ACCEPTED);

        p8_dialog_cleanup(&password_dialog);

        if (!password_entered) {
            return false;
        }
    }

    /* Attempt to connect */
    const char *password = (selected_ap->encrypt == WIFI_ENCRYPT_OPEN) ? NULL : wifi_state.password;
    bool connected = wifi_wait_for_connected_internal(selected_ap->ssid, password);

    /* Show result dialog */
    const char *result_msg;
    char error_msg[64];
    if (connected) {
        result_msg = "connected successfully!";
    } else {
        snprintf(error_msg, sizeof(error_msg), "connection failed (error %d)", errno);
        result_msg = error_msg;
    }

    p8_dialog_control_t result_controls[] = {
        DIALOG_LABEL(result_msg),
        DIALOG_SPACING(),
        DIALOG_BUTTONBAR_OK_ONLY()
    };

    p8_dialog_t result_dialog;
    p8_dialog_init(&result_dialog, "wi-fi connection", result_controls,
                  sizeof(result_controls) / sizeof(result_controls[0]), 0);

    p8_dialog_run(&result_dialog);

    p8_dialog_cleanup(&result_dialog);

    return connected;
}

/* Wait for Wi-Fi connection and IP address */
bool wifi_wait_for_connected(void)
{
    return wifi_wait_for_connected_internal(NULL, NULL);
}

#endif /* NEXTP8 */
