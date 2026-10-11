#include "input/usb_device.h"
#include "ps5/virtual_controller.h"
#include "core/logging.h"
#include "core/time_utils.h"
#include "core/legacy_paths.h"
#include "input/controller_profiles.h"
#include "bluetooth/controller_host.h"
#include "profiles.h"

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
    usb_controller_type_t driver;
    union { max_align_t alignment; unsigned char bytes[PROFILE_CTX_MAX]; } generic;
    pad_state generic_state;
    pad_state_t protocol_state;

#ifdef __PROSPERO__
    struct usb_fs_endpoint endpoints[4];
    uint8_t               in_index;
    uint8_t               out_address;
    uint8_t               tx_buf[64];
    int                   has_output;
    int                   gip;
    uint8_t               interface, in_address;
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

/* Select input and output endpoints from one non-boot HID or XInput interface.
 * Dedicated drivers issue their bounded wake-up commands after setup. */
static uint8_t find_usb_input_endpoint(usb_slot_device_t *dev)
{
    struct usb_gen_descriptor desc = {0};
    uint8_t raw[512] = {0}, first = 0;
    desc.ugd_data = raw; desc.ugd_maxlen = sizeof(raw);
    if (ioctl(dev->fd,USB_GET_FULL_DESC,&desc) != 0) return 0;
    int length = desc.ugd_actlen;
    if (length <= 0 || length > (int)sizeof(raw)) return 0;
    int suitable=0, chosen=-1;
    for (int offset = 0; offset+2 <= length;) {
        int entry = raw[offset];
        if (entry < 2 || offset+entry > length) break;
        if (raw[offset+1]==4 && entry>=9) {
            if (chosen>=0 && first) break;
            int hid=raw[offset+5]==3 && raw[offset+6]==0 && raw[offset+7]==0;
            int gip=raw[offset+5]==0xff && raw[offset+6]==0x47 && raw[offset+7]==0xd0;
            int xinput=raw[offset+5]==0xff && raw[offset+6]==0x5d && raw[offset+7]==1;
            suitable=raw[offset+3]==0 && (hid || ((gip || xinput) && dev->driver==CTRL_XBOX_XINPUT));
            if (suitable) {
                chosen=raw[offset+2]; dev->interface=(uint8_t)chosen; dev->gip=gip;
                dev->out_address=0;
            }
        }
        if (suitable && raw[offset+1] == 5 && entry >= 7) {
            uint8_t address = raw[offset+2], type = raw[offset+3] & 3;
            if ((address & 0x80) && (type == 3 || type == 2)) {
                if (address == 0x81 || !first) first = address;
            }
            if (!(address & 0x80) && (type == 3 || type == 2) && !dev->out_address) dev->out_address=address;
        }
        offset += entry;
    }
    return first;
}

static int usb_fs_setup_device(usb_slot_device_t *sdev)
{
    int iface = sdev->interface;
    ioctl(sdev->fd, USB_CLAIM_INTERFACE, &iface);

    int config = 0;
    if (ioctl(sdev->fd, USB_GET_CONFIG, &config) == 0 && (config & 0xff) == 0xff) {
        config = 0;
        ioctl(sdev->fd, USB_SET_CONFIG, &config);
    }

    uint8_t in_addr = sdev->in_address;
    if (!in_addr) return 0;

    struct usb_fs_init init;
    memset(sdev->endpoints, 0, sizeof(sdev->endpoints));
    memset(&init, 0, sizeof(init));
    init.pEndpoints = sdev->endpoints;
    init.ep_index_max = 2;

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
    if (sdev->out_address && (sdev->driver==CTRL_XBOX_XINPUT || sdev->driver==CTRL_NINTENDO_SWITCH_PRO)) {
        struct usb_fs_open output={0};
        output.max_bufsize=64; output.max_frames=1; output.ep_index=1; output.ep_no=sdev->out_address;
        sdev->has_output=ioctl(sdev->fd,USB_FS_OPEN,&output)==0;
        sdev->fs_frame[1]=sdev->tx_buf;
        sdev->endpoints[1].ppBuffer=&sdev->fs_frame[1];
        sdev->endpoints[1].pLength=&sdev->fs_len[1];
        sdev->endpoints[1].nFrames=1;
        sdev->endpoints[1].flags=USB_FS_FLAG_FORCE_SHORT;
    }
    log_line("usb_hotplug: %s claimed via USB_FS (IN=0x%02x)",
             sdev->name, in_addr);
    return 1;
}

static int usb_output(usb_slot_device_t *dev, const uint8_t *data, size_t length)
{
    if (!dev->has_output || !length || length>sizeof(dev->tx_buf)) return 0;
    memcpy(dev->tx_buf,data,length); dev->fs_len[1]=(uint32_t)length;
    dev->endpoints[1].nFrames=1; dev->endpoints[1].timeout=250;
    struct usb_fs_start start={0}; start.ep_index=1;
    if (ioctl(dev->fd,USB_FS_START,&start)) return 0;
    long deadline=now_ms()+300;
    while (now_ms()<deadline) {
        struct usb_fs_complete complete={0};
        if (ioctl(dev->fd,USB_FS_COMPLETE,&complete)==0)
            return complete.ep_index==1 && dev->endpoints[1].status==USB_ERR_NORMAL_COMPLETION;
        if (errno!=EBUSY && errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=ETIMEDOUT) return 0;
        usleep(1000);
    }
    return 0;
}

static int setup_generic(usb_slot_device_t *dev)
{
    uint8_t descriptor[1024]; struct usb_ctl_request request={0};
    request.ucr_request.bmRequestType=0x81;
    request.ucr_request.bRequest=6;
    USETW(request.ucr_request.wValue,0x2200);
    USETW(request.ucr_request.wIndex,dev->interface);
    USETW(request.ucr_request.wLength,sizeof(descriptor));
    request.ucr_data=descriptor;
    request.ucr_flags=USB_SHORT_XFER_OK;
    if (ioctl(dev->fd,USB_DO_REQUEST,&request)) return 0;
    if (request.ucr_actlen>sizeof(descriptor) ||
        !generic_descriptor_is_gamepad(descriptor,(int)request.ucr_actlen)) return 0;
    pad_state_reset(&dev->generic_state);
    return generic_setup(dev->generic.bytes,descriptor,(int)request.ucr_actlen,
        dev->vid,dev->pid,FREEPAD_STATE_DIR "/maps");
}

static int wake_device(usb_slot_device_t *dev)
{
    if (dev->driver==CTRL_NINTENDO_SWITCH_PRO) {
        static const uint8_t usb_commands[]={2,3,2,4};
        for (size_t i=0; i<sizeof(usb_commands); i++) {
            const uint8_t command[]={0x80,usb_commands[i]};
            if (!usb_output(dev,command,sizeof(command))) return 0;
            usleep(20000);
        }
        const uint8_t mode[]={1,0,0,1,0x40,0x40,0,1,0x40,0x40,3,0x30};
        return usb_output(dev,mode,sizeof(mode));
    }
    if (dev->driver==CTRL_XBOX_XINPUT && dev->gip) {
        const uint8_t power[]={0x05,0x20,0x00,0x01,0x00};
        if (!usb_output(dev,power,sizeof(power))) return 0;
        if (dev->vid==0x045e && (dev->pid==0x02ea || dev->pid==0x0b00)) {
            const uint8_t initialize[]={5,0x20,1,0x0f,6};
            if (!usb_output(dev,initialize,sizeof(initialize))) return 0;
        }
        const uint8_t light[]={0x0a,0x20,2,3,0,1,0x14};
        const uint8_t authenticate[]={6,0x20,3,2,1,0};
        return usb_output(dev,light,sizeof(light)) && usb_output(dev,authenticate,sizeof(authenticate));
    }
    return usb_init_controller_handshake(dev->fd,dev->driver)==0;
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
            if (dev->driver==CTRL_XBOX_XINPUT || dev->driver==CTRL_8BITDO_ADAPTER) {
                parsed=usb_parse_stateful_report(dev->driver,report,(size_t)n,&dev->protocol_state,&st);
                if (parsed) freepad_profiles_remap(dev->vid,dev->pid,&st);
            }
            if (dev->driver==CTRL_GENERIC_HID &&
                generic_profile.parse(dev->generic.bytes,report,n,&dev->generic_state)!=PARSE_NONE) {
                pad_state_neutral(&st);
                st.buttons=dev->generic_state.buttons;
                st.lx=dev->generic_state.lx; st.ly=dev->generic_state.ly;
                st.rx=dev->generic_state.rx; st.ry=dev->generic_state.ry;
                st.l2=dev->generic_state.l2; st.r2=dev->generic_state.r2;
                freepad_profiles_remap(dev->vid,dev->pid,&st); parsed=1;
            }
#ifdef __PROSPERO__
            if (dev->driver==CTRL_XBOX_XINPUT && report[0]==7 && n>=5 && (report[1]&0x10)) {
                uint8_t ack[]={1,0x20,report[2],9,0,7,0x20,2,0,0,0,0,0};
                if (!usb_output(dev,ack,sizeof(ack))) { dev->stop_thread=1; break; }
            }
#endif
            if (parsed) {
                st.conn_type = dev->conn_type;
                st.vid = dev->vid;
                st.pid = dev->pid;
                snprintf(st.controller_name, sizeof(st.controller_name), "%s", dev->name);
                vpad_update(dev->slot, &st);
                last_data_time = cur_time;

                if (st.buttons != last_buttons) {
                    log_line("LibrePadPS5: report bytes=%d -> PS buttons=0x%08x", n, (unsigned)st.buttons);
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
                log_line("usb_hotplug: slot %d disconnected (read error n=%d, errs=%d): terminating reader",
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
            ctrl_name = "LibrePadPS5 controller";

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
            sdev->driver=freepad_profiles_driver(vid,pid);
            pad_state_neutral(&sdev->protocol_state);

#ifdef __PROSPERO__
            sdev->in_address=find_usb_input_endpoint(sdev);
            if (!sdev->in_address) { close(fd); memset(sdev,0,sizeof(*sdev)); continue; }
            if (sdev->driver==CTRL_GENERIC_HID && !setup_generic(sdev)) {
                close(fd); memset(sdev,0,sizeof(*sdev)); continue;
            }
            if (!usb_fs_setup_device(sdev)) {
                log_line("usb_hotplug: Failed to setup usb_fs for %s, closing", path);
                close(fd);
                memset(sdev, 0, sizeof(usb_slot_device_t));
                continue;
            }
            if (!wake_device(sdev)) {
                struct usb_fs_uninit uninit={0}; ioctl(fd,USB_FS_UNINIT,&uninit);
                close(fd); memset(sdev,0,sizeof(*sdev)); continue;
            }
#else
            for (int ep = 1; ep <= 6; ep++) {
                char ep_path[40];
                snprintf(ep_path, sizeof(ep_path), "%s.%d", path, ep);
                sdev->ep_fd[ep - 1] = open(ep_path, O_RDONLY | O_NONBLOCK);
            }
#endif
            sdev->conn_type = CONN_USB_WIRED;

            int thread_error=pthread_create(&sdev->thread, NULL, usb_reader_worker, sdev);
            if (thread_error) {
                log_line("usb_hotplug: Reader creation failed (%d), closing %s", thread_error, path);
#ifdef __PROSPERO__
                struct usb_fs_uninit uninit={0}; ioctl(fd,USB_FS_UNINIT,&uninit);
#else
                for (int e=0; e<6; e++) if (sdev->ep_fd[e]>=0) close(sdev->ep_fd[e]);
#endif
                close(fd); memset(sdev,0,sizeof(*sdev)); continue;
            }
            if (access(FREEPAD_STATE_DIR "/pause", F_OK) != 0)
                vpad_add(slot, sdev->conn_type, ctrl_name);
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
    if (bluetooth_enabled()) return;
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
