#ifndef _UPDATE_H_
#define _UPDATE_H_

#define UPDATER_STATUS_BUFFER_SIZE 64

#define updaterUpdateStatusf(fmt...) snprintf(updaterStatusBuffer, UPDATER_STATUS_BUFFER_SIZE, fmt); updaterUpdateStatus(updaterStatusBuffer)
extern char updaterStatusBuffer[];

void updaterUpdateStatus(char *);

void updaterUpdate(char *host, int port, char *path);

/** Initialise the OTA updater subsystem.
 * The subsystem is initialised with a profile (generate at build time) that is used to identify builds for
 * this type of device. This will be a string defining the functionality in the build, for example 'LLLTHM'
 * which might signify 3 lights (L) a temperature sensor (T), Humidity fan (H) and a motion sensor (M).
 *
 * This profile will be combined with the configured prefix and the version number to a path to download from
 * an HTTP server.
 * ie.
 * <configured prefix>/<build profile>/<version>.ota
  */
void updaterInit();

char *updaterGetVersion(void);
#endif
