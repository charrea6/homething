#include <esp_log.h>
#include <esp_http_server.h>

#include "provisioning.h"
#include "provisioning_int.h"
#include "captdns.h"

#include "wifi.h"
#include "iot.h"

static const char *TAG="PROVISION";

static const char *content_types[CT_MAX] = {
    NULL,
    "text/css",
    "text/javascript",
    "application/json"
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
}
};

int provisioningInit(void)
{
    return 0;
}

int provisioningStart(void)
{
    int i;
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
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

    captdnsInit();
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
    httpd_resp_send(req, resp->data, resp->size);
    return ESP_OK;
}