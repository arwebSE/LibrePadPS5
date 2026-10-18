#include "reconnect.h"
#include "core/paths.h"
#include "xow/utils/log.h"
#include <cstdio>

static bool valid(const Bytes &address) {
    if (address.size() != 6 || (address[0] & 1)) return false;
    for (auto byte : address) if (byte) return true;
    return false;
}

Bytes xbox_last_controller() {
    FILE *file = std::fopen(LIBREPAD_XBOX_CONTROLLER_PATH, "rb");
    if (!file) return {};
    uint8_t data[7];
    size_t count = std::fread(data, 1, sizeof(data), file);
    bool ok = !std::ferror(file);
    std::fclose(file);
    Bytes address(data, data + count);
    return ok && valid(address) ? address : Bytes();
}

void xbox_remember_controller(const Bytes &address) {
    if (!valid(address) || xbox_last_controller() == address) return;
    FILE *file = std::fopen(LIBREPAD_XBOX_CONTROLLER_PATH, "wb");
    if (!file) { Log::error("Cannot save Xbox reconnect address"); return; }
    bool ok = std::fwrite(address.raw(), 1, address.size(), file) == address.size();
    if (std::fclose(file)) ok = false;
    if (!ok) Log::error("Cannot save Xbox reconnect address");
}
