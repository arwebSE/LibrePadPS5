#include "ps5/virtual_controller.h"
#include "input/usb_device.h"
#include "core/logging.h"
#include "core/time_utils.h"
#include "dashboard/http_server.h"
#include "input/controller_profiles.h"

#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>

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
    mkdir(STATE_DIR, 0755);
    /* Advisory lock is released automatically on process exit. Never kill
     * a PID from a stale file, and never load two instances of this build. */
    int lock_fd = open(STATE_DIR "/instance.lock", O_RDWR | O_CREAT, 0600);
    if (lock_fd < 0 || flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
        puts("FreePadUSBPS5: state directory unavailable or another instance is running");
        if (lock_fd >= 0) close(lock_fd);
        return 1;
    }
    log_init(FREEPAD_LOG_PATH);
    log_line("FreePadUSBPS5 development build; configurable USB report layouts; no Bluetooth or autoload");
    if (!freepad_profiles_init(FREEPAD_PROFILES_PATH)) {
        log_line("Invalid/unreadable controller profiles; refusing startup");
        log_close();
        close(lock_fd);
        return 1;
    }

#ifdef __PROSPERO__
    uint32_t fw = kernel_get_fw_version();
    log_line("Firmware: 0x%08x", fw);
    if (fw < 0x07000000 || fw > 0x1360ffff) {
        log_line("Unsupported firmware; exiting without touching virtual pads");
        log_close();
        return 1;
    }
#endif

    if (!vpad_init()) {
        log_line("Virtual pad initialization failed");
        log_close();
        return 1;
    }
    usb_hotplug_init();
    if (!freepad_web_init(8096)) log_line("FreePadUSBPS5 dashboard failed to start");
    log_line("Ready. Stop by creating " STOP_FLAG " or sending SIGTERM.");
    while (running && !atomic_load(&stop_requested)) {
        long now = now_ms();
        vpad_poll(now);
        usb_hotplug_poll(now);
        if (access(STOP_FLAG, F_OK) == 0) {
            unlink(STOP_FLAG);
            running = 0;
        }
        usleep(4000);
    }
    freepad_web_cleanup();
    usb_hotplug_cleanup();
    vpad_cleanup_all();
    log_line("FreePadUSBPS5 stopped cleanly");
    log_close();
    close(lock_fd);
    return 0;
}
