#include "xbox/usb.h"
#include <cassert>
#include <cstdio>
#include <cstring>

static unsigned product, descriptors, claimed, mutations, closed;
static bool stopped;
extern "C" void log_line(const char *, ...) {}
extern "C" int xbox_transport_stopping() { return stopped; }
int xbox_test_open(const char *path, int) {
    if (!std::strcmp(path, "/dev/ugen0.1")) return 1; // native DualSense
    if (!std::strcmp(path, "/dev/ugen0.2")) return 2; // wired PDP
    if (!std::strcmp(path, "/dev/ugen0.3")) return 3; // Microsoft, unsupported PID
    if (!std::strcmp(path, "/dev/ugen0.4")) return 4; // supported adapter, when present
    return -1;
}
int xbox_test_close(int) { ++closed; return 0; }
int xbox_test_ioctl(int fd, unsigned long request, void *arg) {
    if (request == USB_GET_DEVICE_DESC) {
        ++descriptors;
        auto desc = static_cast<usb_device_descriptor *>(arg);
        USETW(desc->idVendor, fd == 1 ? 0x054c : fd == 2 ? 0x0e6f : 0x045e);
        USETW(desc->idProduct, fd == 1 ? 0x0ce6 : fd == 2 ? 0x0184 : fd == 3 ? 0x02dd : product);
        if (fd == 4 && !product) stopped = true;
        return 0;
    }
    // No resets, claims, endpoint opens or vendor writes on unrelated pads.
    assert(fd == 4 && (product == 0x02e6 || product == 0x02fe));
    ++mutations;
    if (request == USB_CLAIM_INTERFACE) ++claimed;
    return 0;
}
int main() {
    UsbDeviceManager manager;
    auto find = [&] { return manager.getDevice({{0x045e, 0x02e6}, {0x045e, 0x02fe}}, [] {}); };
    assert(!find());
    assert(descriptors == 4 && closed == 4 && mutations == 0);
    for (unsigned pid : {0x02e6, 0x02fe}) {
        stopped = false; product = pid;
        auto adapter = find();
        assert(adapter && claimed > 0);
    }
    assert(claimed == 2);
    puts("Xbox discovery: DualSense / PDP / unsupported Microsoft devices untouched; both adapter IDs accepted");
}
