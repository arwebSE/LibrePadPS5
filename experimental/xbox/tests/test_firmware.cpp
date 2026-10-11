#include "../port/firmware.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <vector>
int main(int argc, char **argv) {
    assert(!xbox_firmware_valid(nullptr, 0));
    std::vector<uint8_t> data(40, 0);
    data[0] = 4; data[4] = 4;
    assert(xbox_firmware_valid(data.data(), data.size()));
    for (size_t n = 0; n < data.size(); ++n) assert(!xbox_firmware_valid(data.data(), n));
    data[3] = 0xff;
    assert(!xbox_firmware_valid(data.data(), data.size()));
    if (argc > 1) {
        std::ifstream file(argv[1], std::ios::binary);
        std::vector<uint8_t> actual((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        assert(xbox_firmware_valid(actual.data(), actual.size()));
    }
    puts("Xbox firmware: header / truncation / overflow / local firmware validation tests passed");
}
