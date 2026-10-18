#include "xbox/firmware.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
extern "C" void log_line(const char *, ...) {}
int main(int argc, char **argv) {
    std::remove(FIRMWARE);
    Bytes bundled;
    assert(xbox_load_firmware(bundled));
    assert(bundled.size() == 70620 && xbox_firmware_valid(bundled.raw(), bundled.size()));
    assert(!std::fopen(FIRMWARE, "rb")); // no state file is created
    assert(!xbox_firmware_valid(nullptr, 0));
    std::vector<uint8_t> data(40, 0);
    data[0] = 4; data[4] = 4;
    assert(xbox_firmware_valid(data.data(), data.size()));
    for (size_t n = 0; n < data.size(); ++n) assert(!xbox_firmware_valid(data.data(), n));
    data[3] = 0xff;
    assert(!xbox_firmware_valid(data.data(), data.size()));
    auto save = [](const std::vector<uint8_t> &bytes) {
        std::ofstream file(FIRMWARE, std::ios::binary);
        file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        assert(file.good());
    };
    Bytes loaded;
    save(data); assert(!xbox_load_firmware(loaded)); // malformed override is not hidden
    data[3] = 0; save(data);
    assert(xbox_load_firmware(loaded) && loaded.size() == data.size());
    assert(!std::memcmp(loaded.raw(), data.data(), data.size()));
    data.resize(31); save(data); assert(!xbox_load_firmware(loaded));
    data.resize(1024 * 1024 + 1); save(data); assert(!xbox_load_firmware(loaded));
    std::remove(FIRMWARE);
    assert(xbox_load_firmware(loaded) && loaded.size() == bundled.size());
    if (argc > 1) {
        std::ifstream file(argv[1], std::ios::binary);
        std::vector<uint8_t> actual((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        assert(xbox_firmware_valid(actual.data(), actual.size()));
        assert(actual.size() == bundled.size() && !std::memcmp(actual.data(), bundled.raw(), actual.size()));
    }
    puts("Xbox firmware: bundled bytes / lazy file fallback / overrides / header bounds tests passed");
}
