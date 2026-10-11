#pragma once
#include "xow/controller/gip.h"
#include "sink.h"

// Original GIP implementation is retained; only the Linux input sink is replaced.
class Controller : public GipDevice {
public:
    explicit Controller(SendPacket send) : GipDevice(send) { pad_state_neutral(&state); }
    ~Controller() override { xbox_sink_disconnect(this); }
private:
    pad_state_t state;
    bool connected = false;
    void deviceAnnounced(uint8_t id, const AnnounceData *data) override;
    void statusReceived(uint8_t, const StatusData *data) override;
    void guideButtonPressed(const GuideButtonData *data) override;
    void serialNumberReceived(const SerialData *) override {}
    void inputReceived(const InputData *data) override;
};
