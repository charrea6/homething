#ifndef _SWITCH_H_
#define _SWITCH_H_

typedef void (*SwitchCallback_t)(void* userData, int state);

int switchInit(int max);
void switchAdd(int pin, SwitchCallback_t cb, void *userData);
void switchStart();
#endif
