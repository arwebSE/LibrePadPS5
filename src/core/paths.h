#ifndef LIBREPAD_PATHS_H
#define LIBREPAD_PATHS_H

/* Active storage. Old releases reach this directory through a migration alias. */
#ifndef LIBREPAD_STATE_DIR
#define LIBREPAD_STATE_DIR "/data/LibrePadPS5"
#endif
#define LIBREPAD_LOG_PATH LIBREPAD_STATE_DIR "/LibrePadPS5.log"
#define LIBREPAD_NATIVE_USER_PATH LIBREPAD_STATE_DIR "/native-user"
#define LIBREPAD_NATIVE_USERS_PATH LIBREPAD_STATE_DIR "/native-users"
#define LIBREPAD_CONTROLLER_USER_PATH LIBREPAD_STATE_DIR "/controller-user"
#define LIBREPAD_PROFILES_PATH LIBREPAD_STATE_DIR "/controllers.conf"
#define LIBREPAD_XBOX_FIRMWARE_PATH LIBREPAD_STATE_DIR "/xbox/firmware.bin"
#ifndef LIBREPAD_XBOX_CONTROLLER_PATH
#define LIBREPAD_XBOX_CONTROLLER_PATH LIBREPAD_STATE_DIR "/xbox-controller"
#endif

#endif
