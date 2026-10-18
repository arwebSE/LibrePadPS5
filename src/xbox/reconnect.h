#pragma once
#include "xow/utils/bytes.h"

Bytes xbox_last_controller();
bool xbox_controller_identity(const Bytes &address, uint16_t &vendor, uint16_t &product);
void xbox_remember_controller(const Bytes &address, uint16_t vendor = 0, uint16_t product = 0);
