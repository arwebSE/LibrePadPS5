#include "xbox/usb.h"
#include <atomic>
#include <cassert>
#include <cstring>
#include <thread>
#include <vector>
#include <cstdio>

static usb_fs_endpoint *eps;
static std::vector<unsigned> pending;
static std::atomic<bool> stopped(false);
static std::atomic<int> failures(0);
static unsigned threshold = 3, started, finished, closed, uninitialized;
static bool timeoutRead, shortWrite, failOpen;
extern "C" void log_line(const char *, ...) {}
extern "C" int xbox_transport_stopping() { return stopped; }
int xbox_test_open(const char *, int) { assert(false); return -1; }
int xbox_test_close(int fd) { assert(fd == 123); ++closed; return 0; }
int xbox_test_ioctl(int fd, unsigned long request, void *arg) {
    assert(fd == 123);
    switch (request) {
    case USB_DEVICEENUMERATE: case USB_SET_CONFIG:
        assert(*static_cast<int *>(arg) == 0); return 0;
    case USB_GET_CONFIG: *static_cast<int *>(arg) = 0; return 0;
    case USB_CLAIM_INTERFACE: case USB_RELEASE_INTERFACE: return 0;
    case USB_FS_INIT: {
        auto init = static_cast<usb_fs_init *>(arg);
        assert(init->ep_index_max == 3); eps = init->pEndpoints; return 0;
    }
    case USB_FS_OPEN: {
        auto open = static_cast<usb_fs_open *>(arg);
        const uint8_t expected[] = {0x85, 0x84, 4};
        assert(open->ep_no == expected[open->ep_index]);
        assert(open->max_bufsize >= 0x3808); // full firmware chunk plus header/trailer
        return failOpen ? -1 : 0;
    }
    case USB_FS_UNINIT: ++uninitialized; return 0;
    case USB_DO_REQUEST: {
        auto req = static_cast<usb_ctl_request *>(arg);
        assert(req->ucr_request.bmRequestType == 0xc0);
        assert(UGETW(req->ucr_request.wLength) == 4);
        const uint8_t value[] = {1, 2, 3, 4}; memcpy(req->ucr_data, value, 4);
        req->ucr_actlen = 4; return 0;
    }
    case USB_FS_START: {
        unsigned index = static_cast<usb_fs_start *>(arg)->ep_index;
        pending.push_back(index); ++started; return 0;
    }
    case USB_FS_COMPLETE: {
        if (pending.empty() || started < threshold) return -1;
        // Deliberately complete OUT before both IN requests. Any thread may
        // consume any completion; the real transport must route all three.
        unsigned index = pending.back(); pending.pop_back();
        auto &ep = eps[index];
        if (index < 2) {
            const uint8_t value[] = {static_cast<uint8_t>(index == 0 ? 0x85 : 0x84), 0xaa, 0xbb};
            memcpy(*ep.ppBuffer, value, 3); *ep.pLength = 3;
        } else {
            assert(*ep.pLength >= 3);
            assert(static_cast<uint8_t *>(*ep.ppBuffer)[0] == 0x04);
            if (shortWrite) --*ep.pLength;
        }
        ep.status = timeoutRead && index == 0 ? USB_ERR_TIMEOUT : USB_ERR_NORMAL_COMPLETION;
        static_cast<usb_fs_complete *>(arg)->ep_index = index;
        ++finished; return 0;
    }
    case USB_FS_STOP: return 0;
    default: assert(false); return -1;
    }
}

int main() {
    {
        UsbDevice device(123, [] { ++failures; });
        uint8_t registerValue[4] = {};
        device.controlTransfer({7, 0, 0, registerValue, 4}, false);
        assert(registerValue[0] == 1 && registerValue[3] == 4);
        FixedBytes<USB_MAX_BULK_TRANSFER_SIZE> mcu, wlan;
        int mcuSize = -2, wlanSize = -2;
        std::thread a([&] { mcuSize = device.bulkRead(5, mcu); });
        std::thread b([&] { wlanSize = device.bulkRead(4, wlan); });
        Bytes firmware(0x3808); firmware[0] = 4;
        assert(device.bulkWrite(4, firmware));
        a.join(); b.join();
        assert(started == 3 && finished == 3 && pending.empty());
        assert(mcuSize == 3 && wlanSize == 3);
        assert(mcu.raw()[0] == 0x85 && wlan.raw()[0] == 0x84);
        threshold = 1; timeoutRead = true;
        assert(device.bulkRead(5, mcu) == 0);
        timeoutRead = false;
        assert(device.bulkRead(5, mcu) == 3); // timeout consumed before restarting
        shortWrite = true;
        assert(!device.bulkWrite(4, firmware));
        assert(!device.bulkWrite(3, firmware));
        assert(device.bulkRead(3, mcu) == -1);
        stopped = true;
        unsigned before = started;
        assert(device.bulkRead(5, mcu) == -1 && started == before);
        device.prepareShutdown(); shortWrite = false;
        assert(device.bulkWrite(4, firmware)); // final radio-off is allowed after read cancellation
    }
    assert(failures == 0 && closed == 1 && uninitialized == 1);
    failOpen = true;
    try { UsbDevice device(123, [] { ++failures; }); assert(false); }
    catch (const UsbException &) {}
    assert(closed == 2 && uninitialized == 2);
    puts("Xbox USB transport: concurrent endpoint dispatch / firmware chunk / timeout / cleanup tests passed");
}
