#ifndef LIBREPAD_PROTOCOL_DECODER_H
#define LIBREPAD_PROTOCOL_DECODER_H

#include "core/controller_state.h"
#include <stdint.h>
#include <stddef.h>

typedef enum {
    CTRL_UNKNOWN = 0,
    CTRL_NINTENDO_SWITCH_PRO,
    CTRL_SONY_DS4,
    CTRL_SONY_DS3,
    CTRL_XBOX_XINPUT,
    CTRL_8BITDO_ADAPTER,
    CTRL_GENERIC_HID,
    CTRL_PDP_FACEOFF
} usb_controller_type_t;

#define CTRL_BLUETOOTH_MAPPING 100

typedef struct {
    uint8_t interface, in_address, out_address;
    int gip;
} usb_input_interface_t;

int usb_select_input_interface(const uint8_t *data, size_t length,
                               usb_controller_type_t driver, usb_input_interface_t *out);

usb_controller_type_t usb_identify_controller(uint16_t vid, uint16_t pid, const char **name_out);
int usb_init_controller_handshake(int fd, usb_controller_type_t type, uint8_t interface);
int usb_parse_input_report(usb_controller_type_t type, const uint8_t *data, size_t len, pad_state_t *st);
int usb_parse_stateful_report(usb_controller_type_t type, const uint8_t *data, size_t len,
                             pad_state_t *previous, pad_state_t *state);
uint8_t axis16_to_u8(int16_t val, int invert_y);

#endif /* USB_CONTROLLERS_H */
