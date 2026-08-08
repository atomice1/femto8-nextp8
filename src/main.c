/*
 * main.c
 *
 *  Created on: Dec 13, 2023
 *      Author: bbaker
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "p8_main.h"
#include "p8_parser.h"
#include "p8_emu.h"
#include "p8_lua.h"
#include "strtcpy.h"
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

const uint32_t nextp8_version   = _MAKE_VERSION(API_VERSION, MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
const uint32_t nextp8_timestamp = _TIMESTAMP;
#endif

#define VERSION "1.0.00"

const char *femto8_version = VERSION;

int main(int argc, char *argv[])
{
#ifdef NEXTP8
    _set_postcode(POST_CODE_ENTER_MAIN);
#endif

    setvbuf(stdout, NULL, _IONBF, 0); // Disable buffering for stdout

    const char *file_name = NULL;
    const char *param_string = NULL;
    bool skip_main_loop = false;
    int exit_code = EXIT_SUCCESS;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("v%s\n", VERSION);
            return 0;
        } else if (strcmp(argv[i], "--skip-compat-check") == 0) {
            // Ignore for compatibility
        } else if (strcmp(argv[i], "-x") == 0) {
            skip_main_loop = true;
        } else if (strcmp(argv[i], "--no-frame-delay") == 0) {
            m_no_frame_delay = true;
        } else if (strcmp(argv[i], "--no-frame-skip") == 0) {
            m_no_frame_skip = true;
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            param_string = argv[++i];
        } else if (file_name == NULL) {
            file_name = argv[i];
        }
    }

#ifdef NEXTP8
    uint32_t hw_version = *(uint32_t *)_HW_VERSION_HI;
    if (_EXTRACT_API(hw_version) != HW_API_VERSION)
        _fatal_error("Incompatible hardware version");
#endif

    if (p8_init() != 0)
        return EXIT_FAILURE;

    if (file_name == NULL) {
        do {
            p8_main();
        } while (p8_is_reboot_requested());
    } else {
        if (skip_main_loop)
            p8_set_skip_main_loop_if_no_callbacks(true);
        if (p8_load(file_name, param_string, NULL, NULL) != 0) {
            exit_code = EXIT_FAILURE;
        } else {
            if (p8_run() != 0) {
                lua_print_error();
                exit_code = EXIT_FAILURE;
            }
        }
    }
    p8_shutdown();

    return exit_code;
}
