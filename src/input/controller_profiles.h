#ifndef LIBREPAD_PROFILES_H
#define LIBREPAD_PROFILES_H

#include "core/controller_state.h"
#include <stddef.h>
#include <stdint.h>
#include "input/protocol_decoder.h"

#define LIBREPAD_PROFILE_TEXT_MAX 2048
#define LIBREPAD_PROFILE_MAX 8
int librepad_profiles_init(const char *path);
int librepad_profiles_allowed(uint16_t vid, uint16_t pid);
size_t librepad_profiles_report_size(uint16_t vid, uint16_t pid);
int librepad_profiles_export(char *out, size_t size);
int librepad_profiles_save(const char *text);
int librepad_profiles_parse(uint16_t vid, uint16_t pid, const uint8_t *data,
                          size_t len, pad_state_t *state);
void librepad_profiles_report(char *out, size_t size);
usb_controller_type_t librepad_profiles_driver(uint16_t vid, uint16_t pid);
void librepad_profiles_remap(uint16_t vid, uint16_t pid, pad_state_t *state);

#endif
