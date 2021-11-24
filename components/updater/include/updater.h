#ifndef _UPDATE_H_
#define _UPDATE_H_

typedef void (*updaterStatusCallback_t)(char *);

/** Initialise the OTA updater subsystem.
 */
void updaterInit();

/**
 * Update the firmware the version specified in updateVersion
 */
void updaterUpdate(const char *updateVersion);

/**
 * Add a callback to be called when the updater has a status message to send to the user.
 */
void updaterAddStatusCallback(updaterStatusCallback_t callback);
#endif
