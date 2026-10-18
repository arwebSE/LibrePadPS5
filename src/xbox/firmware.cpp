#include "firmware.h"
#include "xbox/firmware_blob.h"
#include "xow/utils/log.h"
#include <cerrno>
#include <cstdio>
#include <utility>

// Called by MT76 initialization only after discovery found an Xbox adapter.
bool xbox_load_firmware(Bytes &out) {
    FILE *file = std::fopen(FIRMWARE, "rb");
    if (!file) {
        if (errno != ENOENT) {
            Log::error("Cannot read Xbox firmware override %s (errno=%d)", FIRMWARE, errno);
            return false;
        }
        out = Bytes(XBOX_FIRMWARE, XBOX_FIRMWARE + sizeof(XBOX_FIRMWARE));
        Log::info("Using bundled Xbox Wireless adapter firmware (%zu bytes)", out.size());
        return true;
    }
    long size = -1;
    if (!std::fseek(file, 0, SEEK_END)) size = std::ftell(file);
    if (size < 32 || size > 1024 * 1024 || std::fseek(file, 0, SEEK_SET)) {
        std::fclose(file);
        Log::error("Invalid Xbox firmware override size: %s", FIRMWARE);
        return false;
    }
    Bytes bytes(static_cast<size_t>(size));
    bool read = std::fread(bytes.raw(), 1, bytes.size(), file) == bytes.size();
    std::fclose(file);
    if (!read || !xbox_firmware_valid(bytes.raw(), bytes.size())) {
        Log::error("Invalid Xbox firmware override; no firmware transfers sent: %s", FIRMWARE);
        return false;
    }
    out = std::move(bytes);
    Log::info("Using Xbox Wireless firmware override: %s (%zu bytes)", FIRMWARE, out.size());
    return true;
}
