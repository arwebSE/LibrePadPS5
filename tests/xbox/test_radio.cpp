#include "xow/dongle/mt76.h"
#include "xow/dongle/dongle.h"
#include "xbox/sink.h"
#include "xbox/firmware.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <map>

static std::vector<Bytes> output;
static std::map<unsigned, uint32_t> registers;
static int connects, disconnects, inputs;
static void *owner;
static bool uploading;
extern "C" int xbox_sink_connect(void *controller, uint16_t vid, uint16_t pid) {
    assert(vid == 0x045e && pid == 0x02dd); ++connects; owner = controller; return 1;
}
extern "C" void xbox_sink_disconnect(void *controller) { if (controller == owner) ++disconnects; }
extern "C" void xbox_sink_input(void *controller, const pad_state_t *) { assert(controller == owner); ++inputs; }
extern "C" void log_line(const char *, ...) {}
UsbDevice::UsbDevice(int descriptor, Terminate callback) : fd(descriptor), terminate(callback) {}
UsbDevice::~UsbDevice() {}
bool UsbDevice::bulkWrite(uint8_t endpoint, Bytes &packet) {
    assert(endpoint == 4); output.push_back(packet);
    if (uploading) registers[0x0234] |= 0xc0000000; // DMA completion
    return true;
}
int UsbDevice::bulkRead(uint8_t, FixedBytes<USB_MAX_BULK_TRANSFER_SIZE> &) { return -1; }
void UsbDevice::controlTransfer(ControlPacket packet, bool write) {
    if (write && !packet.length) { registers[0x0230] = packet.value == 0x12 ? 1 : 0x80000000; return; }
    assert(packet.length == 4);
    if (write) memcpy(&registers[packet.index], packet.data, 4);
    else memcpy(packet.data, &registers[packet.index], 4);
}
class Radio : public Mt76 {
public:
    Radio() : Mt76(std::unique_ptr<UsbDevice>(new UsbDevice(123, [] {}))) {
        macAddress = {0x62, 0x45, 0xb4, 0xe7, 0x24, 0x99};
    }
    using Mt76::associateClient;
    using Mt76::removeClient;
    using Mt76::pairClient;
    using Mt76::loadFirmware;
};
class RadioHarness : public Dongle {
public:
    RadioHarness() : Dongle(std::unique_ptr<UsbDevice>(new UsbDevice(123, [] {}))) {
        macAddress = {0x62, 0x45, 0xb4, 0xe7, 0x24, 0x99};
    }
    using Dongle::handleBulkData;
};
static Bytes envelope(const Bytes &payload) {
    uint32_t header = static_cast<uint32_t>(payload.size()) | (1u << 19);
    Bytes message; message.append(header); message.append(payload); message.append(header); return message;
}
static Bytes wlan(uint8_t frameControl, const Bytes &body) {
    Bytes frame(56); frame[4] = 1; frame[32] = frameControl;
    const uint8_t address[] = {0x62, 0x45, 0xb4, 0xe7, 0x24, 0x99};
    const uint8_t client[] = {0x7e, 0xed, 0x89, 0xac, 0xdc, 0x81};
    memcpy(frame.raw() + 36, address, 6); memcpy(frame.raw() + 42, client, 6);
    memcpy(frame.raw() + 48, address, 6); frame.append(body); return envelope(frame);
}
static void checkResponse(uint8_t subtype, const Bytes &client) {
    // One 4-byte TX descriptor, 20-byte TXWI, 24-byte WLAN header,
    // 14-byte association body, alignment and trailer.
    assert(output.size() == 4);
    const Bytes &packet = output[2];
    assert(packet.size() == 68 && packet[24] == (subtype << 4));
    assert(!memcmp(packet.raw() + 28, client.raw(), 6));
    assert(packet[50] == 0x10 && packet[51] == 0x01);
}
int main() {
    Radio radio;
    Bytes client = {0x7e, 0xed, 0x89, 0xac, 0xdc, 0x81};
    output.clear();
    assert(radio.pairClient(client));
    assert(output.size() == 1 && output[0].size() == 64);
    const uint8_t payload[] = {0x70, 2, 0, 0x45, 0x55, 1, 0x0f, 0x8f, 0xff, 0x87, 0x1f};
    assert(!memcmp(output[0].raw() + 48, payload, sizeof(payload)));
    output.clear();
    assert(radio.associateClient(client, true) == 1);
    checkResponse(3, client);
    for (unsigned i = 0; i < 40; ++i) {
        output.clear();
        assert(radio.associateClient(client, true) == 1);
        checkResponse(3, client);
    }
    Bytes other = client; other[5]++;
    output.clear();
    assert(radio.associateClient(other) == 2);
    checkResponse(1, other);
    assert(radio.removeClient(1));
    assert(radio.associateClient(client, true) == 1);
    // Warm restart must not bulk-upload firmware into the running radio.
    uint8_t firmware[34] = {}; firmware[0] = firmware[4] = 1;
    std::ofstream file(FIRMWARE, std::ios::binary); file.write(reinterpret_cast<char *>(firmware), sizeof(firmware)); file.close();
    registers[0x0230] = 1;
    output.clear();
    assert(radio.loadFirmware() && output.empty());
    std::remove(FIRMWARE);
    assert(radio.loadFirmware() && output.empty()); // same warm restart using bundled image
    registers[0x0230] = 0; uploading = true;
    assert(radio.loadFirmware()); // fresh adapter, no console firmware file
    Bytes bundled, transferred;
    assert(xbox_load_firmware(bundled));
    for (const Bytes &packet : output) {
        unsigned size = packet[0] | (packet[1] << 8);
        assert(size <= 0x3800 && packet.size() >= size + 4);
        transferred.append(packet.begin() + 4, packet.begin() + 4 + size);
    }
    assert(transferred.size() == bundled.size() - 32);
    assert(!memcmp(transferred.raw(), bundled.raw() + 32, transferred.size()));
    uploading = false;
    {
        RadioHarness dongle;
        Bytes request = wlan(0x20, Bytes(16)); // controller's real reassociation subtype
        output.clear(); dongle.handleBulkData(request); checkResponse(3, client);
        Bytes announce(32); announce[0] = 2; announce[1] = 0x20; announce[2] = 1; announce[3] = 28;
        announce[12] = 0x5e; announce[13] = 4; announce[14] = 0xdd; announce[15] = 2;
        Bytes qos(4); qos.append(announce);
        dongle.handleBulkData(wlan(0x88, qos));
        assert(connects == 1 && disconnects == 0);
        for (int i = 0; i < 40; ++i) dongle.handleBulkData(request);
        assert(connects == 1 && disconnects == 0); // retain the announced controller object
        Bytes input = {0x20, 0, 2, 14, 0, 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        qos = Bytes(4); qos.append(input); dongle.handleBulkData(wlan(0x88, qos));
        assert(inputs == 1);
        for (size_t size = 0; size < request.size(); ++size)
            dongle.handleBulkData(Bytes(request, 0, request.size() - size));
    }
    assert(disconnects == 1);
    puts("Xbox radio: pairing / reassociation / duplicate WCID / warm restart / bundled cold upload tests passed");
}
