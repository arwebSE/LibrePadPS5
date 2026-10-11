/* Minimal host mock of the target USB API. Target builds use SDK headers. */
#ifndef LIBREPAD_TEST_USB_H
#define LIBREPAD_TEST_USB_H
#include <stdint.h>
struct usb_device_descriptor { uint8_t bDeviceClass, idVendor[2], idProduct[2]; };
struct usb_ctl_request {
    struct { uint8_t bmRequestType, bRequest, wValue[2], wIndex[2], wLength[2]; } ucr_request;
    void *ucr_data;
    uint16_t ucr_flags, ucr_actlen;
};
enum { USB_ERR_NORMAL_COMPLETION, USB_ERR_SHORT_XFER, USB_ERR_TIMEOUT, USB_ERR_IOERROR };
#define USB_SHORT_XFER_OK 1
#endif
