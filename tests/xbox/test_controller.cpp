#include "xbox/controller.h"
#include "xow/utils/bytes.h"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <vector>

static pad_state_t latest;
static int connects, reports, disconnects;
static void *owner;
extern "C" int xbox_sink_connect(void *controller, uint16_t vid, uint16_t pid) {
    assert(vid == 0x045e && pid == 0x02dd);
    owner = controller; ++connects; return 1;
}
extern "C" void xbox_sink_input(void *controller, const pad_state_t *state) {
    assert(controller == owner); latest = *state; ++reports;
}
extern "C" void xbox_sink_disconnect(void *controller) { if (controller == owner) ++disconnects; }
extern "C" void log_line(const char *, ...) {}

int main() {
    std::vector<Bytes> sent;
    {
        Controller unknown([&](const Bytes &bytes) { sent.push_back(bytes); return true; });
        Bytes input(18); input[0] = 0x20; input[3] = 14;
        unknown.handlePacket(input);
        assert(connects == 0 && reports == 0 && sent.empty());
    }
    {
        Controller controller([&](const Bytes &bytes) { sent.push_back(bytes); return true; });
        Bytes announce(32);
        announce[0] = 0x02; announce[1] = 0x20; announce[2] = 1; announce[3] = 28;
        announce[12] = 0x5e; announce[13] = 0x04;
        announce[14] = 0xdd; announce[15] = 0x02;
        assert(controller.handlePacket(announce));
        assert(connects == 1 && sent.size() == 2);
        assert(sent[0][0] == 0x05 && sent[0][4] == 0);
        assert(sent[1][0] == 0x0a && sent[1][5] == 1);
        controller.handlePacket(announce);
        assert(connects == 1);
        Bytes input = {0x20, 0x00, 2, 14, 0xff, 0xff, 0xff, 0x03, 0x00, 0x02,
                       0x00, 0x80, 0xff, 0x7f, 0, 0, 0, 0};
        controller.handlePacket(input);
        assert(reports == 1);
        assert(latest.buttons & PAD_BTN_CROSS);
        assert(latest.buttons & PAD_BTN_CIRCLE);
        assert(latest.buttons & PAD_BTN_SQUARE);
        assert(latest.buttons & PAD_BTN_TRIANGLE);
        assert(latest.buttons & PAD_BTN_OPTIONS);
        assert(latest.buttons & PAD_BTN_CREATE);
        assert(latest.buttons & PAD_DPAD_UP);
        assert(latest.buttons & PAD_DPAD_DOWN);
        assert(latest.buttons & PAD_DPAD_LEFT);
        assert(latest.buttons & PAD_DPAD_RIGHT);
        assert(latest.buttons & PAD_BTN_L1);
        assert(latest.buttons & PAD_BTN_R1);
        assert(latest.buttons & PAD_BTN_L3);
        assert(latest.buttons & PAD_BTN_R3);
        assert(latest.l2 == 255 && latest.r2 == 127);
        assert(latest.lx == 0 && latest.ly == 0 && latest.rx == 128 && latest.ry == 128);
        Bytes guide = {0x07, 0x30, 3, 2, 1, 0};
        controller.handlePacket(guide);
        assert(latest.buttons & PAD_BTN_PS);
        assert(sent.back()[0] == 1); // ACK requested by 0x10 flag.
        controller.handlePacket(input);
        assert(latest.buttons & PAD_BTN_PS); // ordinary input preserves Guide.
        guide[4] = 0;
        controller.handlePacket(guide);
        assert(!(latest.buttons & PAD_BTN_PS));
        const int previous = reports;
        for (size_t n = 0; n < input.size(); ++n) controller.handlePacket(Bytes(input, 0, input.size() - n));
        assert(reports == previous);
        Bytes malformed = guide;
        malformed[3] = 255;
        size_t acknowledgements = sent.size();
        controller.handlePacket(malformed);
        assert(sent.size() == acknowledgements && reports == previous);
        malformed = input; malformed[1] = 0x80;
        controller.handlePacket(malformed);
        assert(reports == previous); // chunks require reassembly, never guess
        Bytes accessory = input; accessory[1] = 1;
        controller.handlePacket(accessory);
        assert(reports == previous);
        Bytes neutral = input;
        for (size_t i = 4; i < neutral.size(); ++i) neutral[i] = 0;
        controller.handlePacket(neutral);
        assert(latest.buttons == 0 && latest.l2 == 0 && latest.r2 == 0);
        assert(latest.lx == 128 && latest.ly == 128);
        // Exercise malformed radio/GIP packets over all supported lengths.
        uint32_t random = 0x12345678;
        for (unsigned i = 0; i < 50000; ++i) {
            Bytes packet(i % 65);
            for (size_t j = 0; j < packet.size(); ++j) {
                random ^= random << 13; random ^= random >> 17; random ^= random << 5;
                packet[j] = static_cast<uint8_t>(random);
            }
            controller.handlePacket(packet);
        }
    }
    assert(disconnects == 1);
    puts("Xbox controller mapping / Guide / ACK / truncation tests passed");
}
