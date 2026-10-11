#ifndef LIBREPAD_LEGACY_PATHS_H
#define LIBREPAD_LEGACY_PATHS_H

/* On-console compatibility only. Keep the same storage and lock namespace
 * as released builds so saved profile protection is not lost and old/new
 * payloads cannot run concurrently. Do not change these without migration. */
#ifndef LIBREPAD_STATE_DIR
#define LIBREPAD_STATE_DIR "/data/pdp-pad"
#endif
#define LIBREPAD_LOG_PATH LIBREPAD_STATE_DIR "/pdp-pad.log"
#define LIBREPAD_NATIVE_USER_PATH LIBREPAD_STATE_DIR "/native-user"
#define LIBREPAD_NATIVE_USERS_PATH LIBREPAD_STATE_DIR "/native-users"
#define LIBREPAD_CONTROLLER_USER_PATH LIBREPAD_STATE_DIR "/pdp-user"
#define LIBREPAD_PROFILES_PATH LIBREPAD_STATE_DIR "/controllers.conf"
#define LIBREPAD_XBOX_FIRMWARE_PATH LIBREPAD_STATE_DIR "/xbox/firmware.bin"

#endif
