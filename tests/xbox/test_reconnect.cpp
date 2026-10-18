#include "xbox/reconnect.h"
#include "core/paths.h"
#include <cassert>
#include <cstdio>
extern "C" void log_line(const char *, ...) {}

int main() {
    std::remove(LIBREPAD_XBOX_CONTROLLER_PATH);
    assert(xbox_last_controller().size() == 0);
    Bytes address = {0x7e, 0xed, 0x89, 0xac, 0xdc, 0x81};
    xbox_remember_controller(address);
    assert(xbox_last_controller() == address);
    uint16_t vendor = 0, product = 0;
    assert(!xbox_controller_identity(address, vendor, product)); // legacy MAC-only record
    xbox_remember_controller(address, 0x045e, 0x02dd);
    assert(xbox_last_controller() == address);
    assert(xbox_controller_identity(address, vendor, product));
    assert(vendor == 0x045e && product == 0x02dd);
    Bytes other = address; other[5]++;
    assert(!xbox_controller_identity(other, vendor, product));
    xbox_remember_controller(Bytes(6));
    Bytes invalid = address; invalid[0] |= 1;
    xbox_remember_controller(invalid);
    assert(xbox_last_controller() == address);
    address[5]++;
    xbox_remember_controller(address);
    assert(xbox_last_controller() == address);
    for (size_t size : {0u, 5u, 7u}) {
        FILE *file = std::fopen(LIBREPAD_XBOX_CONTROLLER_PATH, "wb");
        uint8_t data[7] = {};
        assert(file && std::fwrite(data, 1, size, file) == size);
        std::fclose(file);
        assert(xbox_last_controller().size() == 0);
    }
    std::remove(LIBREPAD_XBOX_CONTROLLER_PATH);
    puts("Xbox reconnect storage: saved address / replacement / invalid and truncated records passed");
}
