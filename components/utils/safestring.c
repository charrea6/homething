#include <string.h>
#include "safestring.h"

int _safestrcmp(const char *constant, size_t conLen, const char *variable, size_t len)
{
    if (conLen > len) {
        return -1;
    }
    return strncmp(constant, variable, len);
}
