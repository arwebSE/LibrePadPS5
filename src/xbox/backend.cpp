#include "sink.h"
#include "xbox/librepad_xbox.h"
#ifndef XBOX_BACKEND_TEST
#include "xow/dongle/dongle.h"
#endif
extern "C" {
#include "ps5/virtual_controller.h"
#include "input/controller_router.h"
#include "core/legacy_paths.h"
#include "core/logging.h"
}
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <unistd.h>
#ifdef XBOX_BACKEND_TEST
extern "C" int xbox_test_access(const char *path, int mode);
#define access xbox_test_access
#endif

static std::atomic<bool> stopping(false), failed(false), connected(false);
static std::atomic<bool> reloadRequested(false);
static std::thread worker;
static std::mutex stateMutex;
static void *owner = nullptr;
static void *detachedOwner = nullptr;
static bool attachPending = false, detachPending = false;
static pad_state_t latest;

extern "C" int xbox_transport_stopping() { return stopping || failed; }
extern "C" int xbox_sink_connect(void *controller, uint16_t vid, uint16_t pid) {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (owner) { log_line("xbox: additional controller ignored; radio backend supports one Xbox pad"); return 0; }
    owner = controller;
    pad_state_neutral(&latest);
    latest.vid = vid;
    latest.pid = pid;
    attachPending = true;
    connected = true;
    log_line("LibrePadPS5 Xbox: controller announced %04x:%04x", vid, pid);
    return 1;
}
extern "C" void xbox_sink_input(void *controller, const pad_state_t *state) {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (owner == controller && !stopping) {
        latest = *state;
        // Feed every packet, including Guide edges, through the existing
        // protected-profile lifecycle. Release remains effective until reconnect.
        if (!attachPending) controller_update(owner, &latest);
    }
}
extern "C" void xbox_sink_disconnect(void *controller) {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (owner != controller) return;
    detachedOwner = owner;
    owner = nullptr;
    connected = false;
    detachPending = true;
    attachPending = false;
    log_line("LibrePadPS5 Xbox: controller disconnected");
}

static void runRadio() {
#ifndef XBOX_BACKEND_TEST
    while (!stopping) {
        failed = false;
        try {
            UsbDeviceManager manager;
            auto usb = manager.getDevice({{DONGLE_VID, DONGLE_PID_OLD}, {DONGLE_VID, DONGLE_PID_NEW}}, [] { failed = true; });
            if (!usb) break;
            log_line("LibrePadPS5 Xbox: radio endpoints opened; initializing MT76 firmware");
            {
                Dongle dongle(std::move(usb));
                while (!stopping && !failed) std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        } catch (const std::exception &error) {
            log_line("LibrePadPS5 Xbox: %s", error.what());
        }
        for (unsigned i = 0; i < 200 && !stopping; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
#endif
}
extern "C" int librepad_xbox_start() {
    if (worker.joinable()) return 1;
    stopping = false;
    failed = false;
    pad_state_neutral(&latest);
    // Missing firmware must not cause any vendor writes to the radio.
    if (access(FIRMWARE, R_OK) != 0) {
        log_line("LibrePadPS5 Xbox: firmware missing: %s; radio disabled", FIRMWARE);
        return 0;
    }
    worker = std::thread(runRadio);
    return 1;
}
extern "C" int librepad_xbox_is_connected() { return connected; }
extern "C" void librepad_xbox_reload() { reloadRequested = true; }
extern "C" void librepad_xbox_poll(long) {
    if (reloadRequested.exchange(false)) {
        librepad_xbox_stop();
        librepad_xbox_start();
    }
    std::lock_guard<std::mutex> lock(stateMutex);
    if (detachPending) { controller_detach(detachedOwner); detachedOwner = nullptr; detachPending = false; }
    if (attachPending) {
        if (controller_attach(owner, CONN_USB_DONGLE_24G, "Xbox One Wireless") >= 0) {
            controller_update(owner, &latest);
            attachPending = false;
        }
    }
}
extern "C" void librepad_xbox_stop() {
    stopping = true;
    if (worker.joinable()) worker.join();
    std::lock_guard<std::mutex> lock(stateMutex);
    controller_detach(owner);
    controller_detach(detachedOwner);
    owner = detachedOwner = nullptr;
    connected = false;
    attachPending = detachPending = false;
}
