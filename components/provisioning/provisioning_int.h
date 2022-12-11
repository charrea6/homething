#ifndef _PROVISIONING_INT_H_
#define _PROVISIONING_INT_H_
#include <stdint.h>
#include <esp_http_server.h>

enum ContentType {
    CT_HTML = 0,
    CT_CSS,
    CT_JS,
    CT_JSON,
    CT_MAX
};

#define SIZE_FLAG_COMPRESSED 0x80000000
#define SF_GET_SIZE(size) (size & (SIZE_FLAG_COMPRESSED - 1))
struct static_file_data {
    uint32_t size;
    const int content_type;
    const char data[];
};

esp_err_t provisioningStaticFileHandler(httpd_req_t *req);
esp_err_t provisioningConfigPostHandler(httpd_req_t *req);
esp_err_t provisioningConfigGetHandler(httpd_req_t *req);
esp_err_t provisioningComponentsJsonFileHandler(httpd_req_t *req);

void provisioningRegisterStaticFileHandlers(httpd_handle_t server);
void provisioningSetContentType(httpd_req_t *req, enum ContentType content_type);


#endif