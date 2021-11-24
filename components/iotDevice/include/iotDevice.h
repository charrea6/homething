#ifndef _IOTDEVICE_H_
#define _IOTDEVICE_H_
/**
 * Initialise the device element with the supplied version and capabilites.
 */
int iotDeviceInit(const char *version, const char *capabilities);

/**
 * Update the device status string.
 */
void iotDeviceUpdateStatus(char *status);
#endif