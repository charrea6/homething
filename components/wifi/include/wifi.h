#ifndef _WIFI_H_
#define _WIFI_H_
#include <stdbool.h>

/**
 * Station connection status callback, bool will be true when a connection has been successfully
 * established and false when disconnected.
 */
typedef void (*WifiConnectionCallback_t)(bool);

/**
 * Initialise Wifi stack ready to create a station connection or access point for provisioning.
 * callback - Called when the station connection changes state, true - connected, false - disconnected.
  * returns 0 on success or non-zero on error.
  */
int wifiInit(WifiConnectionCallback_t callback);

/**
 * Start wifi stack
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