#ifndef _SAFESTRING_H_
#define _SAFESTRING_H_
#include "stdlib.h"

int _safestrcmp(const char *constant, size_t conLen, const char *variable, size_t len);
#define safestrcmp(constant, variable, len) _safestrcmp(constant, sizeof("" constant), variable, len)

#endif