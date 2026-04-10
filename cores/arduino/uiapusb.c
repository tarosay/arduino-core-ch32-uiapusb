#include <ch32fun.h>
#include <stdio.h>
#include <stdint.h>
#include "rv003usb.h"
#include "uiapusb.h"

#define UIAPUSB_RX_BUF_SIZE 256

static volatile uint8_t rxbuf[UIAPUSB_RX_BUF_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;

static inline uint16_t rb_next(uint16_t v)
{
    return (uint16_t)((v + 1) % UIAPUSB_RX_BUF_SIZE);
}

static inline int rb_count(void)
{
    if (rx_head >= rx_tail) return (int)(rx_head - rx_tail);
    return (int)(UIAPUSB_RX_BUF_SIZE - rx_tail + rx_head);
}

/*
 * demo_terminal.c と同じ受信入口
 * WebLink tタブから来たデータはここに入る
 */
void handle_debug_input(int numbytes, uint8_t *data)
{
    for (int i = 0; i < numbytes; i++)
    {
        uint16_t n = rb_next(rx_head);
        if (n == rx_tail)
        {
            break; // overflowしたら捨てる
        }
        rxbuf[rx_head] = data[i];
        rx_head = n;
    }
}

void uiapusb_begin(void)
{
    SystemInit();
    SysTick->CNT = 0;
    Delay_Ms(1);   // USB再列挙待ち
    usb_setup();
}

int uiapusb_available(void)
{
    return rb_count();
}

int uiapusb_read(uint8_t *buf, int maxlen)
{
    int n = 0;
    while (n < maxlen && rx_tail != rx_head)
    {
        buf[n++] = rxbuf[rx_tail];
        rx_tail = rb_next(rx_tail);
    }
    return n;
}

int uiapusb_write(const uint8_t *buf, int len)
{
    for (int i = 0; i < len; i++)
    {
        putchar(buf[i]);
    }
    return len;
}