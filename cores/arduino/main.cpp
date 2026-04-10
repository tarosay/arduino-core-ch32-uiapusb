#define ARDUINO_MAIN

#include "Arduino.h"
#include "uiapusb.h"

extern "C" void pre_init(void);

extern "C" int main(void)
{
    uiapusb_begin();
    setup();
    while (1)
    {
        loop();
    }
}