#ifndef _WIFI_H_
#define _WIFI_H_
#include <stdbool.h>

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
#endif