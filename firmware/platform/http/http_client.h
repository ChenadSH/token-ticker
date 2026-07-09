#ifndef TOKEN_TICKER_HTTP_CLIENT_H
#define TOKEN_TICKER_HTTP_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HTTP_URL_MAX_LEN 192
#define HTTP_HEADER_VALUE_MAX_LEN 160

typedef enum
{
    TOKEN_HTTP_METHOD_GET = 0,
    TOKEN_HTTP_METHOD_POST,
} http_method_t;

typedef enum
{
    HTTP_CLIENT_STATUS_IDLE = 0,
    HTTP_CLIENT_STATUS_NOT_IMPLEMENTED,
    HTTP_CLIENT_STATUS_TRANSPORT_ERROR,
    HTTP_CLIENT_STATUS_HTTP_ERROR,
    HTTP_CLIENT_STATUS_BUFFER_TOO_SMALL,
    HTTP_CLIENT_STATUS_OK,
} http_client_status_t;

typedef struct
{
    http_method_t method;
    char url[HTTP_URL_MAX_LEN];
    char bearer_token[HTTP_HEADER_VALUE_MAX_LEN];
    const char *body;
    size_t body_len;
    const char *content_type;
    bool skip_tls_verify;
    uint32_t timeout_ms;
} http_request_t;

typedef struct
{
    http_client_status_t status;
    uint16_t http_status_code;
    size_t bytes_received;
} http_response_meta_t;

void http_request_init(http_request_t *request);
bool http_request_set_url(http_request_t *request, const char *url);
bool http_request_set_bearer_token(http_request_t *request, const char *token);
bool http_request_set_body(http_request_t *request, const char *body, size_t body_len);
bool http_request_set_content_type(http_request_t *request, const char *content_type);
bool http_request_set_skip_tls_verify(http_request_t *request, bool skip);
bool http_client_request(const http_request_t *request,
                         char *response_buffer,
                         size_t response_buffer_len,
                         http_response_meta_t *meta);

bool http_client_get_json(const http_request_t *request,
                          char *response_buffer,
                          size_t response_buffer_len,
                          http_response_meta_t *meta);
bool http_client_get_binary(const http_request_t *request,
                            uint8_t *response_buffer,
                            size_t response_buffer_len,
                            http_response_meta_t *meta);

#endif
