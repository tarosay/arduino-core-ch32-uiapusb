#include "UIAPRawHID.h"

#define INSTANCE_DESCRIPTORS 1

extern "C" {
#include "rv003usb.h"
}
#include "usb_config.h"

volatile uint8_t UIAPRawHID::_configured = 0;

UIAPRawHID RawHID;

// -----------------------------------------------------------------------------
// Internal buffers
// -----------------------------------------------------------------------------

static volatile uint8_t g_usb_started = 0;

static volatile uint8_t g_tx_pending = 0;
static volatile uint8_t g_tx_len = 0;
static uint8_t g_tx_buf[UIAPRawHID::ReportSize];

static volatile uint8_t g_rx_ready = 0;
static volatile uint8_t g_rx_len = 0;
static uint8_t g_rx_buf[UIAPRawHID::ReportSize];

extern "C" {

// rv003usb expects this symbol.
//uint32_t always0_storage = 0;
//uint32_t * always0 = &always0_storage;

void uiapusb_init(void)
{
    if (g_usb_started) {
        return;
    }

    g_tx_pending = 0;
    g_tx_len = 0;
    g_rx_ready = 0;
    g_rx_len = 0;
    UIAPRawHID::_configured = 0;

    usb_setup();
    g_usb_started = 1;
}

int uiapusb_send_report(const uint8_t* data, int len)
{
    if (!data || len <= 0) {
        return 0;
    }

    if (len > UIAPRawHID::ReportSize) {
        len = UIAPRawHID::ReportSize;
    }

    if (g_tx_pending) {
        return 0;
    }

    for (int i = 0; i < len; ++i) {
        g_tx_buf[i] = data[i];
    }
    for (int i = len; i < UIAPRawHID::ReportSize; ++i) {
        g_tx_buf[i] = 0;
    }

    g_tx_len = (uint8_t)len;
    g_tx_pending = 1;
    return len;
}

int uiapusb_available(void)
{
    return g_rx_ready ? g_rx_len : 0;
}

int uiapusb_recv_report(uint8_t* data, int maxlen)
{
    if (!data || maxlen <= 0 || !g_rx_ready) {
        return 0;
    }

    int n = g_rx_len;
    if (n > maxlen) {
        n = maxlen;
    }

    for (int i = 0; i < n; ++i) {
        data[i] = g_rx_buf[i];
    }

    g_rx_ready = 0;
    g_rx_len = 0;
    return n;
}

// -----------------------------------------------------------------------------
// rv003usb callbacks / handlers
// -----------------------------------------------------------------------------

void usb_handle_user_in_request(struct usb_endpoint * e,
                                uint8_t * scratchpad,
                                int endp,
                                uint32_t sendtok,
                                struct rv003usb_internal * ist)
{
    (void)scratchpad;
    (void)ist;

    if (endp != 1) {
        usb_send_empty(sendtok);
        return;
    }

    if (!g_tx_pending) {
        usb_send_empty(sendtok);
        return;
    }

    usb_send_data(g_tx_buf, UIAPRawHID::ReportSize, 0, sendtok);
    g_tx_pending = 0;

    if (e) {
        e->count = 0;
    }

    UIAPRawHID::_configured = 1;
}

void usb_handle_user_data(struct usb_endpoint * e,
                          int current_endpoint,
                          uint8_t * data,
                          int len,
                          struct rv003usb_internal * ist)
{
    (void)e;
    (void)ist;

    if (current_endpoint != 0 || !data || len <= 0) {
        return;
    }

    if (len > UIAPRawHID::ReportSize) {
        len = UIAPRawHID::ReportSize;
    }

    for (int i = 0; i < len; ++i) {
        g_rx_buf[i] = data[i];
    }

    g_rx_len = (uint8_t)len;
    g_rx_ready = 1;
    UIAPRawHID::_configured = 1;
}

void usb_handle_hid_set_report_start(struct usb_endpoint * e,
                                     int reqLen,
                                     uint32_t lValueLSBIndexMSB)
{
    (void)lValueLSBIndexMSB;

    if (!e) {
        return;
    }

    if (reqLen > UIAPRawHID::ReportSize) {
        reqLen = UIAPRawHID::ReportSize;
    }
    if (reqLen < 0) {
        reqLen = 0;
    }

    e->opaque = g_rx_buf;
    e->max_len = (uint16_t)reqLen;
    e->count = 0;
}

void usb_handle_hid_get_report_start(struct usb_endpoint * e,
                                     int reqLen,
                                     uint32_t lValueLSBIndexMSB)
{
    (void)lValueLSBIndexMSB;

    if (!e) {
        return;
    }

    if (reqLen > UIAPRawHID::ReportSize) {
        reqLen = UIAPRawHID::ReportSize;
    }
    if (reqLen < 0) {
        reqLen = 0;
    }

    e->opaque = g_tx_buf;
    e->max_len = (uint16_t)reqLen;
    e->count = 0;
}

void usb_handle_other_control_message(struct usb_endpoint * e,
                                      struct usb_urb * s,
                                      struct rv003usb_internal * ist)
{
    (void)e;
    (void)s;
    (void)ist;
}

} // extern "C"

// -----------------------------------------------------------------------------
// C++ wrapper
// -----------------------------------------------------------------------------

bool UIAPRawHID::begin()
{
    uiapusb_init();
    return true;
}

bool UIAPRawHID::send(const void* data, int len)
{
    return uiapusb_send_report((const uint8_t*)data, len) > 0;
}

int UIAPRawHID::available() const
{
    return uiapusb_available();
}

int UIAPRawHID::recv(void* data, int maxlen)
{
    return uiapusb_recv_report((uint8_t*)data, maxlen);
}