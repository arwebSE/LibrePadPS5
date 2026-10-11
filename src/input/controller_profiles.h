#ifndef FREEPAD_PROFILES_H
#define FREEPAD_PROFILES_H

#include "core/controller_state.h"
#include <stddef.h>
#include <stdint.h>

#define FREEPAD_PROFILE_TEXT_MAX 2048
#define FREEPAD_PROFILE_MAX 8
int freepad_profiles_init(const char *path);
int freepad_profiles_allowed(uint16_t vid, uint16_t pid);
size_t freepad_profiles_report_size(uint16_t vid, uint16_t pid);
int freepad_profiles_export(char *out, size_t size);
int freepad_profiles_save(const char *text);
int freepad_profiles_parse(uint16_t vid, uint16_t pid, const uint8_t *data,
                          size_t len, pad_state_t *state);
void freepad_profiles_report(char *out, size_t size);

#endif
