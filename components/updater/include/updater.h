#ifndef _UPDATE_H_
#define _UPDATE_H_

#define UPDATER_STATUS_BUFFER_SIZE 64

#define updaterUpdateStatusf(fmt...) snprintf(updaterStatusBuffer, UPDATER_STATUS_BUFFER_SIZE, fmt); updaterUpdateStatus(updaterStatusBuffer)

void updaterUpdateStatus(char *);
void updaterUpdate(char *host, int port, char *path);
void updaterInit(void);

extern char updaterStatusBuffer[];
#endif
