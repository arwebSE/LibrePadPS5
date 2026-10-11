#ifndef FREEPAD_COMPAT_H
#define FREEPAD_COMPAT_H

/* On-console compatibility only. Keep the same storage and lock namespace
 * as released builds so saved profile protection is not lost and old/new
 * payloads cannot run concurrently. Do not change these without migration. */
#ifndef FREEPAD_STATE_DIR
#define FREEPAD_STATE_DIR "/data/pdp-pad"
#endif
#define FREEPAD_LOG_PATH FREEPAD_STATE_DIR "/pdp-pad.log"
#define FREEPAD_NATIVE_USER_PATH FREEPAD_STATE_DIR "/native-user"
#define FREEPAD_NATIVE_USERS_PATH FREEPAD_STATE_DIR "/native-users"
#define FREEPAD_CONTROLLER_USER_PATH FREEPAD_STATE_DIR "/pdp-user"
#define FREEPAD_PROFILES_PATH FREEPAD_STATE_DIR "/controllers.conf"

#endif
