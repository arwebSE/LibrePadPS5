#define _DEFAULT_SOURCE
#include "freepad_profiles.h"
#include "usb_controllers.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct { uint16_t vid, pid; uint8_t map[14]; } controller_profile;
static const char *const targets[] = {
    "none", "cross", "circle", "square", "triangle", "l1", "r1", "l2", "r2",
    "create", "options", "l3", "r3", "ps", "touchpad"
};
static const uint32_t masks[] = {
    0, PAD_BTN_CROSS, PAD_BTN_CIRCLE, PAD_BTN_SQUARE, PAD_BTN_TRIANGLE,
    PAD_BTN_L1, PAD_BTN_R1, PAD_BTN_L2, PAD_BTN_R2, PAD_BTN_SHARE,
    PAD_BTN_OPTIONS, PAD_BTN_L3, PAD_BTN_R3, PAD_BTN_PS, PAD_BTN_TOUCHPAD
};
/* switch8 source bits: X,A,B,Y,L,R,ZL,ZR,Minus,Plus,L3,R3,Home,Capture. */
static const uint8_t defaults[14] = {4,2,1,3,5,6,7,8,9,10,11,12,13,14};
static controller_profile profiles[FREEPAD_PROFILE_MAX];
static size_t profile_count;
static char config_path[256];
static char last_report[256] = "No reports yet\n";
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static int hex4(const char *s, uint16_t *value)
{
    unsigned result = 0;
    for (int i = 0; i < 4; i++) {
        unsigned char c = (unsigned char)s[i];
        int n = c >= '0' && c <= '9' ? c-'0' : c >= 'a' && c <= 'f' ? c-'a'+10 :
                c >= 'A' && c <= 'F' ? c-'A'+10 : -1;
        if (n < 0) return 0;
        result = result * 16 + (unsigned)n;
    }
    *value = (uint16_t)result;
    return result != 0;
}

static int parse_text(const char *text, controller_profile *out, size_t *count)
{
    if (!text || strlen(text) >= FREEPAD_PROFILE_TEXT_MAX) return 0;
    char copy[FREEPAD_PROFILE_TEXT_MAX];
    strcpy(copy, text);
    size_t used = 0;
    char *lines = NULL;
    for (char *line = strtok_r(copy, "\n", &lines); line; line = strtok_r(NULL, "\n", &lines)) {
        size_t length = strlen(line);
        if (length && line[length-1] == '\r') line[--length] = 0;
        if (!length || line[0] == '#') continue;
        if (used == FREEPAD_PROFILE_MAX || length < 18 || line[4] != ':' ||
            strncmp(line+9, " switch8 ", 9)) return 0;
        controller_profile *p = &out[used];
        if (!hex4(line, &p->vid) || !hex4(line+5, &p->pid) || p->vid == 0x054c) return 0;
        for (size_t i = 0; i < used; i++)
            if (out[i].vid == p->vid && out[i].pid == p->pid) return 0;
        /* Deliberately no strtok for fields: empty/trailing fields are invalid. */
        char *field = line + 18;
        for (int i = 0; i < 14; i++) {
            char *comma = strchr(field, ',');
            if ((i < 13) != (comma != NULL)) return 0;
            if (comma) *comma = 0;
            int found = -1;
            for (size_t j = 0; j < sizeof(targets)/sizeof(targets[0]); j++)
                if (!strcmp(field, targets[j])) { found = (int)j; break; }
            if (found < 0) return 0;
            p->map[i] = (uint8_t)found;
            if (comma) field = comma+1;
        }
        used++;
    }
    if (!used) return 0;
    *count = used;
    return 1;
}

int freepad_profiles_init(const char *path)
{
    if (!path || strlen(path) >= sizeof(config_path)-16) return 0;
    controller_profile next[FREEPAD_PROFILE_MAX] = {{.vid=0x0e6f,.pid=0x0184}};
    memcpy(next[0].map, defaults, sizeof(defaults));
    size_t count = 1;
    FILE *file = fopen(path, "rb");
    if (file) {
        char text[FREEPAD_PROFILE_TEXT_MAX];
        size_t n = fread(text, 1, sizeof(text)-1, file);
        int extra = fgetc(file), error = ferror(file);
        fclose(file);
        text[n] = 0;
        if (extra != EOF || error || memchr(text, 0, n) || !parse_text(text, next, &count)) return 0;
    } else if (errno != ENOENT) return 0;
    pthread_mutex_lock(&lock);
    memcpy(profiles, next, count*sizeof(*next));
    profile_count = count;
    strcpy(config_path, path);
    pthread_mutex_unlock(&lock);
    return 1;
}

int freepad_profiles_allowed(uint16_t vid, uint16_t pid)
{
    int allowed = 0;
    pthread_mutex_lock(&lock);
    for (size_t i = 0; i < profile_count; i++)
        if (profiles[i].vid == vid && profiles[i].pid == pid) allowed = 1;
    pthread_mutex_unlock(&lock);
    return allowed;
}

int freepad_profiles_export(char *out, size_t size)
{
    size_t used = 0;
    int ok = size > 0;
    if (size) out[0] = 0;
    pthread_mutex_lock(&lock);
    for (size_t i = 0; i < profile_count && ok; i++) {
        int n = snprintf(out+used, size-used, "%04x:%04x switch8 ", profiles[i].vid, profiles[i].pid);
        if (n < 0 || (size_t)n >= size-used) { ok = 0; break; }
        used += (size_t)n;
        for (int j = 0; j < 14; j++) {
            n = snprintf(out+used, size-used, "%s%s", targets[profiles[i].map[j]], j == 13 ? "\n" : ",");
            if (n < 0 || (size_t)n >= size-used) { ok = 0; break; }
            used += (size_t)n;
        }
    }
    pthread_mutex_unlock(&lock);
    return ok;
}

int freepad_profiles_save(const char *text)
{
    controller_profile next[FREEPAD_PROFILE_MAX];
    size_t count = 0;
    if (!parse_text(text, next, &count)) return 0;
    pthread_mutex_lock(&lock);
    char temporary[256];
    snprintf(temporary, sizeof(temporary), "%s.XXXXXX", config_path);
    int fd = config_path[0] ? mkstemp(temporary) : -1;
    int ok = fd >= 0;
    size_t length = strlen(text), used = 0;
    while (ok && used < length) {
        ssize_t n = write(fd, text+used, length-used);
        if (n <= 0) { ok = 0; break; }
        used += (size_t)n;
    }
    if (ok && fsync(fd)) ok = 0;
    if (fd >= 0 && close(fd)) ok = 0;
    if (ok && rename(temporary, config_path)) ok = 0;
    if (!ok && fd >= 0) unlink(temporary);
    if (ok) { memcpy(profiles, next, count*sizeof(*next)); profile_count = count; }
    pthread_mutex_unlock(&lock);
    return ok;
}

int freepad_profiles_parse(uint16_t vid, uint16_t pid, const uint8_t *data,
                          size_t len, pad_state_t *state)
{
    if (!data || !state) return 0;
    pad_state_neutral(state);
    uint8_t map[14];
    int found = 0;
    pthread_mutex_lock(&lock);
    for (size_t i = 0; i < profile_count; i++)
        if (profiles[i].vid == vid && profiles[i].pid == pid) {
            memcpy(map, profiles[i].map, sizeof(map)); found = 1; break;
        }
    if (found) {
        size_t pos = (size_t)snprintf(last_report, sizeof(last_report), "%04x:%04x bytes=%zu report=", vid, pid, len);
        for (size_t i = 0; i < len && i < 64 && pos+3 < sizeof(last_report); i++)
            pos += (size_t)snprintf(last_report+pos, sizeof(last_report)-pos, "%02x", data[i]);
        snprintf(last_report+pos, sizeof(last_report)-pos, "%s\n", len > 64 ? "..." : "");
    }
    pthread_mutex_unlock(&lock);
    if (!found || !(len == 8 || (len == 9 && data[0] == 0))) return 0;
    size_t off = len == 9 ? 1 : 0;
    unsigned hat = data[off+2] & 15;
    if (hat > 7 && hat != 15) return 0;
    if (!usb_parse_input_report(CTRL_PDP_FACEOFF, data, len, state)) return 0;
    uint32_t old = state->buttons;
    state->buttons &= PAD_DPAD_UP | PAD_DPAD_RIGHT | PAD_DPAD_DOWN | PAD_DPAD_LEFT;
    for (int i = 0; i < 14; i++) if (old & masks[defaults[i]]) state->buttons |= masks[map[i]];
    state->l2 = (state->buttons & PAD_BTN_L2) ? 255 : 0;
    state->r2 = (state->buttons & PAD_BTN_R2) ? 255 : 0;
    return 1;
}

void freepad_profiles_report(char *out, size_t size)
{
    pthread_mutex_lock(&lock);
    snprintf(out, size, "%s", last_report);
    pthread_mutex_unlock(&lock);
}
