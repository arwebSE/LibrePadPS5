#pragma once
#include "core/controller_state.h"
#ifdef __cplusplus
extern "C" {
#endif
int xbox_sink_connect(void *controller, uint16_t vid, uint16_t pid);
void xbox_sink_input(void *controller, const pad_state_t *state);
void xbox_sink_disconnect(void *controller);
int xbox_transport_stopping(void);
#ifdef __cplusplus
}
#endif
