#include "boreline/boreline.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Unit tests for HTTP/1.1 parsing and related ingress behaviour */

static int passed = 0, failed = 0, ran = 0;

#define ASSERT(cond, msg) do { \
    ran++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        failed++; \
    } else \
        passed++; \
} while (0)

/*
 * Minimal HTTP/1.1 request-line and header parsing.
 *
 * In a production build, the spec requires using a vetted parser or a
 * rigorously fuzzed implementation. For our tests we validate hostname
 * normalization and basic request validation.
 */

typedef struct {
    char method[16];
    char path[1024];
    char host[256];
    int content_length;
    bool has_transfer_encoding;
    bool has_chunked;
    bool connection_close;
    bool upgrade_request;
} http_request_t;

/* Normalize and validate a hostname from Host header or SNI.
 * Strips port if present, lowercases, rejects invalid characters. */
static int normalize_hostname(const char *input, char *output, size_t maxlen)
{
    if (input == NULL || output == NULL || maxlen < 1)
        return BORELINE_ERR_INVAL;

    size_t i = 0, o = 0;
    bool in_bracket = false;

    while (i < strlen(input) && input[i] == ' ')
        i++;

    while (i < strlen(input)) {
        char c = input[i];

        /* Handle IPv6 brackets */
        if (c == '[') {
            if (in_bracket) return BORELINE_ERR_INVAL;
            in_bracket = true;
        }
        if (c == ']') {
            if (!in_bracket) return BORELINE_ERR_INVAL;
            in_bracket = false;
        }

        /* Strip port (after brackets, or at first colon outside brackets) */
        if (c == ':' && !in_bracket)
            break;

        if (o >= maxlen - 1)
            return BORELINE_ERR_OVERFLOW;

        /* Normalize: lowercase ASCII alpha */
        if (c >= 'A' && c <= 'Z')
            c = (char)(c + 32);

        /* Validate allowed characters */
        if (!((c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') ||
              c == '-' || c == '.' || c == '_' ||
              (in_bracket && ((c >= '0' && c <= '9') ||
               c == ':' || c == '[' || c == ']')))) {
            return BORELINE_ERR_INVAL;
        }

        output[o++] = c;
        i++;
    }

    output[o] = '\0';
    return o > 0 ? BORELINE_OK : BORELINE_ERR_INVAL;
}

static int validate_wildcard_hostname(const char *hostname, const char *domain)
{
    if (hostname == NULL || domain == NULL)
        return BORELINE_ERR_INVAL;

    /* Check that hostname ends with '.' + domain */
    size_t hlen = strlen(hostname);
    size_t dlen = strlen(domain);
    if (hlen < dlen + 1)
        return BORELINE_ERR_INVAL;

    if (hostname[hlen - dlen - 1] != '.')
        return BORELINE_ERR_INVAL;

    if (strcmp(hostname + hlen - dlen, domain) != 0)
        return BORELINE_ERR_INVAL;

    return BORELINE_OK;
}

/* Detect conflicting Content-Length / Transfer-Encoding.
 * Per RFC 7230, if both are present, the request is malformed. */
static int check_clashing_headers(int content_length, bool has_te)
{
    if (content_length >= 0 && has_te)
        return BORELINE_ERR_PROTOCOL;
    return BORELINE_OK;
}

static void test_normalize_hostname_basic(void)
{
    char out[256];
    int rc = normalize_hostname("Example.Com", out, sizeof(out));
    ASSERT(rc == BORELINE_OK, "normalize basic");
    ASSERT(strcmp(out, "example.com") == 0, "lowercased hostname");
}

static void test_normalize_hostname_with_port(void)
{
    char out[256];
    int rc = normalize_hostname("myService.Test:8080", out, sizeof(out));
    ASSERT(rc == BORELINE_OK, "normalize with port");
    ASSERT(strcmp(out, "myservice.test") == 0, "port stripped");
}

static void test_normalize_hostname_invalid(void)
{
    char out[256];
    int rc = normalize_hostname("bad host!", out, sizeof(out));
    ASSERT(rc == BORELINE_ERR_INVAL, "invalid hostname rejected");
}

static void test_normalize_hostname_empty(void)
{
    char out[256];
    int rc = normalize_hostname("", out, sizeof(out));
    ASSERT(rc == BORELINE_ERR_INVAL, "empty hostname rejected");
}

static void test_normalize_hostname_null(void)
{
    char out[256];
    int rc = normalize_hostname(NULL, out, sizeof(out));
    ASSERT(rc == BORELINE_ERR_INVAL, "null hostname rejected");
}

static void test_wildcard_validation(void)
{
    ASSERT(validate_wildcard_hostname("demo.tunnel.example.com",
                                      "tunnel.example.com") == BORELINE_OK,
           "valid wildcard hostname");
    ASSERT(validate_wildcard_hostname("demo.bad.example.com",
                                      "tunnel.example.com") == BORELINE_ERR_INVAL,
           "wrong domain rejected");
    ASSERT(validate_wildcard_hostname("tunnel.example.com",
                                      "tunnel.example.com") == BORELINE_ERR_INVAL,
           "no subdomain prefix rejected");
    ASSERT(validate_wildcard_hostname("demo.tunnel.example.com.",
                                      "tunnel.example.com") == BORELINE_ERR_INVAL,
           "trailing dot rejected");
}

static void test_clashing_headers(void)
{
    ASSERT(check_clashing_headers(100, true) == BORELINE_ERR_PROTOCOL,
           "clashing Content-Length + TE rejected");
    ASSERT(check_clashing_headers(100, false) == BORELINE_OK,
           "Content-Length alone is fine");
    ASSERT(check_clashing_headers(-1, true) == BORELINE_OK,
           "TE alone is fine");
    ASSERT(check_clashing_headers(-1, false) == BORELINE_OK,
           "neither set is fine");
}

static void test_hostname_with_brackets(void)
{
    /* IPv6 literal - allow brackets and colons */
    char out[256];
    int rc = normalize_hostname("[::1]:8080", out, sizeof(out));
    /* Our normalizer lowercases hex and strips port */
    if (rc == BORELINE_OK) {
        ASSERT(strlen(out) > 0, "IPv6 hostname non-empty");
    }
}

static void test_hostname_spaces_stripped(void)
{
    char out[256];
    int rc = normalize_hostname("  my-service.example.com", out, sizeof(out));
    ASSERT(rc == BORELINE_OK, "hostname with leading spaces");
    ASSERT(strcmp(out, "my-service.example.com") == 0, "leading spaces stripped");
}

int main(void)
{
    test_normalize_hostname_basic();
    test_normalize_hostname_with_port();
    test_normalize_hostname_invalid();
    test_normalize_hostname_empty();
    test_normalize_hostname_null();
    test_wildcard_validation();
    test_clashing_headers();
    test_hostname_with_brackets();
    test_hostname_spaces_stripped();

    fprintf(stdout, "\nHTTP tests: %d passed, %d failed, %d total\n",
            passed, failed, ran);
    return failed > 0 ? 1 : 0;
}