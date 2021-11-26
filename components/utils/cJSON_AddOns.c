#include "cJSON_AddOns.h"
#include "stdio.h"

cJSON* cJSON_AddObjectToObjectCS(cJSON * const object, const char * const name)
{
    cJSON *objectItem = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(object, name, objectItem);
    return objectItem;
}

cJSON* cJSON_AddArrayToObjectCS(cJSON * const object, const char * const name)
{
    cJSON *array = cJSON_CreateArray();
    cJSON_AddItemToObjectCS(object, name, array);
    return array;
}


cJSON* cJSON_AddUIntToObjectCS(cJSON * const object, const char * const name, const uint32_t number)
{
    char numberStr[11];
    sprintf(numberStr, "%u", number);
    cJSON *numberItem = cJSON_CreateRaw(numberStr);
    cJSON_AddItemToObjectCS(object, name, numberItem);
    return numberItem;
}

cJSON* cJSON_AddStringToObjectCS(cJSON *object, const char * const name, const char * const string)
{
    cJSON *stringItem = cJSON_CreateString(string);
    cJSON_AddItemToObjectCS(object, name, stringItem);
    return stringItem;
}

cJSON* cJSON_AddStringReferenceToObjectCS(cJSON *object, const char * const name, const char * const string)
{
    cJSON *stringItem = cJSON_CreateStringReference(string);
    cJSON_AddItemToObjectCS(object, name, stringItem);
    return stringItem;
}