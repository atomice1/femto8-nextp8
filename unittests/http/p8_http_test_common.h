/**
 * p8_http_test_common.h - Common test utilities and fixtures for HTTP client tests
 */

#ifndef P8_HTTP_TEST_COMMON_H
#define P8_HTTP_TEST_COMMON_H

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

/* Include the HTTP header */
#include "p8_http.h"

/* Ensure errno is available for assertions */
#ifndef assert_errno
#define assert_errno(expected) assert_int_equal(errno, (expected))
#endif

#endif /* P8_HTTP_TEST_COMMON_H */
