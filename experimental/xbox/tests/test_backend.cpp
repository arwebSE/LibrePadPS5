#include "../port/sink.h"
extern "C" {
#include "../librepad_xbox.h"
#include "ps5_vpad.h"
}
#include <cassert>
#include <cstring>
#include <cstdio>
static bool firmware = true, paused = false;
static int adds, updates, removes;
static pad_state_t received;
extern "C" int xbox_test_access(const char *path, int) {
    if (!strcmp(path, "test-firmware")) return firmware ? 0 : -1;
    assert(strstr(path, "/pause")); return paused ? 0 : -1;
}
extern "C" void log_line(const char *, ...) {}
extern "C" int vpad_add(int slot, pad_conn_type_t type, const char *name) {
    assert(slot == 0 && type == CONN_USB_DONGLE_24G && strstr(name, "LibrePadPS5"));
    ++adds; return 1;
}
extern "C" void vpad_remove(int slot) { assert(slot == 0); ++removes; }
extern "C" void vpad_update(int slot, const pad_state_t *state) { assert(slot == 0); ++updates; received = *state; }
int main() {
    int controller, other;
    firmware = false;
    assert(!librepad_xbox_start());
    assert(!librepad_xbox_is_connected() && adds == 0);
    librepad_xbox_stop();
    firmware = true;
    assert(librepad_xbox_start());
    assert(librepad_xbox_start()); // repeated init must not replace a joinable thread
    assert(xbox_sink_connect(&controller, 0x045e, 0x02dd));
    assert(librepad_xbox_is_connected());
    assert(!xbox_sink_connect(&other, 0x045e, 0x02dd));
    pad_state_t state;
    pad_state_neutral(&state); state.buttons = PAD_BTN_CROSS;
    xbox_sink_input(&controller, &state);
    assert(updates == 0 && adds == 0); // no input injected before attach
    librepad_xbox_poll(0);
    assert(adds == 1 && updates == 1 && received.buttons == PAD_BTN_CROSS);
    xbox_sink_input(&other, &state);
    assert(updates == 1);
    xbox_sink_disconnect(&other);
    assert(librepad_xbox_is_connected());
    // The dashboard's Release persists pause; later input must not re-arm.
    paused = true; vpad_remove(0);
    xbox_sink_input(&controller, &state);
    librepad_xbox_poll(1);
    assert(adds == 1);
    xbox_sink_disconnect(&controller);
    assert(!librepad_xbox_is_connected());
    int before = removes;
    librepad_xbox_poll(2);
    assert(removes == before + 1);
    assert(xbox_sink_connect(&controller, 0x045e, 0x02dd));
    librepad_xbox_poll(3);
    assert(adds == 1); // re-pairing also respects persisted pause
    paused = false; // explicit dashboard Reconnect
    assert(vpad_add(0, CONN_USB_DONGLE_24G, "Xbox / LibrePadPS5"));
    xbox_sink_input(&controller, &state);
    assert(adds == 2 && received.buttons == PAD_BTN_CROSS);
    librepad_xbox_stop();
    assert(!librepad_xbox_is_connected() && xbox_transport_stopping());
    puts("Xbox backend: missing firmware / single controller / release / disconnect / reconnect / shutdown tests passed");
}
