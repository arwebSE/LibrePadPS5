#ifndef FREEPAD_COMPAT_H
#define FREEPAD_COMPAT_H

/* On-console compatibility only. Keep the same storage and lock namespace
 * as released builds so saved profile protection is not lost and old/new
 * payloads cannot run concurrently. Do not change these without migration. */
#define FREEPAD_STATE_DIR "/data/pdp-pad"
#define FREEPAD_LOG_PATH FREEPAD_STATE_DIR "/pdp-pad.log"
#define FREEPAD_NATIVE_USER_PATH FREEPAD_STATE_DIR "/native-user"
#define FREEPAD_CONTROLLER_USER_PATH FREEPAD_STATE_DIR "/pdp-user"

#endif
