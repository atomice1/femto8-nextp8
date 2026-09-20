/**
 * test_http.c - Unit tests for parse_url function in p8_http.c
 */

#include "p8_http_test_common.h"

extern int parse_url(const char *url, bool *use_ssl, char *host, size_t host_len,
                     unsigned *port, char *path, size_t path_len);

/* Test parse_url with http:// URLs */
void test_parse_url_http_basic(void **state)
{
    (void)state;

    bool use_ssl;
    char host[256];
    unsigned port;
    char path[1024];
    int result;

    result = parse_url("http://example.com", &use_ssl, host, sizeof(host), &port, path, sizeof(path));

    assert_int_equal(result, 0);
    assert_false(use_ssl);
    assert_string_equal(host, "example.com");
    assert_int_equal(port, 80);
    assert_string_equal(path, "/");
}

void test_parse_url_http_with_port(void **state)
{
    (void)state;

    bool use_ssl;
    char host[256];
    unsigned port;
    char path[1024];
    int result;

    result = parse_url("http://example.com:8080", &use_ssl, host, sizeof(host), &port, path, sizeof(path));

    assert_int_equal(result, 0);
    assert_false(use_ssl);
    assert_string_equal(host, "example.com");
    assert_int_equal(port, 8080);
    assert_string_equal(path, "/");
}

void test_parse_url_http_with_path(void **state)
{
    (void)state;

    bool use_ssl;
    char host[256];
    unsigned port;
    char path[1024];
    int result;

    result = parse_url("http://example.com/path/to/resource", &use_ssl, host, sizeof(host), &port, path, sizeof(path));

    assert_int_equal(result, 0);
    assert_false(use_ssl);
    assert_string_equal(host, "example.com");
    assert_int_equal(port, 80);
    assert_string_equal(path, "/path/to/resource");
}

void test_parse_url_http_with_path_and_port(void **state)
{
    (void)state;

    bool use_ssl;
    char host[256];
    unsigned port;
    char path[1024];
    int result;

    result = parse_url("http://example.com:3000/api/v1", &use_ssl, host, sizeof(host), &port, path, sizeof(path));

    assert_int_equal(result, 0);
    assert_false(use_ssl);
    assert_string_equal(host, "example.com");
    assert_int_equal(port, 3000);
    assert_string_equal(path, "/api/v1");
}

/* Test parse_url with https:// URLs */
void test_parse_url_https_basic(void **state)
{
    (void)state;

    bool use_ssl;
    char host[256];
    unsigned port;
    char path[1024];
    int result;

    result = parse_url("https://example.com", &use_ssl, host, sizeof(host), &port, path, sizeof(path));

    assert_int_equal(result, 0);
    assert_true(use_ssl);
    assert_string_equal(host, "example.com");
    assert_int_equal(port, 443);
    assert_string_equal(path, "/");
}

void test_parse_url_https_with_port(void **state)
{
    (void)state;

    bool use_ssl;
    char host[256];
    unsigned port;
    char path[1024];
    int result;

    result = parse_url("https://example.com:8443", &use_ssl, host, sizeof(host), &port, path, sizeof(path));

    assert_int_equal(result, 0);
    assert_true(use_ssl);
    assert_string_equal(host, "example.com");
    assert_int_equal(port, 8443);
    assert_string_equal(path, "/");
}

void test_parse_url_https_with_path(void **state)
{
    (void)state;

    bool use_ssl;
    char host[256];
    unsigned port;
    char path[1024];
    int result;

    result = parse_url("https://example.com:9443/secure/path", &use_ssl, host, sizeof(host), &port, path, sizeof(path));

    assert_int_equal(result, 0);
    assert_true(use_ssl);
    assert_string_equal(host, "example.com");
    assert_int_equal(port, 9443);
    assert_string_equal(path, "/secure/path");
}

/* Test error cases */
void test_parse_url_unsupported_protocol(void **state)
{
    (void)state;

    bool use_ssl;
    char host[256];
    unsigned port;
    char path[1024];
    int result;

    result = parse_url("ftp://example.com", &use_ssl, host, sizeof(host), &port, path, sizeof(path));

    assert_int_equal(result, -1);
    assert_errno(EINVAL);
}

void test_parse_url_no_protocol(void **state)
{
    (void)state;

    bool use_ssl;
    char host[256];
    unsigned port;
    char path[1024];
    int result;

    result = parse_url("example.com", &use_ssl, host, sizeof(host), &port, path, sizeof(path));

    assert_int_equal(result, -1);
    assert_errno(EINVAL);
}

void test_parse_url_invalid_port(void **state)
{
    (void)state;

    bool use_ssl;
    char host[256];
    unsigned port;
    char path[1024];
    int result;

    result = parse_url("http://example.com:0", &use_ssl, host, sizeof(host), &port, path, sizeof(path));

    assert_int_equal(result, -1);
    assert_errno(EINVAL);
}

void test_parse_url_invalid_port_too_high(void **state)
{
    (void)state;

    bool use_ssl;
    char host[256];
    unsigned port;
    char path[1024];
    int result;

    result = parse_url("http://example.com:70000", &use_ssl, host, sizeof(host), &port, path, sizeof(path));

    assert_int_equal(result, -1);
    assert_errno(EINVAL);
}

void test_parse_url_invalid_port_non_numeric(void **state)
{
    (void)state;

    bool use_ssl;
    char host[256];
    unsigned port;
    char path[1024];
    int result;

    result = parse_url("http://example.com:abc", &use_ssl, host, sizeof(host), &port, path, sizeof(path));

    assert_int_equal(result, -1);
    assert_errno(EINVAL);
}

void test_parse_url_empty_host(void **state)
{
    (void)state;

    bool use_ssl;
    char host[256];
    unsigned port;
    char path[1024];
    int result;

    result = parse_url("http://", &use_ssl, host, sizeof(host), &port, path, sizeof(path));

    assert_int_equal(result, -1);
    assert_errno(EINVAL);
}

void test_parse_url_host_too_long(void **state)
{
    (void)state;

    bool use_ssl;
    char host[16];  /* Small buffer */
    unsigned port;
    char path[1024];
    int result;

    result = parse_url("http://verylonghostnameexample.com", &use_ssl, host, sizeof(host), &port, path, sizeof(path));

    assert_int_equal(result, -1);
    assert_errno(EINVAL);
}

void test_parse_url_path_too_long(void **state)
{
    (void)state;

    bool use_ssl;
    char host[256];
    unsigned port;
    char path[10];  /* Small buffer */
    int result;

    result = parse_url("http://example.com/verylongpaththatexceedsbuffer", &use_ssl, host, sizeof(host), &port, path, sizeof(path));

    assert_int_equal(result, -1);
    assert_errno(EINVAL);
}

/* Test edge cases */
void test_parse_url_localhost(void **state)
{
    (void)state;

    bool use_ssl;
    char host[256];
    unsigned port;
    char path[1024];
    int result;

    result = parse_url("http://localhost:8000", &use_ssl, host, sizeof(host), &port, path, sizeof(path));

    assert_int_equal(result, 0);
    assert_false(use_ssl);
    assert_string_equal(host, "localhost");
    assert_int_equal(port, 8000);
    assert_string_equal(path, "/");
}

void test_parse_url_ip_address(void **state)
{
    (void)state;

    bool use_ssl;
    char host[256];
    unsigned port;
    char path[1024];
    int result;

    result = parse_url("http://192.168.1.1:3000/data", &use_ssl, host, sizeof(host), &port, path, sizeof(path));

    assert_int_equal(result, 0);
    assert_false(use_ssl);
    assert_string_equal(host, "192.168.1.1");
    assert_int_equal(port, 3000);
    assert_string_equal(path, "/data");
}

void test_parse_url_path_only_root(void **state)
{
    (void)state;

    bool use_ssl;
    char host[256];
    unsigned port;
    char path[1024];
    int result;

    result = parse_url("http://example.com/", &use_ssl, host, sizeof(host), &port, path, sizeof(path));

    assert_int_equal(result, 0);
    assert_false(use_ssl);
    assert_string_equal(host, "example.com");
    assert_int_equal(port, 80);
    assert_string_equal(path, "/");
}

void test_parse_url_no_path_defaults_to_root(void **state)
{
    (void)state;

    bool use_ssl;
    char host[256];
    unsigned port;
    char path[1024];
    int result;

    result = parse_url("http://example.com:8080", &use_ssl, host, sizeof(host), &port, path, sizeof(path));

    assert_int_equal(result, 0);
    assert_false(use_ssl);
    assert_string_equal(host, "example.com");
    assert_int_equal(port, 8080);
    assert_string_equal(path, "/");
}

/* Test that errno is set correctly */
void test_parse_url_errno_set_on_invalid_url(void **state)
{
    (void)state;

    bool use_ssl;
    char host[256];
    unsigned port;
    char path[1024];

    errno = 0;
    parse_url("invalid", &use_ssl, host, sizeof(host), &port, path, sizeof(path));

    assert_int_not_equal(errno, 0);
}
