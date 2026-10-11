#pragma once
#include <cstdint>
// Mock API for exercising the real transport logic on a host. The target build
// always uses the PS5 SDK's actual ioctl structures and request constants.
struct usb_fs_endpoint {
    void **ppBuffer; uint32_t *pLength;
    uint32_t nFrames, aFrames;
    uint16_t flags, timeout, isoc_time_complete;
    int status;
};
struct usb_fs_init { usb_fs_endpoint *pEndpoints; uint8_t ep_index_max; };
struct usb_fs_uninit { uint8_t dummy; };
struct usb_fs_open { uint32_t max_bufsize, max_frames; uint16_t max_packet_length; uint8_t dev_index, ep_index, ep_no; };
struct usb_fs_start { uint8_t ep_index; };
struct usb_fs_stop { uint8_t ep_index; };
struct usb_fs_complete { uint8_t ep_index; };
struct usb_device_descriptor { uint8_t idVendor[2], idProduct[2]; };
struct usb_ctl_request {
    struct { uint8_t bmRequestType, bRequest, wValue[2], wIndex[2], wLength[2]; } ucr_request;
    void *ucr_data;
    uint16_t ucr_flags, ucr_actlen;
};
#define USETW(field, value) do { (field)[0] = (value) & 255; (field)[1] = ((value) >> 8) & 255; } while (0)
#define UGETW(field) (static_cast<unsigned>((field)[0]) | (static_cast<unsigned>((field)[1]) << 8))
enum { USB_GET_CONFIG = 1, USB_SET_CONFIG, USB_CLAIM_INTERFACE, USB_RELEASE_INTERFACE,
       USB_FS_INIT, USB_FS_OPEN, USB_FS_UNINIT, USB_DO_REQUEST, USB_FS_START,
       USB_FS_COMPLETE, USB_FS_STOP, USB_GET_DEVICE_DESC };
enum { USB_ERR_NORMAL_COMPLETION = 0, USB_ERR_SHORT_XFER = 1, USB_ERR_TIMEOUT = 2, USB_ERR_IOERROR = 3 };
#define USB_FS_FLAG_SINGLE_SHORT_OK 1
int xbox_test_ioctl(int fd, unsigned long request, void *argument);
int xbox_test_close(int fd);
