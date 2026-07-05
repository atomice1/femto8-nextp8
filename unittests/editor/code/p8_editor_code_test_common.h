/**
 * p8_editor_code_test_common.h - Common test utilities and fixtures for PICO-8 editor tests
 */

#ifndef P8_EDITOR_CODE_TEST_COMMON_H
#define P8_EDITOR_CODE_TEST_COMMON_H

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Include the main editor header */
#include "p8_editor_code.h"

/* Include the private header with extern declarations */
#include "p8_editor_code_priv.h"

/* Include p8_input.h for KMOD definitions */
#include "p8_input.h"

/* Helper functions */
#include "p8_editor_code_test_helpers.h"

#endif /* P8_EDITOR_CODE_TEST_COMMON_H */
