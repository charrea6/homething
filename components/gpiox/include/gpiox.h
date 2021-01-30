#ifndef _GPIOX_H_
#define _GPIOX_H_
#include <stdint.h>
#include "sdkconfig.h"

/*
  Nodemcu Pin Names

  |    Pins       |
  |GPIO | NodeMcu |
  |-----|---------|
  |  0  |   D3    |
  |  1  |   TX    |
  |  2  |   D4    |
  |  3  |   RX    |
  |  4  |   D2    |
  |  5  |   D1    |
  |6-11 |   --    |
  | 12  |   D6    |
  | 13  |   D7    |
  | 14  |   D5    |
  | 15  |   D8    |
  | 16  |   D0    |

GPIO0(D3) - used to indicate to bootloader to go into upload mode + tied to Flash button.
GPIO16(D0) - possibly tied to RST to allow exit from deep sleep by pulling GPIO16 low.

| LED | Pin |
|-----|-----|
| Red | 16  |
| Blue|  2  |

*/
#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 14
#define D6 12
#define D7 13
#define D8 15
#define RX 3
#define TX 1
#define SD3 10
#define SD2 9

#if CONFIG_GPIOX_EXPANDERS == 1
#define GPIOX_PINS_SIZE 2
#define GPIOX_BASE 32
#define X1 (GPIOX_BASE)
#define X2 (GPIOX_BASE + 1)
#define X3 (GPIOX_BASE + 2)
#define X4 (GPIOX_BASE + 3)
#define X5 (GPIOX_BASE + 4)
#define X6 (GPIOX_BASE + 5)
#define X7 (GPIOX_BASE + 6)
#define X8 (GPIOX_BASE + 7)
#define X9 (GPIOX_BASE + 8)
#define X10 (GPIOX_BASE + 9)
#define X11 (GPIOX_BASE + 10)
#define X12 (GPIOX_BASE + 11)
#define X13 (GPIOX_BASE + 12)
#define X14 (GPIOX_BASE + 13)
#define X15 (GPIOX_BASE + 14)
#define X16 (GPIOX_BASE + 15)
#define X17 (GPIOX_BASE + 16)
#define X18 (GPIOX_BASE + 17)
#define X19 (GPIOX_BASE + 18)
#define X20 (GPIOX_BASE + 19)
#define X21 (GPIOX_BASE + 20)
#define X22 (GPIOX_BASE + 21)
#define X23 (GPIOX_BASE + 22)
#define X24 (GPIOX_BASE + 23)
#define X25 (GPIOX_BASE + 24)
#define X26 (GPIOX_BASE + 25)
#define X27 (GPIOX_BASE + 26)
#define X28 (GPIOX_BASE + 27)
#define X29 (GPIOX_BASE + 28)
#define X30 (GPIOX_BASE + 29)
#define X31 (GPIOX_BASE + 30)
#define X32 (GPIOX_BASE + 31)

#define GPIOX_PINS_SET(_pins, _pin) (_pins).pins[_pin/32] |= 1 << (_pin % 32)
#define GPIOX_PINS_CLEAR(_pins, _pin) ((_pins).pins[_pin/32] = ((_pins).pins[_pin/32] & ~(1 << (_pin % 32))))
#define GPIOX_PINS_CLEAR_ALL(_pins) do{(_pins).pins[0] = 0;(_pins).pins[1] = 0;}while(0)
#define GPIOX_PINS_IS_SET(_pins, _pin) (((_pins).pins[_pin/32] & 1 << (_pin % 32)) != 0)
#define GPIOX_PINS_DIFF(_result, _pins1, _pins2) do{(_result).pins[0] = (_pins1).pins[0] ^ (_pins2).pins[0]; (_result).pins[1] = (_pins1).pins[1] ^ (_pins2).pins[1]; }while(0)
#else

#define GPIOX_PINS_SIZE 1
#define GPIOX_PINS_SET(_pins, _pin) (_pins).pins[0] |= 1 << (_pin % 32)
#define GPIOX_PINS_CLEAR(_pins, _pin) ((_pins).pins[0] = ((_pins).pins[0] & ~(1 << (_pin % 32))))
#define GPIOX_PINS_CLEAR_ALL(_pins)  (_pins).pins[0] = 0
#define GPIOX_PINS_IS_SET(_pins, _pin) (((_pins).pins[0] & 1 << (_pin % 32)) != 0)
#define GPIOX_PINS_DIFF(_result, _pins1, _pins2) (_result).pins[0] = (_pins1).pins[0] ^ (_pins2).pins[0]
#endif

typedef struct {
    uint32_t pins[GPIOX_PINS_SIZE];
} GPIOX_Pins_t;

typedef enum {
    GPIOX_MODE_OUT,
    GPIOX_MODE_IN_FLOAT,
    GPIOX_MODE_IN_PULLUP,
    GPIOX_MODE_IN_PULLDOWN,
} GPIOX_Mode_t;

#define GPIOX_PINS_MAX (GPIOX_PINS_SIZE * 32)

int gpioxInit(void);
int gpioxSetup(GPIOX_Pins_t *pins, GPIOX_Mode_t mode);
int gpioxGetPins(GPIOX_Pins_t *pins, GPIOX_Pins_t *values);
int gpioxSetPins(GPIOX_Pins_t *pins, GPIOX_Pins_t *values);
#endif
