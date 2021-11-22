#ifndef _CJSON_UTILS_H_
#define _CJSON_UTILS_H_
#include "cJSON.h"

cJSON* cJSON_AddObjectToObjectCS(cJSON * const object, const char * const name);
cJSON* cJSON_AddArrayToObjectCS(cJSON * const object, const char * const name);
cJSON* cJSON_AddNumberToObjectCS(cJSON * const object, const char * const name, const double number);
cJSON* cJSON_AddStringToObjectCS(cJSON *object, const char * const name, const char * const string);
cJSON* cJSON_AddStringReferenceToObjectCS(cJSON *object, const char * const name, const char * const string);
#endif