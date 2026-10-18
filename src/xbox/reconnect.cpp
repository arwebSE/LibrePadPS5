#include "reconnect.h"
#include "core/paths.h"
#include "xow/utils/log.h"
#include <cstdio>

static bool valid(const Bytes &address) {
    if (address.size() != 6 || (address[0] & 1)) return false;
    for (auto byte : address) if (byte) return true;
    return false;
}

static Bytes record() {
    FILE *file = std::fopen(LIBREPAD_XBOX_CONTROLLER_PATH, "rb");
    if (!file) return {};
    uint8_t data[11];
    size_t count = std::fread(data, 1, sizeof(data), file);
    bool ok = !std::ferror(file);
    std::fclose(file);
    if (!ok || (count != 6 && count != 10)) return {};
    if (!valid(Bytes(data, data + 6))) return {};
    return Bytes(data, data + count);
}

Bytes xbox_last_controller() {
    Bytes saved = record();
    return saved.size() ? Bytes(saved.raw(), saved.raw() + 6) : Bytes();
}

bool xbox_controller_identity(const Bytes &address, uint16_t &vendor, uint16_t &product) {
    Bytes saved = record();
    if (saved.size() != 10 || Bytes(saved.raw(), saved.raw() + 6) != address) return false;
    uint16_t vid = saved[6] | (saved[7] << 8);
    if (!vid) return false;
    vendor = vid;
    product = saved[8] | (saved[9] << 8);
    return true;
}

void xbox_remember_controller(const Bytes &address, uint16_t vendor, uint16_t product) {
    if (!valid(address)) return;
    Bytes saved = address;
    if (vendor) {
        saved.append(static_cast<uint8_t>(vendor)); saved.append(static_cast<uint8_t>(vendor >> 8));
        saved.append(static_cast<uint8_t>(product)); saved.append(static_cast<uint8_t>(product >> 8));
    }
    if (record() == saved) return;
    FILE *file = std::fopen(LIBREPAD_XBOX_CONTROLLER_PATH, "wb");
    if (!file) { Log::error("Cannot save Xbox reconnect address"); return; }
    bool ok = std::fwrite(saved.raw(), 1, saved.size(), file) == saved.size();
    if (std::fclose(file)) ok = false;
    if (!ok) Log::error("Cannot save Xbox reconnect address");
}
