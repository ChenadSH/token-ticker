#include "http_client.h"

#include <stdio.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "http_client";

static esp_http_client_method_t http_method_to_esp(http_method_t method)
{
    switch (method)
    {
    case TOKEN_HTTP_METHOD_POST:
        return HTTP_METHOD_POST;
    case TOKEN_HTTP_METHOD_GET:
    default:
        return HTTP_METHOD_GET;
    }
}

void http_request_init(http_request_t *request)
{
    if (request == NULL)
    {
        return;
    }

    memset(request, 0, sizeof(*request));
    request->method = TOKEN_HTTP_METHOD_GET;
    request->timeout_ms = 10000;
    request->skip_tls_verify = false;
}

bool http_request_set_url(http_request_t *request, const char *url)
{
    if (request == NULL || url == NULL)
    {
        return false;
    }

    if (strlen(url) >= sizeof(request->url))
    {
        return false;
    }

    strcpy(request->url, url);
    return true;
}

bool http_request_set_bearer_token(http_request_t *request, const char *token)
{
    if (request == NULL || token == NULL)
    {
        return false;
    }

    if (strlen(token) >= sizeof(request->bearer_token))
    {
        return false;
    }

    strcpy(request->bearer_token, token);
    return true;
}

bool http_request_set_body(http_request_t *request, const char *body, size_t body_len)
{
    if (request == NULL || body == NULL)
    {
        return false;
    }

    request->body = body;
    request->body_len = body_len;
    return true;
}

bool http_request_set_content_type(http_request_t *request, const char *content_type)
{
    if (request == NULL || content_type == NULL)
    {
        return false;
    }

    request->content_type = content_type;
    return true;
}

bool http_request_set_skip_tls_verify(http_request_t *request, bool skip)
{
    if (request == NULL)
    {
        return false;
    }

    request->skip_tls_verify = skip;
    return true;
}

static bool http_client_open_and_send(const http_request_t *request, esp_http_client_handle_t client)
{
    esp_err_t error;

    if (request->method == TOKEN_HTTP_METHOD_POST && request->body != NULL && request->body_len > 0)
    {
        error = esp_http_client_open(client, (int)request->body_len);
        if (error != ESP_OK)
        {
            return false;
        }
        int written = esp_http_client_write(client, request->body, (int)request->body_len);
        if (written < 0 || (size_t)written != request->body_len)
        {
            return false;
        }
    }
    else
    {
        error = esp_http_client_open(client, 0);
        if (error != ESP_OK)
        {
            return false;
        }
    }
    return true;
}

static void http_client_apply_headers(const http_request_t *request, esp_http_client_handle_t client)
{
    char auth_header[HTTP_HEADER_VALUE_MAX_LEN + 8];

    if (request->bearer_token[0] != '\0')
    {
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", request->bearer_token);
        esp_http_client_set_header(client, "Authorization", auth_header);
    }

    if (request->content_type != NULL && request->content_type[0] != '\0')
    {
        esp_http_client_set_header(client, "Content-Type", request->content_type);
    }
    else
    {
        esp_http_client_set_header(client, "Accept", "application/json");
    }
}

bool http_client_request(const http_request_t *request,
                         char *response_buffer,
                         size_t response_buffer_len,
                         http_response_meta_t *meta)
{
    esp_http_client_config_t config;
    esp_http_client_handle_t client;
    int read_len;
    size_t total_read = 0;

    if (meta != NULL)
    {
        meta->status = HTTP_CLIENT_STATUS_NOT_IMPLEMENTED;
        meta->http_status_code = 0;
        meta->bytes_received = 0;
    }

    if (request == NULL || response_buffer == NULL || response_buffer_len == 0)
    {
        return false;
    }

    response_buffer[0] = '\0';

    memset(&config, 0, sizeof(config));
    config.url = request->url;
    config.method = http_method_to_esp(request->method);
    config.timeout_ms = (int)request->timeout_ms;

    if (request->skip_tls_verify)
    {
        config.skip_cert_common_name_check = true;
        config.crt_bundle_attach = NULL;
        ESP_LOGW(TAG, "TLS verification disabled for url=%s", request->url);
    }
    else
    {
        config.crt_bundle_attach = esp_crt_bundle_attach;
    }

    client = esp_http_client_init(&config);
    if (client == NULL)
    {
        if (meta != NULL)
        {
            meta->status = HTTP_CLIENT_STATUS_TRANSPORT_ERROR;
        }
        return false;
    }

    http_client_apply_headers(request, client);

    if (!http_client_open_and_send(request, client))
    {
        if (meta != NULL)
        {
            meta->status = HTTP_CLIENT_STATUS_TRANSPORT_ERROR;
        }
        esp_http_client_cleanup(client);
        return false;
    }

    (void)esp_http_client_fetch_headers(client);

    if (meta != NULL)
    {
        meta->http_status_code = (uint16_t)esp_http_client_get_status_code(client);
    }

    while ((read_len = esp_http_client_read(client,
                                            response_buffer + total_read,
                                            (int)(response_buffer_len - 1 - total_read))) > 0)
    {
        total_read += (size_t)read_len;
        if (total_read > response_buffer_len - 1)
        {
            response_buffer[response_buffer_len - 1] = '\0';
            if (meta != NULL)
            {
                meta->status = HTTP_CLIENT_STATUS_BUFFER_TOO_SMALL;
                meta->bytes_received = total_read;
            }
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return false;
        }
    }

    response_buffer[total_read] = '\0';

    if (meta != NULL)
    {
        meta->bytes_received = total_read;
    }

    if (read_len < 0)
    {
        if (meta != NULL)
        {
            meta->status = HTTP_CLIENT_STATUS_TRANSPORT_ERROR;
        }
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (meta != NULL)
    {
        meta->status = (meta->http_status_code >= 200 && meta->http_status_code < 300)
                           ? HTTP_CLIENT_STATUS_OK
                           : HTTP_CLIENT_STATUS_HTTP_ERROR;
    }

    return meta == NULL || meta->status == HTTP_CLIENT_STATUS_OK;
}

bool http_client_get_json(const http_request_t *request,
                          char *response_buffer,
                          size_t response_buffer_len,
                          http_response_meta_t *meta)
{
    return http_client_request(request, response_buffer, response_buffer_len, meta);
}

bool http_client_get_binary(const http_request_t *request,
                            uint8_t *response_buffer,
                            size_t response_buffer_len,
                            http_response_meta_t *meta)
{
    return http_client_request(request, (char *)response_buffer, response_buffer_len, meta);
}
