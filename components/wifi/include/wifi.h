#ifndef _WIFI_H_
#define _WIFI_H_
#include <stdbool.h>
#include "esp_wifi.h"

typedef void (*wifiScanCallback_t)(uint8_t nrofAPs, wifi_ap_record_t *records);
/**
 * Initialise Wifi stack ready to create a station connection or access point for provisioning.
 * returns 0 on success or non-zero on error.
 */
int wifiInit(void);

/**
 * Start wifi stack and attempts to connect to the configured access point.
 */
void wifiStart(void);

/**
 * Retrieve the current IP address as a correctly formated string.
 * Return a pointer to a static string.
 */
const char *wifiGetIPAddrStr(void);

/**
 * Used to determine if Wifi Station is connected to an access point and has an IP.
 * Return true if connected, false otherwise.
 */
bool wifiIsConnected(void);

/**
 * Initiate a wifi scan and report the result via the supplied callback.
 * returns 0 on success or non-zero on error.
 */
int wifiScan(wifiScanCallback_t callback);

/**
 * Returns the number of times, since boot, that this device has connected to an access point.
 */
uint32_t wifiGetConnectionCount();

/**
 * Get the name of the Wifi network the device should connect to.
 * Returns a string that does not need to be freed.
 */
const char* wifiGetConnectionSSID();
#endif