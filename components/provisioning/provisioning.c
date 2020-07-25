#include <esp_log.h>
#include <esp_http_server.h>

#include "provisioning.h"
#include "provisioning_int.h"
#include "captdns.h"

static const char *TAG="PROVISION";

static const char *content_types[] = {
    NULL,
    "text/css",
    "text/javascript",
    "font/woff2",
};

/* An HTTP GET handler */
esp_err_t provisioningStaticFileHandler(httpd_req_t *req)
{
    const struct static_file_data* resp = (const struct static_file_data*) req->user_ctx;
    const char *content_type = content_types[resp->content_type];
    if (content_type != NULL) {
        httpd_resp_set_type(req, content_type);
    }
    httpd_resp_send(req, resp->data, resp->size);

    return ESP_OK;
}

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
    
    captdnsInit();
    return 0;
}
