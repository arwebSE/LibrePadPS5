#include "input/protocol_decoder.h"
#include "core/controller_state.h"
#include "core/logging.h"
#include "core/time_utils.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef __PROSPERO__
#include <dev/usb/usb.h>
#include <dev/usb/usb_ioctl.h>
#include <dev/usb/usb_endian.h>
#endif

/* Adapter 2 exposes this non-gamepad identity while no controller is connected.
 * See https://joypad.ai/controllers/8bitdo-wireless-usb-adapter. */
int usb_receiver_is_idle(uint16_t vid, uint16_t pid)
{
    return vid == 0x2dc8 && pid == 0x3107;
}

/* Exact protocol IDs. Other known vendors require a real HID gamepad descriptor. */
usb_controller_type_t usb_identify_controller(uint16_t vid, uint16_t pid, const char **name_out)
{
    if (usb_receiver_is_idle(vid, pid)) {
        if (name_out) *name_out = "8BitDo Adapter 2 waiting for controller";
        return CTRL_UNKNOWN;
    }
    static const struct { uint16_t vid, pid; usb_controller_type_t type; const char *name; } devices[] = {
        {0x0e6f,0x0184,CTRL_PDP_FACEOFF,"8-byte wired gamepad"},
        {0x057e,0x2009,CTRL_NINTENDO_SWITCH_PRO,"Switch Pro"},
        {0x057e,0x2017,CTRL_NINTENDO_SWITCH_PRO,"Nintendo NSO"},
        {0x057e,0x2019,CTRL_NINTENDO_SWITCH_PRO,"Nintendo NSO"},
        {0x057e,0x201a,CTRL_NINTENDO_SWITCH_PRO,"Nintendo NSO"},
        {0x054c,0x05c4,CTRL_SONY_DS4,"DualShock 4"},
        {0x054c,0x09cc,CTRL_SONY_DS4,"DualShock 4"},
        {0x054c,0x0ba0,CTRL_SONY_DS4,"DualShock 4 adapter"},
        {0x054c,0x0268,CTRL_SONY_DS3,"DualShock 3"},
        {0x045e,0x028e,CTRL_XBOX_XINPUT,"Xbox 360"},
        {0x045e,0x02d1,CTRL_XBOX_XINPUT,"Xbox One"},
        {0x045e,0x02dd,CTRL_XBOX_XINPUT,"Xbox One"},
        {0x045e,0x02e3,CTRL_XBOX_XINPUT,"Xbox Elite"},
        {0x045e,0x02ea,CTRL_XBOX_XINPUT,"Xbox One S"},
        {0x045e,0x0b00,CTRL_XBOX_XINPUT,"Xbox Elite Series 2"},
        {0x045e,0x0b05,CTRL_XBOX_XINPUT,"Xbox controller"},
        {0x045e,0x0b12,CTRL_XBOX_XINPUT,"Xbox Series"},
        {0x045e,0x0b13,CTRL_XBOX_XINPUT,"Xbox Series"},
        {0x1a34,0xf517,CTRL_XBOX_XINPUT,"2.4 GHz XInput receiver"},
        {0x2dc8,0x3106,CTRL_8BITDO_ADAPTER,"8BitDo XInput gamepad"},
        {0x2dc8,0x3109,CTRL_8BITDO_ADAPTER,"8BitDo XInput receiver"},
        {0x2dc8,0x310a,CTRL_8BITDO_ADAPTER,"8BitDo adapter"},
        {0x2dc8,0x310b,CTRL_8BITDO_ADAPTER,"8BitDo adapter"},
        {0x2dc8,0x2100,CTRL_8BITDO_ADAPTER,"8BitDo gamepad"},
        {0x2dc8,0x3012,CTRL_8BITDO_ADAPTER,"8BitDo gamepad"}
    };
    static const uint16_t generic_vendors[] = {
        0x2345,0x2563,0x2f24,0x20bc,0x3537,0x0e8f,0x1a34,
        0x0079,0x0810,0x11ff,0x12bd,0x20d6,0x24c6,0x146b,
        0x0738,0x1038,0x1532,0x2dc8
    };
    for (size_t i=0; i<sizeof(devices)/sizeof(devices[0]); i++)
        if (devices[i].vid==vid && devices[i].pid==pid) {
            if (name_out) *name_out=devices[i].name;
            return devices[i].type;
        }
    for (size_t i=0; i<sizeof(generic_vendors)/sizeof(generic_vendors[0]); i++)
        if (generic_vendors[i]==vid) {
            if (name_out) *name_out="Descriptor-validated HID gamepad";
            return CTRL_GENERIC_HID;
        }
    if (name_out) *name_out="Unknown USB device";
    return CTRL_UNKNOWN;
}

/* Keep endpoints together on one alternate-zero interface. 8BitDo XInput
 * devices use the same vendor interface as Xbox 360 pads, not HID reports. */
int usb_select_input_interface(const uint8_t *data, size_t length,
                               usb_controller_type_t driver, usb_input_interface_t *out)
{
    if (!data || !out) return 0;
    memset(out, 0, sizeof(*out));
    usb_input_interface_t candidate = {0}, selected = {0};
    int suitable = 0, selected_score = 0;
    for (size_t offset = 0; offset < length;) {
        if (length-offset < 2) return 0;
        size_t entry = data[offset];
        if (entry < 2 || entry > length-offset) return 0;
        const uint8_t *d = data+offset;
        if (d[1] == 4) {
            if (entry < 9) return 0;
            if (candidate.in_address && suitable > selected_score) {
                selected = candidate; selected_score = suitable;
            }
            memset(&candidate, 0, sizeof(candidate));
            int hid = d[5] == 3 && d[6] == 0 && d[7] == 0;
            int gip = d[5] == 0xff && d[6] == 0x47 && d[7] == 0xd0;
            int xinput = d[5] == 0xff && d[6] == 0x5d && d[7] == 1;
            int xbox = driver == CTRL_XBOX_XINPUT || driver == CTRL_8BITDO_ADAPTER;
            suitable = d[3] != 0 ? 0 : xbox && (gip || xinput) ? 2 : hid ? 1 : 0;
            candidate.interface = d[2]; candidate.gip = gip;
        } else if (d[1] == 5) {
            if (entry < 7) return 0;
            uint8_t address = d[2], type = d[3] & 3;
            if (suitable && (address & 15) && (type == 2 || type == 3)) {
                if (address & 0x80) {
                    if (!candidate.in_address || address == 0x81) candidate.in_address = address;
                } else if (!candidate.out_address) candidate.out_address = address;
            }
        }
        offset += entry;
    }
    if (candidate.in_address && suitable > selected_score) selected = candidate;
    if (!selected.in_address) return 0;
    *out = selected;
    return 1;
}

int usb_init_controller_handshake(int fd, usb_controller_type_t type, uint8_t interface)
{
    (void)type;
    (void)interface;
    if (fd < 0) return -1;

#ifdef __PROSPERO__
    struct usb_ctl_request req;

    if (type == CTRL_NINTENDO_SWITCH_PRO) {
        /* Switch Pro handshake 1: 0x80 0x02 0x01 */
        uint8_t hs1[] = { 0x80, 0x02 };
        memset(&req, 0, sizeof(req));
        req.ucr_request.bmRequestType = 0x21; /* Class, Interface */
        req.ucr_request.bRequest = 0x09;      /* SET_REPORT */
        USETW(req.ucr_request.wValue, 0x0101);
        USETW(req.ucr_request.wIndex, interface);
        USETW(req.ucr_request.wLength, sizeof(hs1));
        req.ucr_data = hs1;
        ioctl(fd, USB_DO_REQUEST, &req);
        usleep(20000);

        /* Handshake 2: 0x80 0x04 */
        uint8_t hs2[] = { 0x80, 0x04 };
        USETW(req.ucr_request.wLength, sizeof(hs2));
        req.ucr_data = hs2;
        ioctl(fd, USB_DO_REQUEST, &req);
        usleep(20000);

        /* Handshake 3: Enable standard 60Hz full reporting (subcommand 0x03 0x30) */
        uint8_t hs3[] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x30 };
        USETW(req.ucr_request.wLength, sizeof(hs3));
        req.ucr_data = hs3;
        ioctl(fd, USB_DO_REQUEST, &req);
        log_line("usb_ctrl: Switch Pro handshake completed");
        return 0;
    }
    else if (type == CTRL_SONY_DS3) {
        /* DualShock 3 magic initialization report */
        uint8_t ds3_init[] = { 0x42, 0x03, 0x00, 0x00 };
        memset(&req, 0, sizeof(req));
        req.ucr_request.bmRequestType = 0x21;
        req.ucr_request.bRequest = 0x09;
        USETW(req.ucr_request.wValue, 0x03f4);
        USETW(req.ucr_request.wIndex, interface);
        USETW(req.ucr_request.wLength, sizeof(ds3_init));
        req.ucr_data = ds3_init;
        ioctl(fd, USB_DO_REQUEST, &req);
        log_line("usb_ctrl: DualShock 3 magic initialization sent");
        return 0;
    }
    else if (type == CTRL_XBOX_XINPUT || type == CTRL_8BITDO_ADAPTER) {
        /* Handshake 1: ShanWan / Machenike / Clone wake-up control transfer.
         * Many third-party 2.4G dongles (045e:028e clones) boot into dormant mode
         * and will not emit input until this control transfer is dispatched. */
        uint8_t dummy[32];
        memset(&req, 0, sizeof(req));
        req.ucr_request.bmRequestType = 0xc1; /* Device to host, vendor, interface */
        req.ucr_request.bRequest = 0x01;
        USETW(req.ucr_request.wValue, 0x0100);
        USETW(req.ucr_request.wIndex, interface);
        USETW(req.ucr_request.wLength, 0x0014);
        req.ucr_data = dummy;
        int r1 = ioctl(fd, USB_DO_REQUEST, &req);
        usleep(20000);

        /* Handshake 2: Xbox 360 Player 1 LED initialization */
        uint8_t x360_led[] = { 0x01, 0x03, 0x02 };
        memset(&req, 0, sizeof(req));
        req.ucr_request.bmRequestType = 0x21; /* Host to device, class, interface */
        req.ucr_request.bRequest = 0x09;      /* SET_REPORT */
        USETW(req.ucr_request.wValue, 0x0200);
        USETW(req.ucr_request.wIndex, interface);
        USETW(req.ucr_request.wLength, sizeof(x360_led));
        req.ucr_data = x360_led;
        int r2 = ioctl(fd, USB_DO_REQUEST, &req);
        log_line("usb_ctrl: XInput wake-up sent (r1=%d, r2=%d)", r1, r2);
        return 0;
    }
#endif
    return 0;
}

uint8_t axis16_to_u8(int16_t val, int invert_y)
{
    if (val == 0) return 128;
    int v = ((int)val + 32768) >> 8;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return (uint8_t)(invert_y ? (255 - v) : v);
}

int usb_parse_stateful_report(usb_controller_type_t type, const uint8_t *data, size_t len,
                             pad_state_t *previous, pad_state_t *state)
{
    if (!data || !previous || !state || !len) return 0;
    if ((type==CTRL_XBOX_XINPUT || type==CTRL_8BITDO_ADAPTER) && data[0]==7 && len>=5) {
        *state=*previous; state->buttons &= ~PAD_BTN_PS;
        if (data[4]&3) state->buttons |= PAD_BTN_PS;
    } else {
        if (!usb_parse_input_report(type,data,len,state)) return 0;
        if ((type==CTRL_XBOX_XINPUT || type==CTRL_8BITDO_ADAPTER) && data[0]==0x20)
            state->buttons |= previous->buttons & PAD_BTN_PS;
    }
    *previous=*state; return 1;
}

int usb_parse_input_report(usb_controller_type_t type, const uint8_t *data, size_t len, pad_state_t *st)
{
    if (!data || len == 0 || !st) return 0;
    pad_state_neutral(st);

    switch (type) {
    case CTRL_PDP_FACEOFF: {
        /* HID descriptor: 14 buttons, two padding bits, 4-bit hat, four
         * unsigned 8-bit axes, then one padding byte. No report ID.
         * Some host HID APIs prepend a zero report ID; USB_FS does not. */
        size_t off = (len == 9 && data[0] == 0) ? 1 : 0;
        if (len < off + 7) return 0;
        uint16_t b = (uint16_t)data[off] | ((uint16_t)data[off + 1] << 8);
        if (b & (1u << 0)) st->buttons |= PAD_BTN_TRIANGLE; /* X */
        if (b & (1u << 1)) st->buttons |= PAD_BTN_CIRCLE;   /* A */
        if (b & (1u << 2)) st->buttons |= PAD_BTN_CROSS;    /* B */
        if (b & (1u << 3)) st->buttons |= PAD_BTN_SQUARE;   /* Y */
        if (b & (1u << 4)) st->buttons |= PAD_BTN_L1;
        if (b & (1u << 5)) st->buttons |= PAD_BTN_R1;
        if (b & (1u << 6)) { st->buttons |= PAD_BTN_L2; st->l2 = 255; }
        if (b & (1u << 7)) { st->buttons |= PAD_BTN_R2; st->r2 = 255; }
        if (b & (1u << 8)) st->buttons |= PAD_BTN_SHARE;
        if (b & (1u << 9)) st->buttons |= PAD_BTN_OPTIONS;
        if (b & (1u << 10)) st->buttons |= PAD_BTN_L3;
        if (b & (1u << 11)) st->buttons |= PAD_BTN_R3;
        if (b & (1u << 12)) st->buttons |= PAD_BTN_PS;
        if (b & (1u << 13)) st->buttons |= PAD_BTN_TOUCHPAD;
        switch (data[off + 2] & 0x0f) {
        case 0: st->buttons |= PAD_DPAD_UP; break;
        case 1: st->buttons |= PAD_DPAD_UP | PAD_DPAD_RIGHT; break;
        case 2: st->buttons |= PAD_DPAD_RIGHT; break;
        case 3: st->buttons |= PAD_DPAD_DOWN | PAD_DPAD_RIGHT; break;
        case 4: st->buttons |= PAD_DPAD_DOWN; break;
        case 5: st->buttons |= PAD_DPAD_DOWN | PAD_DPAD_LEFT; break;
        case 6: st->buttons |= PAD_DPAD_LEFT; break;
        case 7: st->buttons |= PAD_DPAD_UP | PAD_DPAD_LEFT; break;
        default: break;
        }
        st->lx = data[off + 3];
        st->ly = data[off + 4];
        st->rx = data[off + 5];
        st->ry = data[off + 6];
        return 1;
    }
    case CTRL_NINTENDO_SWITCH_PRO: {
        /* 64-byte or standard Switch Pro input report */
        if (len < 12 || data[0]!=0x30) return 0;
        uint8_t b1 = data[3];
        uint8_t b2 = data[4];
        uint8_t b3 = data[5];

        /* Face buttons */
        if (b1 & 0x01) st->buttons |= PAD_BTN_SQUARE;
        if (b1 & 0x02) st->buttons |= PAD_BTN_TRIANGLE;
        if (b1 & 0x04) st->buttons |= PAD_BTN_CROSS;
        if (b1 & 0x08) st->buttons |= PAD_BTN_CIRCLE;
        if (b1 & 0x40) st->buttons |= PAD_BTN_R1;
        if (b1 & 0x80) { st->buttons |= PAD_BTN_R2; st->r2 = 255; }

        if (b2 & 0x01) st->buttons |= PAD_BTN_SHARE;
        if (b2 & 0x02) st->buttons |= PAD_BTN_OPTIONS;
        if (b2 & 0x04) st->buttons |= PAD_BTN_R3;
        if (b2 & 0x08) st->buttons |= PAD_BTN_L3;
        if (b2 & 0x10) st->buttons |= PAD_BTN_PS;       /* Home -> PS */
        if (b2 & 0x20) st->buttons |= PAD_BTN_TOUCHPAD; /* Capture -> Touchpad */
        if (b3 & 0x40) st->buttons |= PAD_BTN_L1;
        if (b3 & 0x80) { st->buttons |= PAD_BTN_L2; st->l2 = 255; }

        /* D-Pad */
        if (b3 & 0x01) st->buttons |= PAD_DPAD_DOWN;
        if (b3 & 0x02) st->buttons |= PAD_DPAD_UP;
        if (b3 & 0x04) st->buttons |= PAD_DPAD_RIGHT;
        if (b3 & 0x08) st->buttons |= PAD_DPAD_LEFT;

        /* Analog Sticks (12-bit packed in Switch Pro) */
        uint16_t raw_lx = (uint16_t)(data[6] | ((data[7] & 0x0F) << 8));
        uint16_t raw_ly = (uint16_t)((data[7] >> 4) | (data[8] << 4));
        uint16_t raw_rx = (uint16_t)(data[9] | ((data[10] & 0x0F) << 8));
        uint16_t raw_ry = (uint16_t)((data[10] >> 4) | (data[11] << 4));

        uint8_t ly_u8 = (uint8_t)(raw_ly >> 4);
        uint8_t ry_u8 = (uint8_t)(raw_ry >> 4);
        st->lx = (uint8_t)(raw_lx >> 4);
        st->ly = (ly_u8 == 128) ? 128 : (uint8_t)(255 - ly_u8);
        st->rx = (uint8_t)(raw_rx >> 4);
        st->ry = (ry_u8 == 128) ? 128 : (uint8_t)(255 - ry_u8);
        return 1;
    }

    case CTRL_SONY_DS4: {
        /* Standard DS4 USB report: report ID 0x01, sticks at offsets 1..4 */
        int offset = (data[0] == 0x01) ? 1 : 0;
        if (len < (size_t)(offset + 9)) return 0;

        st->lx = data[offset + 0];
        st->ly = data[offset + 1];
        st->rx = data[offset + 2];
        st->ry = data[offset + 3];

        uint8_t dpad = data[offset + 4] & 0x0F;
        switch (dpad) {
        case 0: st->buttons |= PAD_DPAD_UP; break;
        case 1: st->buttons |= PAD_DPAD_UP | PAD_DPAD_RIGHT; break;
        case 2: st->buttons |= PAD_DPAD_RIGHT; break;
        case 3: st->buttons |= PAD_DPAD_DOWN | PAD_DPAD_RIGHT; break;
        case 4: st->buttons |= PAD_DPAD_DOWN; break;
        case 5: st->buttons |= PAD_DPAD_DOWN | PAD_DPAD_LEFT; break;
        case 6: st->buttons |= PAD_DPAD_LEFT; break;
        case 7: st->buttons |= PAD_DPAD_UP | PAD_DPAD_LEFT; break;
        default: break;
        }

        uint8_t b1 = data[offset + 4];
        if (b1 & 0x10) st->buttons |= PAD_BTN_SQUARE;
        if (b1 & 0x20) st->buttons |= PAD_BTN_CROSS;
        if (b1 & 0x40) st->buttons |= PAD_BTN_CIRCLE;
        if (b1 & 0x80) st->buttons |= PAD_BTN_TRIANGLE;

        uint8_t b2 = data[offset + 5];
        if (b2 & 0x01) st->buttons |= PAD_BTN_L1;
        if (b2 & 0x02) st->buttons |= PAD_BTN_R1;
        if (b2 & 0x04) st->buttons |= PAD_BTN_L2;
        if (b2 & 0x08) st->buttons |= PAD_BTN_R2;
        if (b2 & 0x10) st->buttons |= PAD_BTN_SHARE;
        if (b2 & 0x20) st->buttons |= PAD_BTN_OPTIONS;
        if (b2 & 0x40) st->buttons |= PAD_BTN_L3;
        if (b2 & 0x80) st->buttons |= PAD_BTN_R3;

        uint8_t b3 = data[offset + 6];
        if (b3 & 0x01) st->buttons |= PAD_BTN_PS;
        if (b3 & 0x02) st->buttons |= PAD_BTN_TOUCHPAD;

        st->l2 = data[offset + 7];
        st->r2 = data[offset + 8];
        return 1;
    }

    case CTRL_XBOX_XINPUT:
    case CTRL_8BITDO_ADAPTER: {
        /* 1. Xbox One GIP Controller Input Report */
        if (data[0] == 0x20 && len >= 18) {
            uint16_t btn = (uint16_t)(data[4] | ((uint16_t)data[5] << 8));
            btn &= (uint16_t)~0x0002u; /* Bit 2 is GIP keep-alive flag */

            if (btn & 0x0004) st->buttons |= PAD_BTN_OPTIONS;  /* Menu / Start */
            if (btn & 0x0008) st->buttons |= PAD_BTN_SHARE;    /* View / Back */
            if (btn & 0x0010) st->buttons |= PAD_BTN_CROSS;    /* A -> Cross */
            if (btn & 0x0020) st->buttons |= PAD_BTN_CIRCLE;   /* B -> Circle */
            if (btn & 0x0040) st->buttons |= PAD_BTN_SQUARE;   /* X -> Square */
            if (btn & 0x0080) st->buttons |= PAD_BTN_TRIANGLE; /* Y -> Triangle */
            if (btn & 0x0100) st->buttons |= PAD_DPAD_UP;
            if (btn & 0x0200) st->buttons |= PAD_DPAD_DOWN;
            if (btn & 0x0400) st->buttons |= PAD_DPAD_LEFT;
            if (btn & 0x0800) st->buttons |= PAD_DPAD_RIGHT;
            if (btn & 0x1000) st->buttons |= PAD_BTN_L1;
            if (btn & 0x2000) st->buttons |= PAD_BTN_R1;
            if (btn & 0x4000) st->buttons |= PAD_BTN_L3;
            if (btn & 0x8000) st->buttons |= PAD_BTN_R3;

            /* 16-bit analog triggers (0-1023) */
            uint16_t trig_l = (uint16_t)(data[6] | ((uint16_t)data[7] << 8));
            uint16_t trig_r = (uint16_t)(data[8] | ((uint16_t)data[9] << 8));
            st->l2 = (uint8_t)(trig_l > 1023 ? 255 : (trig_l >> 2));
            st->r2 = (uint8_t)(trig_r > 1023 ? 255 : (trig_r >> 2));
            if (st->l2 >= PAD_TRIGGER_DIGITAL_THRESHOLD) st->buttons |= PAD_BTN_L2;
            if (st->r2 >= PAD_TRIGGER_DIGITAL_THRESHOLD) st->buttons |= PAD_BTN_R2;

            /* 16-bit signed joysticks -> 8-bit unsigned coordinates */
            int16_t sx = (int16_t)(data[10] | ((uint16_t)data[11] << 8));
            int16_t sy = (int16_t)(data[12] | ((uint16_t)data[13] << 8));
            int16_t rx = (int16_t)(data[14] | ((uint16_t)data[15] << 8));
            int16_t ry = (int16_t)(data[16] | ((uint16_t)data[17] << 8));

            st->lx = axis16_to_u8(sx, 0);
            st->ly = axis16_to_u8(sy, 1);
            st->rx = axis16_to_u8(rx, 0);
            st->ry = axis16_to_u8(ry, 1);
            return 1;
        }

        /* 2. Xbox One GIP Guide (PS) Button Packet */
        if (data[0] == 0x07 && len >= 5) {
            if ((len >= 6 && data[4] == 0x5b && (data[5] & 0x01)) || (data[4] & 0x03)) {
                st->buttons |= PAD_BTN_PS;
                return 1;
            }
            return 0;
        }

        /* 3. Standard Wired Xbox 360 / Wireless Receiver / Generic XInput */
        int off = -1;
        if (len >= 14 && data[0] == 0x00) {
            /* Any Xbox 360 packet starting with 0x00 has button data at offset 2.
             * Byte 1 is packet length (0x14 for official, but clones may report 0x00 or variable). */
            off = 2;
        } else if (len >= 18 && (data[1] & 0x01) && (data[0] == 0x00 || (data[0] & 0x08))) {
            off = 4; /* Wireless receiver */
        }

        if (off < 0 || len < (size_t)(off + 12)) {
            return 0; /* Unknown packets are never guessed as another protocol. */
        }

        uint16_t btn = (uint16_t)(data[off + 0] | ((uint16_t)data[off + 1] << 8));

        if (btn & 0x0001) st->buttons |= PAD_DPAD_UP;
        if (btn & 0x0002) st->buttons |= PAD_DPAD_DOWN;
        if (btn & 0x0004) st->buttons |= PAD_DPAD_LEFT;
        if (btn & 0x0008) st->buttons |= PAD_DPAD_RIGHT;
        if (btn & 0x0010) st->buttons |= PAD_BTN_OPTIONS; /* Start -> Options */
        if (btn & 0x0020) st->buttons |= PAD_BTN_SHARE;   /* Back -> Share */
        if (btn & 0x0040) st->buttons |= PAD_BTN_L3;
        if (btn & 0x0080) st->buttons |= PAD_BTN_R3;
        if (btn & 0x0100) st->buttons |= PAD_BTN_L1;
        if (btn & 0x0200) st->buttons |= PAD_BTN_R1;
        if (btn & 0x0400) st->buttons |= PAD_BTN_PS;       /* Guide -> PS */
        if (btn & 0x1000) st->buttons |= PAD_BTN_CROSS;    /* A -> Cross */
        if (btn & 0x2000) st->buttons |= PAD_BTN_CIRCLE;   /* B -> Circle */
        if (btn & 0x4000) st->buttons |= PAD_BTN_SQUARE;   /* X -> Square */
        if (btn & 0x8000) st->buttons |= PAD_BTN_TRIANGLE; /* Y -> Triangle */

        /* Triggers: present at off+2 and off+3 */
        st->l2 = data[off + 2];
        st->r2 = data[off + 3];
        if (st->l2 >= PAD_TRIGGER_DIGITAL_THRESHOLD) st->buttons |= PAD_BTN_L2;
        if (st->r2 >= PAD_TRIGGER_DIGITAL_THRESHOLD) st->buttons |= PAD_BTN_R2;

        /* 16-bit signed sticks -> 8-bit unsigned */
        int16_t sx = (int16_t)(data[off + 4] | ((uint16_t)data[off + 5] << 8));
        int16_t sy = (int16_t)(data[off + 6] | ((uint16_t)data[off + 7] << 8));
        int16_t rx = (int16_t)(data[off + 8] | ((uint16_t)data[off + 9] << 8));
        int16_t ry = (int16_t)(data[off + 10] | ((uint16_t)data[off + 11] << 8));

        st->lx = axis16_to_u8(sx, 0);
        st->ly = axis16_to_u8(sy, 1);
        st->rx = axis16_to_u8(rx, 0);
        st->ry = axis16_to_u8(ry, 1);
        return 1;
    }

    case CTRL_SONY_DS3: {
        /* DualShock 3 standard HID report */
        if (len < 10) return 0;
        uint8_t b1 = data[2];
        uint8_t b2 = data[3];

        if (b1 & 0x01) st->buttons |= PAD_BTN_SHARE;   /* Select */
        if (b1 & 0x02) st->buttons |= PAD_BTN_L3;
        if (b1 & 0x04) st->buttons |= PAD_BTN_R3;
        if (b1 & 0x08) st->buttons |= PAD_BTN_OPTIONS; /* Start */
        if (b1 & 0x10) st->buttons |= PAD_DPAD_UP;
        if (b1 & 0x20) st->buttons |= PAD_DPAD_RIGHT;
        if (b1 & 0x40) st->buttons |= PAD_DPAD_DOWN;
        if (b1 & 0x80) st->buttons |= PAD_DPAD_LEFT;

        if (b2 & 0x01) { st->buttons |= PAD_BTN_L2; st->l2 = 255; }
        if (b2 & 0x02) { st->buttons |= PAD_BTN_R2; st->r2 = 255; }
        if (b2 & 0x04) st->buttons |= PAD_BTN_L1;
        if (b2 & 0x08) st->buttons |= PAD_BTN_R1;
        if (b2 & 0x10) st->buttons |= PAD_BTN_TRIANGLE;
        if (b2 & 0x20) st->buttons |= PAD_BTN_CIRCLE;
        if (b2 & 0x40) st->buttons |= PAD_BTN_CROSS;
        if (b2 & 0x80) st->buttons |= PAD_BTN_SQUARE;

        if (len > 4 && (data[4] & 0x01)) st->buttons |= PAD_BTN_PS;

        if (len >= 10) {
            st->lx = data[6];
            st->ly = data[7];
            st->rx = data[8];
            st->ry = data[9];
        }
        return 1;
    }

    default: {
        /* Robust generic HID gamepad fallback */
        int off = (len >= 7 && data[0] <= 0x04) ? 1 : 0;
        if (len >= (size_t)(off + 5)) {
            st->lx = data[off + 0];
            st->ly = data[off + 1];
            st->rx = (len >= (size_t)(off + 4)) ? data[off + 2] : 128;
            st->ry = (len >= (size_t)(off + 4)) ? data[off + 3] : 128;

            uint8_t hat = data[off + 4] & 0x0F;
            switch (hat) {
            case 0: st->buttons |= PAD_DPAD_UP; break;
            case 1: st->buttons |= PAD_DPAD_UP | PAD_DPAD_RIGHT; break;
            case 2: st->buttons |= PAD_DPAD_RIGHT; break;
            case 3: st->buttons |= PAD_DPAD_DOWN | PAD_DPAD_RIGHT; break;
            case 4: st->buttons |= PAD_DPAD_DOWN; break;
            case 5: st->buttons |= PAD_DPAD_DOWN | PAD_DPAD_LEFT; break;
            case 6: st->buttons |= PAD_DPAD_LEFT; break;
            case 7: st->buttons |= PAD_DPAD_UP | PAD_DPAD_LEFT; break;
            default: break;
            }

            if (len >= (size_t)(off + 6)) {
                uint16_t b = (uint16_t)(data[off + 4] >> 4) | ((uint16_t)data[off + 5] << 4);
                if (len >= (size_t)(off + 7)) {
                    b |= ((uint16_t)data[off + 6] << 12);
                }
                if (b & 0x01) st->buttons |= PAD_BTN_CROSS;
                if (b & 0x02) st->buttons |= PAD_BTN_CIRCLE;
                if (b & 0x04) st->buttons |= PAD_BTN_SQUARE;
                if (b & 0x08) st->buttons |= PAD_BTN_TRIANGLE;
                if (b & 0x10) st->buttons |= PAD_BTN_L1;
                if (b & 0x20) st->buttons |= PAD_BTN_R1;
                if (b & 0x40) { st->buttons |= PAD_BTN_L2; st->l2 = 255; }
                if (b & 0x80) { st->buttons |= PAD_BTN_R2; st->r2 = 255; }
                if (b & 0x0100) st->buttons |= PAD_BTN_SHARE;   /* Select / Back */
                if (b & 0x0200) st->buttons |= PAD_BTN_OPTIONS; /* Start */
                if (b & 0x0400) st->buttons |= PAD_BTN_L3;
                if (b & 0x0800) st->buttons |= PAD_BTN_R3;
                if (b & 0x1000) st->buttons |= PAD_BTN_PS;      /* Home / Guide */
            }
            return 1;
        }
        break;
    }
        break;
    }
    return 0;
}
