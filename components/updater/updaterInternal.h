#ifndef _UPDATERINTERNAL_H_
#define _UPDATERINTERNAL_H_

#define UPDATER_STATUS_BUFFER_SIZE 64

#define updaterUpdateStatusf(fmt...) snprintf(updaterStatusBuffer, UPDATER_STATUS_BUFFER_SIZE, fmt); updaterUpdateStatus(updaterStatusBuffer)
extern char updaterStatusBuffer[];

void updaterUpdateStatus(char *);
void updaterDownloadAndUpdate(char *host, int port, char *path);
#endif