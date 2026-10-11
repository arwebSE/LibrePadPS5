#include "log.h"
#include "xow/utils/bytes.h"
std::string Log::formatBytes(const Bytes &bytes) {
    std::string result;
    char text[4];
    for (auto byte : bytes) { snprintf(text, sizeof(text), "%02x:", byte); result += text; }
    if (!result.empty()) result.pop_back();
    return result;
}
