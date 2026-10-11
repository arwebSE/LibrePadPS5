#pragma once
#include <cstddef>
#include <cstdint>
inline bool xbox_firmware_valid(const uint8_t *bytes, size_t size) {
    if (!bytes || size < 32 || size > 1024 * 1024) return false;
    uint32_t ilm = static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
                   (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
    uint32_t dlm = static_cast<uint32_t>(bytes[4]) | (static_cast<uint32_t>(bytes[5]) << 8) |
                   (static_cast<uint32_t>(bytes[6]) << 16) | (static_cast<uint32_t>(bytes[7]) << 24);
    return ilm > 0 && dlm > 0 && ilm <= size - 32 && dlm == size - 32 - ilm;
}
