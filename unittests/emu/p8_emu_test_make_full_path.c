/**
 * p8_emu_test_make_full_path.c - Unit tests for p8_make_full_path function
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include "p8_emu.h"

/* Test 1: Basic path concatenation */

void test_make_full_path_basic(void **state)
{
    (void)state;

    char ret[256];
    int result = p8_make_full_path(ret, sizeof(ret), "/home/user", "file.p8");

    assert_int_equal(result, 0);
    assert_string_equal(ret, "/home/user/file.p8");
}

/* Test 2: Empty directory path with file name */

void test_make_full_path_empty_dir(void **state)
{
    (void)state;

    char ret[256];
    int result = p8_make_full_path(ret, sizeof(ret), "", "file.p8");

    assert_int_equal(result, 0);
    assert_string_equal(ret, "file.p8");
}

/* Test 3: Current directory (.) with file name */

void test_make_full_path_current_dir(void **state)
{
    (void)state;

    char ret[256];
    int result = p8_make_full_path(ret, sizeof(ret), ".", "file.p8");

    assert_int_equal(result, 0);
    assert_string_equal(ret, "file.p8");
}

/* Test 4: ".." from current directory (.) */

void test_make_full_path_dot_dot_from_current(void **state)
{
    (void)state;

    char ret[256];
    int result = p8_make_full_path(ret, sizeof(ret), ".", "..");

    assert_int_equal(result, 0);
    assert_string_equal(ret, "..");
}

/* Test 5: ".." from simple directory */

void test_make_full_path_dot_dot_from_simple_dir(void **state)
{
    (void)state;

    char ret[256];
    int result = p8_make_full_path(ret, sizeof(ret), "/home/user", "..");

    assert_int_equal(result, 0);
    assert_string_equal(ret, "/home");
}

/* Test 6: ".." from directory with trailing slash */
/* Note: trailing slashes are not stripped, so /home/user/ + .. = /home/user */

void test_make_full_path_dot_dot_with_trailing_slash(void **state)
{
    (void)state;

    char ret[256];
    int result = p8_make_full_path(ret, sizeof(ret), "/home/user/", "..");

    assert_int_equal(result, 0);
    assert_string_equal(ret, "/home/user");
}

/* Test 7: ".." from root directory (/) */

void test_make_full_path_dot_dot_from_root(void **state)
{
    (void)state;

    char ret[256];
    int result = p8_make_full_path(ret, sizeof(ret), "/", "..");

    assert_int_equal(result, 0);
    assert_string_equal(ret, "/");
}

/* Test 8: ".." from Windows drive root (C:\) */

void test_make_full_path_dot_dot_windows_drive_root(void **state)
{
    (void)state;

    char ret[256];
    int result = p8_make_full_path(ret, sizeof(ret), "C:\\", "..");

    assert_int_equal(result, 0);
    assert_string_equal(ret, "");
}

/* Test 9: ".." from Windows path at drive root (C:\dir) */

void test_make_full_path_dot_dot_windows_at_drive_root(void **state)
{
    (void)state;

    char ret[256];
    int result = p8_make_full_path(ret, sizeof(ret), "C:\\dir", "..");

    assert_int_equal(result, 0);
    assert_string_equal(ret, "C:\\");
}

/* Test 10: ".." from Windows path (C:\dir\subdir) */

void test_make_full_path_dot_dot_windows_subdir(void **state)
{
    (void)state;

    char ret[256];
    int result = p8_make_full_path(ret, sizeof(ret), "C:\\dir\\subdir", "..");

    assert_int_equal(result, 0);
    assert_string_equal(ret, "C:\\dir");
}

/* Test 11: ".." from nested directory */

void test_make_full_path_dot_dot_nested(void **state)
{
    (void)state;

    char ret[256];
    int result = p8_make_full_path(ret, sizeof(ret), "/a/b/c/d", "..");

    assert_int_equal(result, 0);
    assert_string_equal(ret, "/a/b/c");
}

/* Test 12: ".." from directory ending with ".." */
/* Note: the last component is "..", so we append another ".." giving /a/b/../.. */

void test_make_full_path_dot_dot_ending_with_dot_dot(void **state)
{
    (void)state;

    char ret[256];
    int result = p8_make_full_path(ret, sizeof(ret), "/a/b/..", "..");

    assert_int_equal(result, 0);
    assert_string_equal(ret, "/a/b/../..");
}

/* Test 13: ".." from empty directory path */

void test_make_full_path_dot_dot_from_empty(void **state)
{
    (void)state;

    char ret[256];
    int result = p8_make_full_path(ret, sizeof(ret), "", "..");

    assert_int_equal(result, 0);
    assert_string_equal(ret, "");
}

/* Test 14: ".." from directory that is just ".." */

void test_make_full_path_dot_dot_is_dot_dot(void **state)
{
    (void)state;

    char ret[256];
    int result = p8_make_full_path(ret, sizeof(ret), "..", "..");

    assert_int_equal(result, 0);
    assert_string_equal(ret, "../..");
}

/* Test 14b: ".." from directory that is "../.." */
/* Should append ".." to "../.." giving "../../.." */

void test_make_full_path_dot_dot_from_dot_dot(void **state)
{
    (void)state;

    char ret[256];
    int result = p8_make_full_path(ret, sizeof(ret), "../..", "..");

    assert_int_equal(result, 0);
    assert_string_equal(ret, "../../..");
}

/* Test 15: NULL return buffer - should return -1 with EINVAL */

void test_make_full_path_null_ret(void **state)
{
    (void)state;

    int result = p8_make_full_path(NULL, 256, "/home", "file.p8");

    assert_int_equal(result, -1);
    assert_int_equal(errno, EINVAL);
}

/* Test 16: NULL directory path - should return -1 with EINVAL */

void test_make_full_path_null_dir(void **state)
{
    (void)state;

    char ret[256];
    int result = p8_make_full_path(ret, sizeof(ret), NULL, "file.p8");

    assert_int_equal(result, -1);
    assert_int_equal(errno, EINVAL);
}

/* Test 17: NULL file name - should return -1 with EINVAL */

void test_make_full_path_null_file(void **state)
{
    (void)state;

    char ret[256];
    int result = p8_make_full_path(ret, sizeof(ret), "/home", NULL);

    assert_int_equal(result, -1);
    assert_int_equal(errno, EINVAL);
}

/* Test 18: Buffer size too small (< 3) - should return -1 with EINVAL */

void test_make_full_path_buffer_too_small(void **state)
{
    (void)state;

    char ret[256];
    int result = p8_make_full_path(ret, 2, "/home", "file.p8");

    assert_int_equal(result, -1);
    assert_int_equal(errno, EINVAL);
}

/* Test 19: Buffer too small for result */

void test_make_full_path_buffer_too_small_for_result(void **state)
{
    (void)state;

    char ret[10];
    int result = p8_make_full_path(ret, 10, "/very/long/directory/path", "file.p8");

    assert_int_equal(result, -1);
    /* errno should be set to ENAMETOOLONG or similar */
}

/* Test 20: Forward slash path separator */

void test_make_full_path_forward_slash(void **state)
{
    (void)state;

    char ret[256];
    int result = p8_make_full_path(ret, sizeof(ret), "home/user", "file.p8");

    assert_int_equal(result, 0);
    assert_string_equal(ret, "home/user/file.p8");
}
