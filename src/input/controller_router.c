#include "input/controller_router.h"
#include "ps5/virtual_controller.h"
#include "core/legacy_paths.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

typedef struct {
    void *owner;
    pad_conn_type_t type;
    char name[64];
    pad_state_t state;
} source_t;
static source_t sources[MAX_SLOTS];
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void pause_path(int slot, char path[80])
{ snprintf(path, 80, FREEPAD_STATE_DIR "/pause-%d", slot); }
int controller_paused(int slot)
{
    char path[80]; pause_path(slot, path);
    return access(FREEPAD_STATE_DIR "/pause", F_OK) == 0 || access(path, F_OK) == 0;
}
int controller_attach(void *owner, pad_conn_type_t type, const char *name)
{
    if (!owner) return -1;
    pthread_mutex_lock(&mutex);
    int slot = -1;
    for (int i = 0; i < MAX_SLOTS; i++) if (sources[i].owner == owner) {
        pthread_mutex_unlock(&mutex); return i;
    }
    for (int i = 0; i < MAX_SLOTS; i++)
        if (!sources[i].owner && vpad_slot_is_free(i)) { slot = i; break; }
    if (slot >= 0) {
        source_t *source = &sources[slot];
        source->owner = owner; source->type = type;
        snprintf(source->name, sizeof(source->name), "%s", name ? name : "Gamepad");
        pad_state_neutral(&source->state);
        if (!controller_paused(slot)) vpad_add(slot, type, source->name);
    }
    pthread_mutex_unlock(&mutex);
    return slot;
}
void controller_update(void *owner, const pad_state_t *state)
{
    if (!owner || !state) return;
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_SLOTS; i++) if (sources[i].owner == owner) {
        sources[i].state = *state;
        vpad_update(i, state); break;
    }
    pthread_mutex_unlock(&mutex);
}
void controller_detach(void *owner)
{
    if (!owner) return;
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_SLOTS; i++) if (sources[i].owner == owner) {
        vpad_remove(i); memset(&sources[i], 0, sizeof(sources[i])); break;
    }
    pthread_mutex_unlock(&mutex);
}
int controller_connected(int slot)
{
    if (slot < 0 || slot >= MAX_SLOTS) return 0;
    pthread_mutex_lock(&mutex);
    int connected = sources[slot].owner != NULL;
    pthread_mutex_unlock(&mutex); return connected;
}
int controller_reconnect(int slot)
{
    if (slot < 0 || slot >= MAX_SLOTS) return 0;
    pthread_mutex_lock(&mutex);
    vpad_slot_info_t pad; memset(&pad, 0, sizeof(pad)); vpad_get_slot_info(slot, &pad);
    int ok = sources[slot].owner && (pad.status == VP_FREE || pad.status == VP_FAILED);
    if (ok && pad.status == VP_FAILED) vpad_remove(slot);
    /* Migrate a legacy global pause before clearing it, so reconnecting one
     * slot cannot silently unpause other devices on their next replug. */
    if (ok && access(FREEPAD_STATE_DIR "/pause", F_OK) == 0) {
        for (int i = 0; i < MAX_SLOTS; i++) {
            char paused[80]; pause_path(i, paused);
            int fd = open(paused, O_WRONLY | O_CREAT, 0600);
            if (fd < 0) { ok = 0; break; }
            close(fd);
        }
    }
    char path[80]; pause_path(slot, path);
    if (ok) ok = unlink(path) == 0 || errno == ENOENT;
    if (ok) ok = unlink(FREEPAD_STATE_DIR "/pause") == 0 || errno == ENOENT;
    if (ok) {
        ok = vpad_add(slot, sources[slot].type, sources[slot].name);
        if (ok) vpad_update(slot, &sources[slot].state);
    }
    pthread_mutex_unlock(&mutex); return ok;
}
void controller_name(int slot, char *name, size_t size)
{
    if (!name || !size || slot < 0 || slot >= MAX_SLOTS) return;
    pthread_mutex_lock(&mutex);
    snprintf(name, size, "%s", sources[slot].name);
    pthread_mutex_unlock(&mutex);
}
int controller_release(int slot)
{
    if (slot < -1 || slot >= MAX_SLOTS) return 0;
    pthread_mutex_lock(&mutex);
    int ok = 1;
    for (int i = 0; i < MAX_SLOTS; i++) if (slot < 0 || i == slot) {
        char path[80]; pause_path(i, path);
        int fd = open(path, O_WRONLY | O_CREAT, 0600);
        if (fd < 0) { ok = 0; break; }
        close(fd); vpad_remove(i);
    }
    if (ok && slot < 0) {
        int fd = open(FREEPAD_STATE_DIR "/pause", O_WRONLY | O_CREAT, 0600);
        if (fd < 0) ok = 0; else close(fd);
    }
    pthread_mutex_unlock(&mutex); return ok;
}
