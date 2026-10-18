#include "controller.h"
#include <cstdio>

static uint8_t stick(int16_t value, bool invert = false) {
    if (!value) return 128;
    const int axis = (static_cast<int>(value) + 32768) >> 8;
    return static_cast<uint8_t>(invert ? 255 - axis : axis);
}

void Controller::deviceAnnounced(uint8_t id, const AnnounceData *data) {
    if (connected || id != 0) return;
    LedModeData led = {};
    led.mode = LED_ON;
    led.brightness = 0x14;
    if (!setPowerMode(0, POWER_ON) || !setLedMode(led)) return;
    state.vid = data->vendorId;
    state.pid = data->productId;
    state.connected = 1;
    state.conn_type = CONN_USB_DONGLE_24G;
    snprintf(state.controller_name, sizeof(state.controller_name), "Xbox One Wireless / LibrePadPS5");
    connected = xbox_sink_connect(this, state.vid, state.pid) != 0;
    if (connected && onConnect) onConnect();
}

void Controller::statusReceived(uint8_t, const StatusData *data) {
    static const uint8_t levels[] = {0, 25, 60, 100};
    state.battery_level = levels[data->batteryLevel];
    state.battery_charging = data->batteryType == BATT_TYPE_CHARGING;
}

void Controller::guideButtonPressed(const GuideButtonData *data) {
    if (data->pressed) state.buttons |= PAD_BTN_PS;
    else state.buttons &= ~PAD_BTN_PS;
    if (connected) xbox_sink_input(this, &state);
}

void Controller::inputReceived(const InputData *data) {
    static_assert(sizeof(InputData) == 14, "GIP wire layout must use packed GCC bitfields");
    state.buttons &= PAD_BTN_PS; // Guide arrives in separate command 0x07.
    if (data->buttons.start) state.buttons |= PAD_BTN_OPTIONS;
    if (data->buttons.select) state.buttons |= PAD_BTN_CREATE;
    if (data->buttons.a) state.buttons |= PAD_BTN_CROSS;
    if (data->buttons.b) state.buttons |= PAD_BTN_CIRCLE;
    if (data->buttons.x) state.buttons |= PAD_BTN_SQUARE;
    if (data->buttons.y) state.buttons |= PAD_BTN_TRIANGLE;
    if (data->buttons.bumperLeft) state.buttons |= PAD_BTN_L1;
    if (data->buttons.bumperRight) state.buttons |= PAD_BTN_R1;
    if (data->buttons.stickLeft) state.buttons |= PAD_BTN_L3;
    if (data->buttons.stickRight) state.buttons |= PAD_BTN_R3;
    if (data->buttons.dpadUp) state.buttons |= PAD_DPAD_UP;
    if (data->buttons.dpadDown) state.buttons |= PAD_DPAD_DOWN;
    if (data->buttons.dpadLeft) state.buttons |= PAD_DPAD_LEFT;
    if (data->buttons.dpadRight) state.buttons |= PAD_DPAD_RIGHT;
    state.lx = stick(data->stickLeftX);
    state.ly = stick(data->stickLeftY, true);
    state.rx = stick(data->stickRightX);
    state.ry = stick(data->stickRightY, true);
    state.l2 = static_cast<uint8_t>((data->triggerLeft > 1023 ? 1023 : data->triggerLeft) * 255u / 1023u);
    state.r2 = static_cast<uint8_t>((data->triggerRight > 1023 ? 1023 : data->triggerRight) * 255u / 1023u);
    if (state.l2 >= PAD_TRIGGER_DIGITAL_THRESHOLD) state.buttons |= PAD_BTN_L2;
    if (state.r2 >= PAD_TRIGGER_DIGITAL_THRESHOLD) state.buttons |= PAD_BTN_R2;
    if (connected) xbox_sink_input(this, &state);
}
