#ifndef _DEVICEPROFILE_H_
#define _DEVICEPROFILE_H_

#include <stddef.h>
#include <stdint.h>
#include "component_config.h"

int deviceProfileGetProfile(const char **profile);
int deviceProfileSetProfile(const char *profile);
int deviceProfileDeserialize(const char *profile, DeviceProfile_DeviceConfig_t *config);

#endif
