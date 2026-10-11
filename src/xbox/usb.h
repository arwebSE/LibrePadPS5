#pragma once
#include "xow/utils/bytes.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <stdexcept>
#include <mutex>
#include <array>
#include <atomic>
#ifdef XBOX_USB_TEST
#include "xbox/usb_mock.h"
#else
#include <dev/usb/usb.h>
#include <dev/usb/usb_ioctl.h>
#endif
#define USB_MAX_BULK_TRANSFER_SIZE 512

class UsbDevice {
public:
    using Terminate = std::function<void()>;
    struct ControlPacket { uint8_t request; uint16_t value, index; uint8_t *data; uint16_t length; };
    UsbDevice(int fd, Terminate terminate);
    ~UsbDevice();
    void controlTransfer(ControlPacket packet, bool write);
    int bulkRead(uint8_t endpoint, FixedBytes<USB_MAX_BULK_TRANSFER_SIZE> &buffer);
    bool bulkWrite(uint8_t endpoint, Bytes &data);
    void prepareShutdown() { shutdownWrites = true; }
private:
    static constexpr size_t capacity = 0x4000;
    int fd;
    bool initialized = false;
    bool claimed = false;
    std::atomic<bool> shutdownWrites{false};
    Terminate terminate;
    std::mutex ioMutex;
    std::array<std::mutex, 3> endpointMutex;
    usb_fs_endpoint endpoints[3] = {};
    std::array<std::array<uint8_t, capacity>, 3> buffers;
    void *frames[3] = {};
    uint32_t lengths[3] = {};
    bool complete[3] = {};
    int transfer(unsigned index, uint8_t *data, size_t length, bool write);
    void closeDevice();
};

class UsbDeviceManager {
public:
    struct HardwareId { uint16_t vendorId, productId; };
    std::unique_ptr<UsbDevice> getDevice(std::initializer_list<HardwareId> ids, UsbDevice::Terminate terminate);
};
class UsbException : public std::runtime_error {
public: explicit UsbException(const std::string &message) : std::runtime_error(message) {}
};
