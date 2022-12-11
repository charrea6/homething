#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "notifications.h"

typedef struct NotificationsCallbackDetails {
    NotificationsCallback_t callback;
    void *user;
    uint32_t id;
    struct NotificationsCallbackDetails *next;
} NotificationsCallbackDetails_t;

typedef struct NotificationsNamedID {
    const char *name;
    Notifications_ID_t id;
    struct NotificationsNamedID *next;
} NotificationsNamedID_t;

static NotificationsCallbackDetails_t *callbacks[Notifications_Class_Max];
static const char TAG[]="notifications";
static Notifications_ID_t nextID = NOTIFICATIONS_ID_DYNAMIC_START;
static NotificationsNamedID_t *rootEntry = NULL;

void notificationsInit(void)
{
    int i;
    for (i=0; i < Notifications_Class_Max; i ++) {
        callbacks[i] = NULL;
    }
}

void notificationsRegister(Notifications_Class_e clazz, Notifications_ID_t id, NotificationsCallback_t callback, void *user)
{
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

void notificationsUnregister(Notifications_Class_e clazz, Notifications_ID_t id, NotificationsCallback_t callback, void *user)
{
    NotificationsCallbackDetails_t *current = NULL, *prev = NULL;

    for (current = callbacks[clazz]; current; current = current->next) {
        if ((current->id == id) && (current->callback == callback) && (current->user == user)) {
            if (prev) {
                prev->next = current->next;
            } else {
                callbacks[clazz] = current->next;
            }
            free(current);
            break;
        }
        prev = current;
    }
}

void notificationsNotify(Notifications_Class_e clazz, Notifications_ID_t id, NotificationsData_t *data)
{
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
        if ((callback->id == NOTIFICATIONS_ID_ALL) || (id == callback->id)) {
            callback->callback(callback->user, &message);
        }
    }
}

Notifications_ID_t notificationsNewId(const char *name)
{
    Notifications_ID_t id = nextID;
    NotificationsNamedID_t *entry;

    nextID ++;
    if (name == NULL) {
        return id;
    }

    entry = calloc(1, sizeof(NotificationsNamedID_t));
    if (entry == NULL) {
        return NOTIFICATIONS_ID_ERROR;
    }

    entry->id = id;
    entry->name = name;
    entry->next = rootEntry;
    rootEntry = entry;
    return id;
}

int notificationsRegisterId(Notifications_ID_t id, const char *name)
{
    NotificationsNamedID_t *entry;

    entry = calloc(1, sizeof(NotificationsNamedID_t));
    if (entry == NULL) {
        return -1;
    }

    entry->id = id;
    entry->name = name;
    entry->next = rootEntry;
    rootEntry = entry;
    return 0;
}

Notifications_ID_t notificationsFindId(const char *name)
{
    NotificationsNamedID_t *entry;
    for (entry = rootEntry; entry; entry = entry->next) {
        if (strcmp(name, entry->name) == 0) {
            return entry->id;
        }
    }
    return NOTIFICATIONS_ID_ERROR;
}
