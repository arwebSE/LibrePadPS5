#include "input/usb_device.h"
#include "ps5/virtual_controller.h"
#include "core/logging.h"
#include "core/time_utils.h"
#include "core/legacy_paths.h"
#include "input/controller_profiles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>

#ifdef __PROSPERO__
#include <dev/usb/usb.h>
#include <dev/usb/usb_ioctl.h>
#include <dev/usb/usb_endian.h>
#include <dev/usb/usbdi.h>
#endif

typedef struct {
    int                   active;
    int                   slot;
    int                   fd;        /* /dev/ugenX.Y control device */
    char                  dev_path[32];
    uint16_t              vid, pid;
    const char           *name;
    pad_conn_type_t       conn_type;
    pthread_t             thread;
    volatile int          stop_thread;

#ifdef __PROSPERO__
    struct usb_fs_endpoint endpoints[4];
    uint8_t               in_index;
    int                   use_fs;
    int                   in_pending;
    void                 *fs_frame[4];
    uint32_t              fs_len[4];
    uint8_t               rx_buf[64];
#else
    int                   ep_fd[6];
#endif
} usb_slot_device_t;

typedef struct {
    usb_slot_device_t devices[MAX_SLOTS];
    long              last_scan_time;
    pthread_mutex_t   lock;
} usb_hotplug_state_t;

static usb_hotplug_state_t g_usb_hotplug;
static uint32_t s_last_seen[6][21] = {{0}};

#ifdef __PROSPERO__

/* Prefer endpoint 0x81, otherwise use the first advertised input endpoint.
 * No controller-specific output or wake-up commands are issued. */
static uint8_t find_usb_input_endpoint(int fd)
{
    struct usb_gen_descriptor desc = {0};
    uint8_t raw[512] = {0}, first = 0;
    desc.ugd_data = raw; desc.ugd_maxlen = sizeof(raw);
    if (ioctl(fd,USB_GET_FULL_DESC,&desc) != 0) return 0;
    int length = desc.ugd_actlen;
    if (length <= 0 || length > (int)sizeof(raw)) return 0;
    for (int offset = 0; offset+2 <= length;) {
        int entry = raw[offset];
        if (entry < 2 || offset+entry > length) break;
        if (raw[offset+1] == 5 && entry >= 7) {
            uint8_t address = raw[offset+2], type = raw[offset+3] & 3;
            if ((address & 0x80) && (type == 3 || type == 2)) {
                if (address == 0x81) return address;
                if (!first) first = address;
            }
        }
        offset += entry;
    }
    return first;
}

static int usb_fs_setup_device(usb_slot_device_t *sdev)
{
    int iface = 0;
    ioctl(sdev->fd, USB_CLAIM_INTERFACE, &iface);

    int config = 0;
    if (ioctl(sdev->fd, USB_GET_CONFIG, &config) == 0 && (config & 0xff) == 0xff) {
        config = 0;
        ioctl(sdev->fd, USB_SET_CONFIG, &config);
    }

    uint8_t in_addr = find_usb_input_endpoint(sdev->fd);
    if (!in_addr) return 0;

    struct usb_fs_init init;
    memset(sdev->endpoints, 0, sizeof(sdev->endpoints));
    memset(&init, 0, sizeof(init));
    init.pEndpoints = sdev->endpoints;
    init.ep_index_max = 1;

    if (ioctl(sdev->fd, USB_FS_INIT, &init) != 0) {
        if (errno != ENOTTY && errno != ENXIO && errno != EIO && errno != EBADF) {
            log_line("usb_hotplug: USB_FS_INIT failed for %s (%s)", sdev->dev_path, strerror(errno));
        }
        return 0;
    }

    sdev->in_index = 0;
    struct usb_fs_open open_in;
    int in_opened = 0;
    uint8_t try_ins[] = { in_addr };
    for (size_t i = 0; i < sizeof(try_ins); i++) {
        if (try_ins[i] == 0) continue;
        memset(&open_in, 0, sizeof(open_in));
        open_in.max_bufsize = 64;
        open_in.max_frames = 1;
        open_in.ep_index = sdev->in_index;
        open_in.ep_no = try_ins[i];
        if (ioctl(sdev->fd, USB_FS_OPEN, &open_in) == 0) {
            in_addr = try_ins[i];
            in_opened = 1;
            break;
        }
    }
    if (!in_opened) {
        log_line("usb_hotplug: USB_FS_OPEN IN failed for %s (%s)", sdev->dev_path, strerror(errno));
        return 0;
    }

    sdev->fs_frame[0] = sdev->rx_buf;
    sdev->fs_len[0] = sizeof(sdev->rx_buf);
    sdev->endpoints[0].ppBuffer = &sdev->fs_frame[0];
    sdev->endpoints[0].pLength = &sdev->fs_len[0];
    sdev->endpoints[0].nFrames = 1;
    sdev->endpoints[0].flags = USB_FS_FLAG_SINGLE_SHORT_OK;

    sdev->use_fs = 1;
    log_line("usb_hotplug: %s claimed via USB_FS (IN=0x%02x)",
             sdev->name, in_addr);
    return 1;
}

static int usb_fs_recv_packet(usb_slot_device_t *dev, uint8_t *buffer, uint32_t length, int timeout_ms)
{
    if (!dev->use_fs) return -1;
    struct usb_fs_endpoint *ep = &dev->endpoints[dev->in_index];
    (void)timeout_ms;
    /* Do not restart a transfer before consuming its completion. The old
     * short-timeout loop could discard press/release reports between calls. */
    if (!dev->in_pending) {
        size_t report_size = freepad_profiles_report_size(dev->vid, dev->pid);
        if (!report_size || report_size > length) return -1;
        dev->fs_len[dev->in_index] = (uint32_t)report_size;
        ep->nFrames = 1;
        ep->timeout = 1000;
        struct usb_fs_start start = {0};
        start.ep_index = dev->in_index;
        if (ioctl(dev->fd, USB_FS_START, &start) != 0) return -1;
        dev->in_pending = 1;
    }
    struct usb_fs_complete complete = {0};
    if (ioctl(dev->fd, USB_FS_COMPLETE, &complete) == 0) {
        dev->in_pending = 0;
        if (complete.ep_index != dev->in_index) return -1;
        if (ep->status != USB_ERR_NORMAL_COMPLETION && ep->status != USB_ERR_SHORT_XFER) return 0;
        uint32_t count = dev->fs_len[dev->in_index];
        if (count > length) count = length;
        memcpy(buffer, dev->rx_buf, count);
        return (int)count;
    }
    if (errno != EBUSY && errno != ETIMEDOUT && errno != EAGAIN && errno != EWOULDBLOCK) return -1;
    fd_set reads;
    FD_ZERO(&reads);
    FD_SET(dev->fd, &reads);
    struct timeval wait = {0, 10000};
    select(dev->fd + 1, &reads, NULL, NULL, &wait);
    return 0;
}

#endif /* __PROSPERO__ */

static void *usb_reader_worker(void *arg)
{
    usb_slot_device_t *dev = (usb_slot_device_t *)arg;
    uint8_t report[256];
    pad_state_t st;
    
    long last_data_time = now_ms();
    long last_log_t = 0;
    int consecutive_errors = 0;
    uint32_t last_buttons = UINT32_MAX;

    log_line("usb_hotplug: Reader thread started for slot %d (%s on %s)",
             dev->slot, dev->name, dev->dev_path);

    while (!dev->stop_thread) {
        long cur_time = now_ms();

        int n = 0;
#ifdef __PROSPERO__
        if (dev->use_fs) {
            n = usb_fs_recv_packet(dev, report, sizeof(report), 15);
        }
#else
        for (int i = 0; i < 6; i++) {
            if (dev->ep_fd[i] >= 0) {
                ssize_t rb = read(dev->ep_fd[i], report, sizeof(report));
                if (rb > 0) { n = (int)rb; break; }
            }
        }
        if (n <= 0 && dev->fd >= 0) {
            ssize_t rb = read(dev->fd, report, sizeof(report));
            if (rb > 0) n = (int)rb;
        }
#endif

        if (n > 0) {
            consecutive_errors = 0;
            int parsed = freepad_profiles_parse(dev->vid, dev->pid, report, (size_t)n, &st);
            if (parsed) {
                st.conn_type = dev->conn_type;
                st.vid = dev->vid;
                st.pid = dev->pid;
                snprintf(st.controller_name, sizeof(st.controller_name), "%s", dev->name);
                vpad_update(dev->slot, &st);
                last_data_time = cur_time;

                if (st.buttons != last_buttons) {
                    log_line("FreePad: report bytes=%d -> PS buttons=0x%08x", n, (unsigned)st.buttons);
                    last_buttons = st.buttons;
                }

                if (st.buttons != 0 && (cur_time - last_log_t > 300)) {
                    last_log_t = cur_time;
                    log_line("usb_hotplug: slot %d button press 0x%08x (LX=%d LY=%d)",
                             dev->slot, (unsigned)st.buttons, st.lx, st.ly);
                }
            } else if (cur_time - last_log_t > 2000) {
                last_log_t = cur_time;
                log_line("usb_hotplug: slot %d rejected report length/layout (%d bytes)", dev->slot, n);
            }
            if (!parsed) {
                /* Reject guessed layouts and release any previously held input. */
                pad_state_neutral(&st);
                st.conn_type = dev->conn_type;
                vpad_update(dev->slot, &st);
            }
        } else if (n < 0) {
            consecutive_errors++;
            if (consecutive_errors >= 50) {
                log_line("usb_hotplug: slot %d disconnected (read error n=%d, errs=%d) — terminating reader",
                         dev->slot, n, consecutive_errors);
                dev->stop_thread = 1;
                break;
            }
        } else if (cur_time - last_data_time > 3000 && cur_time - last_log_t > 3000) {
            last_log_t = cur_time;
            log_line("usb_hotplug: slot %d idle/standby (n=%d, use_fs=%d)",
                     dev->slot, n, dev->use_fs);
        }

    }


#ifdef __PROSPERO__
    if (dev->use_fs && dev->fd >= 0) {
        struct usb_fs_uninit uninit;
        memset(&uninit, 0, sizeof(uninit));
        ioctl(dev->fd, USB_FS_UNINIT, &uninit);
        dev->use_fs = 0;
    }
#endif

    log_line("usb_hotplug: Reader thread stopped for slot %d", dev->slot);
    return NULL;
}

static int is_device_claimed(const char *path)
{
    for (int i = 0; i < MAX_SLOTS; i++) {
        if (g_usb_hotplug.devices[i].active &&
            strcmp(g_usb_hotplug.devices[i].dev_path, path) == 0) {
            return 1;
        }
    }
    return 0;
}

static int find_free_slot(void)
{
    for (int i = 0; i < MAX_SLOTS; i++) {
        if (!g_usb_hotplug.devices[i].active && vpad_slot_is_free(i)) {
            return i;
        }
    }
    return -1;
}


static void probe_usb_devices(void)
{
    char path[32];

    for (int bus = 0; bus <= 5; bus++) {
        for (int dev = 1; dev <= 20; dev++) {
            snprintf(path, sizeof(path), "/dev/ugen%d.%d", bus, dev);

            if (is_device_claimed(path)) continue;

            int fd = open(path, O_RDWR | O_NONBLOCK);
            if (fd < 0) continue;

            uint16_t vid = 0, pid = 0;
#ifdef __PROSPERO__
            struct usb_device_descriptor dd;
            if (ioctl(fd, USB_GET_DEVICE_DESC, &dd) == 0) {
                vid = UGETW(dd.idVendor);
                pid = UGETW(dd.idProduct);

                /* Explicitly ignore non-gamepad classes:
                 * 0x08 = Mass Storage, 0x09 = Hub, 0x02 = CDC, 0x01 = Audio, 0x0e = Video, 0xE0 = Bluetooth */
                if (dd.bDeviceClass == 0x08 || dd.bDeviceClass == 0x09 ||
                    dd.bDeviceClass == 0x02 || dd.bDeviceClass == 0x01 ||
                    dd.bDeviceClass == 0x0e || dd.bDeviceClass == 0xE0) {
                    close(fd);
                    continue;
                }
            }
#endif
            if (vid == 0 && pid == 0) {
                close(fd);
                continue;
            }

            /* Never claim or send USB transfers to any other device. */
            if (!freepad_profiles_allowed(vid, pid)) {
                close(fd);
                continue;
            }

            const char *ctrl_name = NULL;
            ctrl_name = "FreePad USB controller";

            if (vid != 0 || pid != 0) {
                uint32_t packed = ((uint32_t)vid << 16) | pid;
                if (s_last_seen[bus][dev] != packed) {
                    s_last_seen[bus][dev] = packed;
                    log_line("usb_hotplug: Device discovered on %s (VID 0x%04x, PID 0x%04x: %s)",
                             path, vid, pid, ctrl_name);
                }
            }

            int slot = find_free_slot();
            /* The dashboard and protected-profile lifecycle manage slot zero only. */
            if (slot > 0) { close(fd); continue; }
            if (slot < 0) {
                log_line("usb_hotplug: No free vpad slot for %s (%04x:%04x)", ctrl_name, vid, pid);
                close(fd);
                return;
            }

            log_line("usb_hotplug: Claiming %s on %s (slot %d)", ctrl_name, path, slot);


            usb_slot_device_t *sdev = &g_usb_hotplug.devices[slot];
            memset(sdev, 0, sizeof(usb_slot_device_t));
            sdev->active = 1;
            sdev->slot = slot;
            sdev->fd = fd;
            snprintf(sdev->dev_path, sizeof(sdev->dev_path), "%s", path);
            sdev->vid = vid;
            sdev->pid = pid;
            sdev->name = ctrl_name;
            sdev->stop_thread = 0;

#ifdef __PROSPERO__
            if (!usb_fs_setup_device(sdev)) {
                log_line("usb_hotplug: Failed to setup usb_fs for %s, closing", path);
                close(fd);
                memset(sdev, 0, sizeof(usb_slot_device_t));
                continue;
            }
#else
            for (int ep = 1; ep <= 6; ep++) {
                char ep_path[40];
                snprintf(ep_path, sizeof(ep_path), "%s.%d", path, ep);
                sdev->ep_fd[ep - 1] = open(ep_path, O_RDONLY | O_NONBLOCK);
            }
#endif
            sdev->conn_type = CONN_USB_WIRED;

            if (access(FREEPAD_STATE_DIR "/pause", F_OK) != 0)
                vpad_add(slot, sdev->conn_type, ctrl_name);
            pthread_create(&sdev->thread, NULL, usb_reader_worker, sdev);
        }
    }
}

int usb_hotplug_init(void)
{
    memset(&g_usb_hotplug, 0, sizeof(g_usb_hotplug));
    pthread_mutex_init(&g_usb_hotplug.lock, NULL);
    log_line("usb_hotplug: Initialized USB hotplug manager");
    return 1;
}

int usb_hotplug_freepad_connected(void)
{
    int connected = 0;
    pthread_mutex_lock(&g_usb_hotplug.lock);
    for (int i = 0; i < MAX_SLOTS; i++) {
        if (g_usb_hotplug.devices[i].active && !g_usb_hotplug.devices[i].stop_thread &&
            freepad_profiles_allowed(g_usb_hotplug.devices[i].vid, g_usb_hotplug.devices[i].pid))
            connected = 1;
    }
    pthread_mutex_unlock(&g_usb_hotplug.lock);
    return connected;
}

void usb_hotplug_poll(long now)
{
    /* Scan every 1500 ms */
    if (now - g_usb_hotplug.last_scan_time < 1500) return;
    g_usb_hotplug.last_scan_time = now;

    int dead[MAX_SLOTS];
    int n_dead = 0;

    pthread_mutex_lock(&g_usb_hotplug.lock);
    for (int i = 0; i < MAX_SLOTS; i++) {
        if (g_usb_hotplug.devices[i].active) {
            if (access(g_usb_hotplug.devices[i].dev_path, F_OK) != 0 ||
                g_usb_hotplug.devices[i].stop_thread) {
                log_line("usb_hotplug: Controller disconnected from %s (slot %d)",
                         g_usb_hotplug.devices[i].dev_path, i);
                g_usb_hotplug.devices[i].stop_thread = 1;
                dead[n_dead++] = i;
            }
        }
    }
    pthread_mutex_unlock(&g_usb_hotplug.lock);

    /* Join and release resources WITHOUT holding the hotplug lock */
    for (int d = 0; d < n_dead; d++) {
        int i = dead[d];
        pthread_join(g_usb_hotplug.devices[i].thread, NULL);

        pthread_mutex_lock(&g_usb_hotplug.lock);
        int bus = 0, dev_idx = 0;
        if (sscanf(g_usb_hotplug.devices[i].dev_path, "/dev/ugen%d.%d", &bus, &dev_idx) == 2) {
            if (bus >= 0 && bus < 6 && dev_idx >= 0 && dev_idx < 21) {
                s_last_seen[bus][dev_idx] = 0;
            }
        }
#ifndef __PROSPERO__
        for (int e = 0; e < 6; e++) {
            if (g_usb_hotplug.devices[i].ep_fd[e] >= 0) {
                close(g_usb_hotplug.devices[i].ep_fd[e]);
                g_usb_hotplug.devices[i].ep_fd[e] = -1;
            }
        }
#endif
        if (g_usb_hotplug.devices[i].fd >= 0) {
            close(g_usb_hotplug.devices[i].fd);
            g_usb_hotplug.devices[i].fd = -1;
        }
        vpad_remove(i);
        memset(&g_usb_hotplug.devices[i], 0, sizeof(usb_slot_device_t));
        pthread_mutex_unlock(&g_usb_hotplug.lock);
    }

    pthread_mutex_lock(&g_usb_hotplug.lock);
    probe_usb_devices();
    pthread_mutex_unlock(&g_usb_hotplug.lock);
}

void usb_hotplug_cleanup(void)
{
    pthread_mutex_lock(&g_usb_hotplug.lock);
    for (int i = 0; i < MAX_SLOTS; i++) {
        if (g_usb_hotplug.devices[i].active) {
            g_usb_hotplug.devices[i].stop_thread = 1;
        }
    }
    pthread_mutex_unlock(&g_usb_hotplug.lock);

    for (int i = 0; i < MAX_SLOTS; i++) {
        if (g_usb_hotplug.devices[i].active) {
            pthread_join(g_usb_hotplug.devices[i].thread, NULL);
#ifndef __PROSPERO__
            for (int e = 0; e < 6; e++) {
                if (g_usb_hotplug.devices[i].ep_fd[e] >= 0) {
                    close(g_usb_hotplug.devices[i].ep_fd[e]);
                    g_usb_hotplug.devices[i].ep_fd[e] = -1;
                }
            }
#endif
            if (g_usb_hotplug.devices[i].fd >= 0) {
                close(g_usb_hotplug.devices[i].fd);
                g_usb_hotplug.devices[i].fd = -1;
            }
            vpad_remove(i);
            memset(&g_usb_hotplug.devices[i], 0, sizeof(usb_slot_device_t));
        }
    }
}
