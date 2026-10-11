#ifndef LIBREPAD_TEST_USB_IOCTL_H
#define LIBREPAD_TEST_USB_IOCTL_H
#include "usb.h"
struct usb_fs_endpoint {
    void **ppBuffer; uint32_t *pLength;
    uint32_t nFrames, aFrames;
    uint16_t flags, timeout, isoc_time_complete;
    int status;
};
struct usb_fs_init { struct usb_fs_endpoint *pEndpoints; uint8_t ep_index_max; };
struct usb_fs_uninit { uint8_t dummy; };
struct usb_fs_open { uint32_t max_bufsize, max_frames; uint16_t max_packet_length; uint8_t dev_index, ep_index, ep_no; };
struct usb_fs_start { uint8_t ep_index; };
struct usb_fs_stop { uint8_t ep_index; };
struct usb_fs_complete { uint8_t ep_index; };
struct usb_gen_descriptor { void *ugd_data; uint16_t ugd_maxlen, ugd_actlen; };
enum { USB_GET_CONFIG=1, USB_SET_CONFIG, USB_CLAIM_INTERFACE, USB_FS_INIT,
       USB_FS_OPEN, USB_FS_UNINIT, USB_DO_REQUEST, USB_FS_START, USB_FS_COMPLETE,
       USB_FS_STOP, USB_GET_DEVICE_DESC, USB_GET_FULL_DESC };
#define USB_FS_FLAG_SINGLE_SHORT_OK 1
#define USB_FS_FLAG_FORCE_SHORT 4
#endif
