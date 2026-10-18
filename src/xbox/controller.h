#pragma once
#include "xow/controller/gip.h"
#include "sink.h"

// Original GIP implementation is retained; only the Linux input sink is replaced.
class Controller : public GipDevice {
public:
    explicit Controller(SendPacket send, std::function<void(uint16_t, uint16_t)> onConnect = {},
                        uint16_t vendor = 0, uint16_t product = 0)
        : GipDevice(send), onConnect(onConnect), cachedVendor(vendor), cachedProduct(product) { pad_state_neutral(&state); }
    ~Controller() override { xbox_sink_disconnect(this); }
private:
    pad_state_t state;
    bool connected = false;
    std::function<void(uint16_t, uint16_t)> onConnect;
    uint16_t cachedVendor, cachedProduct;
    void deviceAnnounced(uint8_t id, const AnnounceData *data) override;
    void statusReceived(uint8_t, const StatusData *data) override;
    void guideButtonPressed(const GuideButtonData *data) override;
    void serialNumberReceived(const SerialData *) override {}
    void inputReceived(const InputData *data) override;
};
