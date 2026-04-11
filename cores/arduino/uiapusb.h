#ifndef _UIAPUSB_H
#define _UIAPUSB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * core 側の最低限初期化。
 * ここではクロック/時間基準だけを初期化し、
 * USB デバイス機能は起動しない。
 */
void uiap_core_begin(void);

/*
 * USB/HID を使う時だけ呼ぶ初期化。
 * core 初期化は別で済んでいる前提だが、
 * 念のため内部でも必要なら呼べるようにしてある。
 */
void uiapusb_begin(void);

int  uiapusb_available(void);
int  uiapusb_read(uint8_t *buf, int maxlen);
int  uiapusb_write(const uint8_t *buf, int len);

#ifdef __cplusplus
}
#endif

#endif