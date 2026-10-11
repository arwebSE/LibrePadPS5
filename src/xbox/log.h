#pragma once
#include <string>
#include <cstdio>
extern "C" {
#include "core/logging.h"
}
class Bytes;
namespace Log {
std::string formatBytes(const Bytes &bytes);
inline void init() {}
template<typename... Args> void info(std::string message, Args... args) { log_line(("xbox: " + message).c_str(), args...); }
template<typename... Args> void error(std::string message, Args... args) { log_line(("xbox ERROR: " + message).c_str(), args...); }
template<typename... Args> void debug(std::string message, Args... args) {
#ifdef DEBUG
    log_line(("xbox debug: " + message).c_str(), args...);
#else
    (void)message;
    (void)sizeof...(args);
#endif
}
}
