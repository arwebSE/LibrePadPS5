#include "usb.h"
#include "sink.h"
extern "C" {
#include "core/logging.h"
}
#ifndef XBOX_USB_TEST
#include <dev/usb/usb_endian.h>
#include <dev/usb/usbdi.h>
#include <sys/ioctl.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <thread>
#ifdef XBOX_USB_TEST
#define open xbox_test_open
#define ioctl xbox_test_ioctl
#define close xbox_test_close
#ifndef O_NONBLOCK
#define O_NONBLOCK 0
#endif
#endif

UsbDevice::UsbDevice(int descriptor, Terminate onTerminate) : fd(descriptor), terminate(onTerminate) {
    try {
        // FreeBSD's USB_DEVICEENUMERATE only schedules a bus reset. Opening
        // USB_FS immediately afterwards races that reset and loses endpoints.
        // The MT76 firmware path already restarts a resident radio; keep its
        // existing USB configuration intact across payload/service restarts.
        int config = -1;
        if (ioctl(fd, USB_GET_CONFIG, &config)) throw UsbException("USB_GET_CONFIG failed");
        // FreeBSD uses a configuration INDEX, so index zero is USB configuration 1.
        if (config != 0) {
            config = 0;
            if (ioctl(fd, USB_SET_CONFIG, &config)) throw UsbException("USB_SET_CONFIG failed");
        }
        int iface = 0;
        if (ioctl(fd, USB_CLAIM_INTERFACE, &iface)) throw UsbException("USB_CLAIM_INTERFACE failed");
        claimed = true;
        usb_fs_init init = {};
        init.pEndpoints = endpoints;
        init.ep_index_max = 3;
        if (ioctl(fd, USB_FS_INIT, &init)) throw UsbException("USB_FS_INIT failed");
        initialized = true;
        const uint8_t addresses[] = {0x85, 0x84, 0x04};
        for (unsigned i = 0; i < 3; ++i) {
            usb_fs_open open = {};
            open.max_bufsize = capacity;
            open.max_frames = 1;
            open.ep_index = i;
            open.ep_no = addresses[i];
            if (ioctl(fd, USB_FS_OPEN, &open)) throw UsbException("USB_FS_OPEN failed for radio endpoint " + std::to_string(addresses[i]));
            frames[i] = buffers[i].data();
            endpoints[i].ppBuffer = &frames[i];
            endpoints[i].pLength = &lengths[i];
            endpoints[i].nFrames = 1;
            endpoints[i].flags = USB_FS_FLAG_SINGLE_SHORT_OK;
            endpoints[i].timeout = 1000;
        }
    } catch (...) { closeDevice(); throw; }
}

void UsbDevice::closeDevice() {
    if (fd < 0) return;
    if (initialized) { usb_fs_uninit uninit = {}; ioctl(fd, USB_FS_UNINIT, &uninit); initialized = false; }
    int iface = 0;
    if (claimed) { ioctl(fd, USB_RELEASE_INTERFACE, &iface); claimed = false; }
    close(fd);
    fd = -1;
}
UsbDevice::~UsbDevice() { closeDevice(); }

void UsbDevice::controlTransfer(ControlPacket packet, bool write) {
    std::lock_guard<std::mutex> lock(ioMutex);
    if (xbox_transport_stopping() && !shutdownWrites) { if (!write && packet.data) memset(packet.data, 0, packet.length); return; }
    usb_ctl_request req = {};
    req.ucr_request.bmRequestType = write ? 0x40 : 0xc0;
    req.ucr_request.bRequest = packet.request;
    USETW(req.ucr_request.wValue, packet.value);
    USETW(req.ucr_request.wIndex, packet.index);
    USETW(req.ucr_request.wLength, packet.length);
    req.ucr_data = packet.data;
    if (ioctl(fd, USB_DO_REQUEST, &req) || req.ucr_actlen != packet.length) {
        log_line("xbox USB: control %s request=%02x value=%04x index=%04x length=%u actual=%u errno=%d",
                 write ? "OUT" : "IN", packet.request, packet.value, packet.index,
                 packet.length, req.ucr_actlen, errno);
        terminate();
        if (!write && packet.data) memset(packet.data, 0, packet.length);
    }
}

int UsbDevice::transfer(unsigned index, uint8_t *data, size_t length, bool write) {
    const bool shutdownOutput = write && shutdownWrites;
    if (!data || length > capacity || (xbox_transport_stopping() && !shutdownOutput)) return -1;
    std::lock_guard<std::mutex> ownEndpoint(endpointMutex[index]);
    {
        std::lock_guard<std::mutex> lock(ioMutex);
        if (write) memcpy(buffers[index].data(), data, length);
        lengths[index] = static_cast<uint32_t>(length);
        complete[index] = false;
        usb_fs_start start = {};
        start.ep_index = index;
        if (ioctl(fd, USB_FS_START, &start)) {
            log_line("xbox USB: start endpoint index=%u length=%zu errno=%d", index, length, errno);
            terminate(); return -1;
        }
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (true) {
        {
            std::lock_guard<std::mutex> lock(ioMutex);
            // USB_FS_COMPLETE is device-wide: dispatch by index, never confuse
            // WLAN input, MCU input and firmware/output completions.
            usb_fs_complete done = {};
            while (ioctl(fd, USB_FS_COMPLETE, &done) == 0) {
                if (done.ep_index >= 3) { terminate(); return -1; }
                complete[done.ep_index] = true;
            }
            if (complete[index]) {
                int status = endpoints[index].status;
                if (status == USB_ERR_TIMEOUT) {
                    if (write) { log_line("xbox USB: OUT timed out length=%zu", length); terminate(); }
                    return 0;
                }
                if (status != USB_ERR_NORMAL_COMPLETION && status != USB_ERR_SHORT_XFER) {
                    log_line("xbox USB: endpoint index=%u status=%d actual=%u requested=%zu",
                             index, status, lengths[index], length);
                    terminate(); return -1;
                }
                if (lengths[index] > length) { terminate(); return -1; }
                if (!write) memcpy(data, buffers[index].data(), lengths[index]);
                return static_cast<int>(lengths[index]);
            }
            if ((xbox_transport_stopping() && !shutdownOutput) || std::chrono::steady_clock::now() >= deadline) {
                // Mark the device failed; persistent buffers remain alive until
                // USB_FS_UNINIT, even if cancellation completion arrives later.
                usb_fs_stop stop = {};
                stop.ep_index = index;
                ioctl(fd, USB_FS_STOP, &stop);
                terminate();
                return -1;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
int UsbDevice::bulkRead(uint8_t endpoint, FixedBytes<USB_MAX_BULK_TRANSFER_SIZE> &buffer) {
    if (endpoint != 4 && endpoint != 5) return -1;
    return transfer(endpoint == 5 ? 0 : 1, buffer.raw(), buffer.size(), false);
}
bool UsbDevice::bulkWrite(uint8_t endpoint, Bytes &data) {
    return endpoint == 4 && transfer(2, data.raw(), data.size(), true) == static_cast<int>(data.size());
}

std::unique_ptr<UsbDevice> UsbDeviceManager::getDevice(std::initializer_list<HardwareId> ids, UsbDevice::Terminate terminate) {
    while (!xbox_transport_stopping()) {
        // USB addresses can advance after enumeration; 20 is not a bus limit.
        for (int bus = 0; bus <= 5; ++bus) for (int device = 1; device < 128; ++device) {
            char path[32];
            snprintf(path, sizeof(path), "/dev/ugen%d.%d", bus, device);
            int fd = open(path, O_RDWR | O_NONBLOCK);
            if (fd < 0) continue;
            usb_device_descriptor desc = {};
            bool match = false;
            if (ioctl(fd, USB_GET_DEVICE_DESC, &desc) == 0)
                for (const auto &id : ids) if (UGETW(desc.idVendor) == id.vendorId && UGETW(desc.idProduct) == id.productId) match = true;
            if (match) {
                log_line("xbox USB: adapter %04x:%04x found at %s", UGETW(desc.idVendor), UGETW(desc.idProduct), path);
                return std::unique_ptr<UsbDevice>(new UsbDevice(fd, terminate));
            }
            close(fd);
        }
        for (unsigned i = 0; i < 100 && !xbox_transport_stopping(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return nullptr;
}
