#ifndef _SWITCH_H_
#define _SWITCH_H_

typedef void (*SwitchCallback_t)(void*);

void switchAdd(int pin, SwitchCallback_t cb, void *userData);
void switchStart();
#endif
