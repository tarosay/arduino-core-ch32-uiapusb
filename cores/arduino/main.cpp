#define ARDUINO_MAIN

#include "Arduino.h"
#include "uiapusb.h"

extern "C" int main(void)
{
    /*
     * core 初期化だけは常時実行する。
     *
     * これにより:
     * - delay()
     * - millis()
     * - micros()
     *
     * が HIDuiap.begin() を呼ばないスケッチでも成立する。
     *
     * 一方で usb_setup() はここでは呼ばないので、
     * USB デバイス機能そのものは起動しない。
     */
    uiap_core_begin();

    setup();

    while (1)
    {
        loop();
    }
}