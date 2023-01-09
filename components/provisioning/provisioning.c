#include <esp_log.h>
#include <esp_http_server.h>

#include "provisioning.h"
#include "provisioning_int.h"

#include "wifi.h"
#include "iot.h"

#include "uzlib.h"

#ifdef CONFIG_IDF_TARGET_ESP8266
#define HTTPD_TASK_STACK_SIZE 2048
#elif CONFIG_IDF_TARGET_ESP32
#define HTTPD_TASK_STACK_SIZE 4096
#endif


#define DICT_SIZE 1024
#define DECOMPRESS_BUFFER_SIZE 1024

static const char *TAG="PROVISION";
static const char *ACCEPT_ENCODING = "accept-encoding";
static const char *CONTENT_ENCODING = "content-encoding";

static const char *content_types[CT_MAX] = {
    NULL,
    "text/css",
    "text/javascript",
    "application/json",
};

static const httpd_uri_t handlers[] = {{
        .method = HTTP_GET,
        .uri = "/config",
        .handler = provisioningConfigGetHandler,
        .user_ctx = NULL
    }, {
        .method = HTTP_POST,
        .uri = "/config",
        .handler = provisioningConfigPostHandler,
        .user_ctx = NULL
    }, {
        .method = HTTP_GET,
        .uri = "/components.json",
        .handler = provisioningComponentsJsonFileHandler,
        .user_ctx = NULL
    }, {
        .method = HTTP_GET,
        .uri = "/wifiscan",
        .handler = provisioningWifiScanGetHandler,
        .user_ctx = NULL
    }
};

static bool isCompressionAcceptable(httpd_req_t *req);
static esp_err_t sendDecompressed(httpd_req_t *req, const struct static_file_data *resp);

int provisioningInit(void)
{
    uzlib_init();
    return 0;
}

int provisioningStart(void)
{
    int i;
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = HTTPD_TASK_STACK_SIZE;

    ESP_LOGI(TAG, "Starting server on port: %d", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK) {
        return 1;
    }
    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    provisioningRegisterStaticFileHandlers(server);

    for (i = 0; i < sizeof(handlers)/sizeof(httpd_uri_t); i++) {
        httpd_register_uri_handler(server, &handlers[i]);
    }

    return 0;
}

void provisioningSetContentType(httpd_req_t *req, enum ContentType content_type)
{
    const char *content_type_str = content_types[content_type];
    if (content_type_str != NULL) {
        httpd_resp_set_type(req, content_type_str);
    }
}

esp_err_t provisioningStaticFileHandler(httpd_req_t *req)
{
    const struct static_file_data* resp = (const struct static_file_data*) req->user_ctx;
    provisioningSetContentType(req, resp->content_type);
    if (resp->size & SIZE_FLAG_COMPRESSED) {
        // Check if compression via gzip is acceptable?
        if (!isCompressionAcceptable(req)) {
            return sendDecompressed(req, resp);
        }
        httpd_resp_set_hdr(req, CONTENT_ENCODING, "gzip");
    }
    httpd_resp_send(req, resp->data, SF_GET_SIZE(resp->size));
    return ESP_OK;
}

static bool isCompressionAcceptable(httpd_req_t *req)
{
    size_t len = httpd_req_get_hdr_value_len(req, ACCEPT_ENCODING);
    if (len == 0) {
        return false;
    }
    char *accept_enc = malloc(len);
    if (accept_enc == NULL) {
        return false;
    }
    accept_enc[0] = 0;
    httpd_req_get_hdr_value_str(req, ACCEPT_ENCODING, accept_enc, len);
    bool result = (strstr(accept_enc, "gzip") != NULL);
    free(accept_enc);
    return result;
}

static esp_err_t sendDecompressed(httpd_req_t *req, const struct static_file_data *resp)
{
    int res;
    struct uzlib_uncomp d;
    unsigned char *buffer;
    unsigned char *dict;

    dict = malloc(DICT_SIZE);
    if (dict == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    uzlib_uncompress_init(&d, dict, DICT_SIZE);
    d.source = (unsigned char*) resp->data;
    d.source_limit = (unsigned char*)resp->data + SF_GET_SIZE(resp->size) - 4;
    res = uzlib_gzip_parse_header(&d);
    if (res != TINF_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    buffer = malloc(DECOMPRESS_BUFFER_SIZE);
    if (buffer == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Decompression buffer allocated.");
    d.dest = d.dest_start = buffer;
    d.dest_limit = buffer + DECOMPRESS_BUFFER_SIZE;
    int r = uzlib_uncompress(&d);
    ESP_LOGI(TAG, "uncompress result %d", r);
    while (r == TINF_OK) {
        ESP_LOGI(TAG, "Decompressed chunk, dest %p (limit %p) source %p (limit %p)", d.dest, d.dest_limit, d.source, d.source_limit);
        httpd_resp_send_chunk(req, (char*)buffer, d.dest - buffer);
        ESP_LOGI(TAG, "Sent decompressed chunk");
        d.dest = buffer;
        r = uzlib_uncompress(&d);
        ESP_LOGI(TAG, "uncompress result %d", r);
    }
    if (d.dest != buffer) {
        httpd_resp_send_chunk(req, (char*)buffer, d.dest - buffer);
    }
    httpd_resp_send_chunk(req, NULL, 0);
    free(buffer);
    free(dict);
    return ESP_OK;
}