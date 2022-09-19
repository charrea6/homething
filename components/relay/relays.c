#include <stdlib.h>
#include <string.h>
#include "relay.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "iot.h"

struct RelayMapEntry {
    const char *id;
    Relay_t *relay;
    struct RelayMapEntry *next;
};

static const char TAG[]="relays";

static struct RelayMapEntry *rootEntry = NULL;

void relayRegister(Relay_t *relay, const char *id)
{
    struct RelayMapEntry *entry;
    entry = calloc(1, sizeof(struct RelayMapEntry));
    if (entry == NULL) {
        ESP_LOGE(TAG, "Failed to allocate entry for %s", id);
        return;
    }
    entry->id = id;
    entry->relay = relay;
    entry->next = rootEntry;
    rootEntry = entry;
}

Relay_t *relayFind(const char *id)
{
    struct RelayMapEntry *entry;   
    for (entry = rootEntry; entry; entry = entry->next) {
        if (strcmp(id, entry->id) == 0) {
            return entry->relay;
        }
    }
    return NULL;
}