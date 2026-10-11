// Adapter for the standalone test payload only. Exclude this file when linking
// liblibrepad_xbox.a into the main project: it keeps the public module API separate.
#include "../librepad_xbox.h"
extern "C" {
#include "input/usb_device.h"
}
extern "C" int usb_hotplug_init() { return librepad_xbox_start(); }
extern "C" void usb_hotplug_poll(long now) { librepad_xbox_poll(now); }
extern "C" int usb_hotplug_freepad_connected() { return librepad_xbox_is_connected(); }
extern "C" void usb_hotplug_cleanup() { librepad_xbox_stop(); }
