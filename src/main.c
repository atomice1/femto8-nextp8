/*
 * main.c
 *
 *  Created on: Dec 13, 2023
 *      Author: bbaker
 */

#include <stdio.h>
#include <string.h>
#include "p8_browse.h"
#include "p8_parser.h"
#include "p8_emu.h"
#ifdef NEXTP8
#include "nextp8.h"
#include "postcodes.h"
#include "timestamp_macros.h"
#include "version_macros.h"

#define HW_API_VERSION 0
#define API_VERSION    0
#define MAJOR_VERSION  0
#define MINOR_VERSION  1
#define PATCH_VERSION  0
#define DEV_BUILD      1

static const uint32_t femto8_version   = _MAKE_VERSION(API_VERSION, MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
static const uint32_t femto8_timestamp = _TIMESTAMP;
#endif

int main(int argc, char *argv[])
{
#ifdef NEXTP8
    _set_postcode(POST_CODE_ENTER_MAIN);
    uint32_t hw_timestamp = *(uint32_t *)_BUILD_TIMESTAMP_HI;
    uint32_t hw_version = *(uint32_t *)_HW_VERSION_HI;
    if (_EXTRACT_API(hw_version) != HW_API_VERSION)
        _fatal_error("Incompatible hardware version");
#if DEV_BUILD
    uint32_t loader_version = _loader_data->loader_version;
    uint32_t loader_timestamp = _loader_data->loader_timestamp;
    _set_postcode(32);
    _show_message(
        "This is a development build of\n"
        "nextp8. About 60%%-70%% of carts\n"
        "may work. Audio may be glitchy.\n"
        "\n"
        "nextp8 comes with NO WARRANTY.\n"
        "\n"
        "nextp8 %u.%u.%u %u%02u%02u %02u:%02u:%02u\n"
        "HW %u.%u.%u %u%02u%02u %02u:%02u:%02u\n"
        "BSP %u.%u.%u %u%02u%02u %02u:%02u:%02u\n"
        "Loader %u.%u.%u %u%02u%02u %02u:%02u:%02u\n",
        _EXTRACT_MAJOR(femto8_version),
        _EXTRACT_MINOR(femto8_version),
        _EXTRACT_PATCH(femto8_version),
        _EXTRACT_YEAR(femto8_timestamp),
        _EXTRACT_MONTH(femto8_timestamp),
        _EXTRACT_DAY(femto8_timestamp),
        _EXTRACT_HOUR(femto8_timestamp),
        _EXTRACT_MINUTE(femto8_timestamp),
        _EXTRACT_SECOND(femto8_timestamp),
        _EXTRACT_MAJOR(hw_version),
        _EXTRACT_MINOR(hw_version),
        _EXTRACT_PATCH(hw_version),
        _EXTRACT_YEAR(hw_timestamp),
        _EXTRACT_MONTH(hw_timestamp),
        _EXTRACT_DAY(hw_timestamp),
        _EXTRACT_HOUR(hw_timestamp),
        _EXTRACT_MINUTE(hw_timestamp),
        _EXTRACT_SECOND(hw_timestamp),
        _EXTRACT_MAJOR(_bsp_version),
        _EXTRACT_MINOR(_bsp_version),
        _EXTRACT_PATCH(_bsp_version),
        _EXTRACT_YEAR(_bsp_timestamp),
        _EXTRACT_MONTH(_bsp_timestamp),
        _EXTRACT_DAY(_bsp_timestamp),
        _EXTRACT_HOUR(_bsp_timestamp),
        _EXTRACT_MINUTE(_bsp_timestamp),
        _EXTRACT_SECOND(_bsp_timestamp),
        _EXTRACT_MAJOR(loader_version),
        _EXTRACT_MINOR(loader_version),
        _EXTRACT_PATCH(loader_version),
        _EXTRACT_YEAR(loader_timestamp),
        _EXTRACT_MONTH(loader_timestamp),
        _EXTRACT_DAY(loader_timestamp),
        _EXTRACT_HOUR(loader_timestamp),
        _EXTRACT_MINUTE(loader_timestamp),
        _EXTRACT_SECOND(loader_timestamp));
#endif
#endif

    const char *file_name = NULL;
    const char *param_string = NULL;
    bool skip_compat = false;
    bool skip_main_loop = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--skip-compat-check") == 0) {
            skip_compat = true;
        } else if (strcmp(argv[i], "-x") == 0) {
            skip_main_loop = true;
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            param_string = argv[++i];
        } else if (file_name == NULL) {
            file_name = argv[i];
        }
    }

    if (file_name == NULL)
        file_name = browse_for_cart();

    if (skip_compat)
        p8_set_skip_compat_check(true);
    if (skip_main_loop)
        p8_set_skip_main_loop_if_no_callbacks(true);
    if (file_name != NULL)
        p8_init_file_with_param(file_name, param_string);
    p8_shutdown();

    return 0;
}
