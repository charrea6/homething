
#include "cJSON_AddOns.h"

cJSON* cJSON_AddObjectToObjectCS(cJSON * const object, const char * const name)
{
    cJSON *object_item = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(object, name, object_item);
    return object_item;
}

cJSON* cJSON_AddArrayToObjectCS(cJSON * const object, const char * const name)
{
    cJSON *array = cJSON_CreateArray();
    cJSON_AddItemToObjectCS(object, name, array);
    return array;
}


cJSON* cJSON_AddNumberToObjectCS(cJSON * const object, const char * const name, const double number)
{
    cJSON *number_item = cJSON_CreateNumber(number);
    cJSON_AddItemToObjectCS(object, name, number_item);
    return number_item;
}

cJSON* cJSON_AddStringToObjectCS(cJSON *object, const char * const name, const char * const string)
{
    cJSON *string_item = cJSON_CreateString(string);
    cJSON_AddItemToObjectCS(object, name, string_item);
    return string_item;
}

cJSON* cJSON_AddStringReferenceToObjectCS(cJSON *object, const char * const name, const char * const string)
{
    cJSON *string_item = cJSON_CreateStringReference(string);
    cJSON_AddItemToObjectCS(object, name, string_item);
    return string_item;
}