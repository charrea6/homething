#define variables_wifi_len 2
struct variable variables_wifi[] = {
    { .name="ssid", .type=FT_USERNAME },
    { .name="pass", .type=FT_PASSWORD },
};
#define variables_mqtt_len 4
struct variable variables_mqtt[] = {
    { .name="host", .type=FT_HOSTNAME },
    { .name="port", .type=FT_PORT },
    { .name="user", .type=FT_USERNAME },
    { .name="pass", .type=FT_PASSWORD },
};
#define variables_log_len 3
struct variable variables_log[] = {
    { .name="host", .type=FT_HOSTNAME },
    { .name="port", .type=FT_PORT },
    { .name="enable", .type=FT_CHECKBOX },
};
const int nrofSettings=3;
static struct setting settings[]= {
    { .name="wifi", .nrofVariables=variables_wifi_len, .variables=&variables_wifi },
    { .name="mqtt", .nrofVariables=variables_mqtt_len, .variables=&variables_mqtt },
    { .name="log", .nrofVariables=variables_log_len, .variables=&variables_log },
};
