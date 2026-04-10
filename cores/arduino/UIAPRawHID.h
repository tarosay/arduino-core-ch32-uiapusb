#ifndef _UIAP_RAW_HID_H_
#define _UIAP_RAW_HID_H_

#include <Arduino.h>

class UIAPRawHID {
public:
    static const uint8_t ReportSize = 8;

    bool begin();
    bool send(const void* data, int len);
    int available() const;
    int recv(void* data, int maxlen);

    static volatile uint8_t _configured;
private:

};

extern UIAPRawHID RawHID;

#ifdef __cplusplus
extern "C" {
#endif

void uiapusb_init(void);
int  uiapusb_send_report(const uint8_t* data, int len);
int  uiapusb_available(void);
int  uiapusb_recv_report(uint8_t* data, int maxlen);

#ifdef __cplusplus
}
#endif

#endif