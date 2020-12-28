#include <stdlib.h>
#include "esp_log.h"
#include "notifications.h"

typedef struct NotificationsCallbackDetails{
    NotificationsCallback_t callback;
    void *user;
    uint32_t id;
    struct NotificationsCallbackDetails *next;
} NotificationsCallbackDetails_t;

static NotificationsCallbackDetails_t *callbacks[Notifications_Class_Max];
static const char TAG[]="notifications";

void notificationsInit(void){
    int i;
    for (i=0; i < Notifications_Class_Max; i ++) {
        callbacks[i] = NULL;
    }
}

void notificationsRegister(Notifications_Class_e clazz, uint32_t id, NotificationsCallback_t callback, void *user){
    if (clazz >= Notifications_Class_Max) {
        ESP_LOGE(TAG, "Register: Class %d >= %d", clazz, Notifications_Class_Max);
        return;
    }
    
    NotificationsCallbackDetails_t *details, *end;
    details = malloc(sizeof(NotificationsCallbackDetails_t));
    if (details == NULL) {
        ESP_LOGE(TAG, "Register: Failed to allocate callback details struct");
        return;
    }

    details->id = id;
    details->callback = callback;
    details->user = user; 
    details->next = NULL;
    if (callbacks[clazz] == NULL) {
        callbacks[clazz] = details;
    } else {
        for (end = callbacks[clazz]; end->next != NULL; end = end->next);
        end->next = details;
    }
}

void notificationsNotify(Notifications_Class_e clazz, uint32_t id, NotificationsData_t *data){
    if (clazz >= Notifications_Class_Max) {
        ESP_LOGE(TAG, "Notify: Class %d >= %d", clazz, Notifications_Class_Max);
        return;
    }

    NotificationsMessage_t message;
    NotificationsCallbackDetails_t *callback;

    message.id = id;
    message.clazz = clazz;
    message.data = *data;
    
    for (callback = callbacks[clazz]; callback; callback = callback->next) {
        if ((callback->id == NOTIFICATIONS_ID_ALL) || (id == callback->id)){
            callback->callback(callback->user, &message);
        }    
    }
}