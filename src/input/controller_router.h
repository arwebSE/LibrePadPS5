#ifndef LIBREPAD_CONTROLLER_ROUTER_H
#define LIBREPAD_CONTROLLER_ROUTER_H
#include "core/controller_state.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Physical owners keep their slot while released, so another backend cannot
 * steal it. All callbacks may be called from transport reader threads. */
int controller_attach(void *owner, pad_conn_type_t type, const char *name);
void controller_update(void *owner, const pad_state_t *state);
void controller_detach(void *owner);
int controller_connected(int slot);
void controller_name(int slot, char *name, size_t size);
int controller_reconnect(int slot);
int controller_release(int slot); /* -1 releases all slots, including empty ones. */
int controller_paused(int slot);
#ifdef __cplusplus
}
#endif
#endif
