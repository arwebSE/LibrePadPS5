#include "ps5/virtual_controller.h"
#include "input/usb_device.h"
#include "core/logging.h"
#include "core/time_utils.h"
#include "core/version.h"
#include "dashboard/http_server.h"
#include "input/controller_profiles.h"
#include "bluetooth/controller_host.h"
#ifdef LIBREPAD_WITH_XBOX
#include "../../modules/xbox/librepad_xbox.h"
#endif

#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#ifdef __PROSPERO__
#include <ps5/kernel.h>
#endif

#include "core/legacy_paths.h"
#define STATE_DIR FREEPAD_STATE_DIR
#define STOP_FLAG STATE_DIR "/stop"

static volatile sig_atomic_t running = 1;
static atomic_int stop_requested;

void freepad_request_stop(void)
{
    atomic_store(&stop_requested, 1);
}

static void stop_signal(int sig)
{
    (void)sig;
    running = 0;
}

int main(void)
{
    signal(SIGINT, stop_signal);
    signal(SIGTERM, stop_signal);
    signal(SIGPIPE, SIG_IGN);
    notify_ps5("LibrePadPS5 " LIBREPAD_VERSION ": starting");
    if (mkdir(STATE_DIR, 0755) != 0 && errno != EEXIST) {
        int directory_error = errno;
        printf("LibrePadPS5: mkdir %s failed: %s\n", STATE_DIR, strerror(directory_error));
        notify_ps5("LibrePadPS5: cannot create " STATE_DIR ": %s", strerror(directory_error));
        return 1;
    }
    /* Advisory lock is released automatically on process exit. Never kill
     * a PID from a stale file, and never load two instances of this build. */
    int lock_fd = open(STATE_DIR "/instance.lock", O_RDWR | O_CREAT, 0600);
    if (lock_fd < 0 || flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
        int lock_error = errno;
        printf("LibrePadPS5: instance lock fd=%d: %s (errno %d)\n", lock_fd, strerror(lock_error), lock_error);
        if (lock_fd >= 0 && (lock_error == EWOULDBLOCK || lock_error == EAGAIN))
            notify_ps5("LibrePadPS5 is already running. Open http://<PS5_IP>:8096/ or stop the old instance before loading this preview.");
        else notify_ps5("LibrePadPS5: cannot lock " STATE_DIR ": %s", strerror(lock_error));
        if (lock_fd >= 0) close(lock_fd);
        return 1;
    }
    log_init(FREEPAD_LOG_PATH);
    log_line("LibrePadPS5 version " LIBREPAD_VERSION);
#ifdef LIBREPAD_XBOX_STANDALONE
    log_line("LibrePadPS5 Xbox Wireless: native adapter / MT76 / GIP");
#elif defined(LIBREPAD_WITH_XBOX)
    log_line("LibrePadPS5: USB and Xbox Wireless; opt-in Bluetooth; shared virtual slots");
#else
    log_line("LibrePadPS5: USB and opt-in Bluetooth; shared virtual slots");
#endif
    if (!freepad_profiles_init(FREEPAD_PROFILES_PATH)) {
        log_line("Invalid/unreadable controller profiles; refusing startup");
        notify_ps5("LibrePadPS5: controllers.conf is invalid or unreadable. Check " FREEPAD_LOG_PATH);
        log_close();
        close(lock_fd);
        return 1;
    }

#ifdef __PROSPERO__
    uint32_t fw = kernel_get_fw_version();
    log_line("Firmware: 0x%08x", fw);
    if (fw < 0x07000000 || fw > 0x1360ffff) {
        log_line("Unsupported firmware; exiting without touching virtual pads");
        notify_ps5("LibrePadPS5: unsupported firmware 0x%08x. Check " FREEPAD_LOG_PATH, fw);
        log_close();
        close(lock_fd);
        return 1;
    }
#endif

    if (!vpad_init()) {
        log_line("Virtual pad initialization failed");
        notify_ps5("LibrePadPS5: virtual pad initialization failed. Check " FREEPAD_LOG_PATH " for the reason.");
        log_close();
        close(lock_fd);
        return 1;
    }
    usb_hotplug_init();
#ifdef LIBREPAD_WITH_XBOX
    librepad_xbox_start();
#endif
    if (!freepad_web_init(8096)) {
        log_line("LibrePadPS5 dashboard failed to start");
        notify_ps5("LibrePadPS5: dashboard failed on port 8096. Check " FREEPAD_LOG_PATH);
    } else {
        notify_ps5("LibrePadPS5 " LIBREPAD_VERSION " ready. Dashboard: http://<PS5_IP>:8096/");
    }
    log_line("Ready. Stop by creating " STOP_FLAG " or sending SIGTERM.");
    while (running && !atomic_load(&stop_requested)) {
        long now = now_ms();
        vpad_poll(now);
        bluetooth_poll();
        usb_hotplug_poll(now);
#ifdef LIBREPAD_WITH_XBOX
        librepad_xbox_poll(now);
#endif
        if (access(STOP_FLAG, F_OK) == 0) {
            unlink(STOP_FLAG);
            running = 0;
        }
        usleep(4000);
    }
    freepad_web_cleanup();
    bluetooth_cleanup();
    usb_hotplug_cleanup();
#ifdef LIBREPAD_WITH_XBOX
    librepad_xbox_stop();
#endif
    vpad_cleanup_all();
    log_line("LibrePadPS5 stopped cleanly");
    log_close();
    close(lock_fd);
    return 0;
}
