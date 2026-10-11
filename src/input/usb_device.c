#include "input/usb_device.h"
#include "input/controller_router.h"
#include "ps5/virtual_controller.h"
#include "core/logging.h"
#include "core/time_utils.h"
#include "core/paths.h"
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
/* Report the protocol actually selected, not the mode requested on an adapter
 * or the controller paired behind it. Those can share the same USB identity. */
static const char *usb_protocol_label(usb_controller_type_t driver, int gip)
{
    switch (driver) {
    case CTRL_XBOX_XINPUT:
    case CTRL_8BITDO_ADAPTER: return gip ? "Xbox GIP" : "XInput";
    case CTRL_NINTENDO_SWITCH_PRO: return "Switch Pro";
    case CTRL_SONY_DS4: return "DualShock 4";
    case CTRL_SONY_DS3: return "DualShock 3";
    case CTRL_GENERIC_HID: return "Generic HID";
    case CTRL_PDP_FACEOFF: return "Configured HID";
    default: return "Custom USB layout";
    }
}

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
    int                   in_ready, out_ready;
    uint32_t              in_requested;
    long                  last_read_error_log;
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
    uint8_t raw[512] = {0};
    desc.ugd_data = raw; desc.ugd_maxlen = sizeof(raw);
    if (ioctl(dev->fd,USB_GET_FULL_DESC,&desc) != 0) return 0;
    size_t length = desc.ugd_actlen;
    usb_input_interface_t selected;
    if (!length || length > sizeof(raw) ||
        !usb_select_input_interface(raw, length, dev->driver, &selected)) return 0;
    dev->interface = selected.interface;
    dev->gip = selected.gip;
    dev->out_address = selected.out_address;
    return selected.in_address;
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
    if (sdev->out_address && (sdev->driver==CTRL_XBOX_XINPUT || sdev->driver==CTRL_8BITDO_ADAPTER || sdev->driver==CTRL_NINTENDO_SWITCH_PRO)) {
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

/* Completions are device-wide, including while sending initialization or a
 * Guide ACK. Leave completed input untouched until the reader consumes it. */
static int usb_fs_collect(usb_slot_device_t *dev)
{
    struct usb_fs_complete complete={0};
    if (ioctl(dev->fd,USB_FS_COMPLETE,&complete)) {
        if (errno==EBUSY || errno==EAGAIN || errno==EWOULDBLOCK || errno==ETIMEDOUT) return 0;
        log_line("usb_hotplug: %04x:%04x USB_FS_COMPLETE failed (errno=%d: %s)",
                 dev->vid,dev->pid,errno,strerror(errno));
        return -1;
    }
    if (complete.ep_index==dev->in_index) {
        dev->in_pending=0; dev->in_ready=1;
    } else if (complete.ep_index==1) dev->out_ready=1;
    else {
        log_line("usb_hotplug: unexpected USB completion index=%u",complete.ep_index);
        return -1;
    }
    return 1;
}

static int usb_fs_start_input(usb_slot_device_t *dev)
{
    if (dev->in_pending || dev->in_ready) return 1;
    size_t size=librepad_profiles_report_size(dev->vid,dev->pid);
    if (!size || size>sizeof(dev->rx_buf)) return 0;
    dev->in_requested=dev->fs_len[dev->in_index]=(uint32_t)size;
    struct usb_fs_endpoint *ep=&dev->endpoints[dev->in_index];
    ep->nFrames=1; ep->timeout=1000;
    struct usb_fs_start start={0}; start.ep_index=dev->in_index;
    if (ioctl(dev->fd,USB_FS_START,&start)) {
        long now=now_ms();
        if (!dev->last_read_error_log || now-dev->last_read_error_log>=3000) {
            dev->last_read_error_log=now;
            log_line("usb_hotplug: %04x:%04x USB_FS_START IN=0x%02x failed (requested=%u, errno=%d: %s)",
                     dev->vid,dev->pid,dev->in_address,dev->in_requested,errno,strerror(errno));
        }
        return 0;
    }
    dev->in_pending=1;
    return 1;
}

static int usb_output(usb_slot_device_t *dev, const uint8_t *data, size_t length)
{
    if (!dev->has_output || !length || length>sizeof(dev->tx_buf)) return 0;
    memcpy(dev->tx_buf,data,length); dev->fs_len[1]=(uint32_t)length;
    dev->out_ready=0;
    dev->endpoints[1].nFrames=1; dev->endpoints[1].timeout=250;
    struct usb_fs_start start={0}; start.ep_index=1;
    if (ioctl(dev->fd,USB_FS_START,&start)) return 0;
    long deadline=now_ms()+300;
    while (now_ms()<deadline) {
        if (usb_fs_collect(dev)<0) break;
        if (dev->out_ready) {
            int status=dev->endpoints[1].status;
            int ok=status==USB_ERR_NORMAL_COMPLETION && dev->fs_len[1]==length;
            if (!ok) log_line("usb_hotplug: %04x:%04x OUT=0x%02x failed (status=%d, actual=%u, requested=%zu)",
                              dev->vid,dev->pid,dev->out_address,status,dev->fs_len[1],length);
            return ok;
        }
        usleep(1000);
    }
    /* Keep buffers alive until UNINIT; do not reuse an endpoint whose canceled
     * completion may still be queued. Input may continue for optional LEDs. */
    struct usb_fs_stop stop={0}; stop.ep_index=1;
    ioctl(dev->fd,USB_FS_STOP,&stop);
    dev->has_output=0;
    log_line("usb_hotplug: %04x:%04x OUT=0x%02x did not complete; output disabled",dev->vid,dev->pid,dev->out_address);
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
    if (ioctl(dev->fd,USB_DO_REQUEST,&request)) {
        log_line("usb_hotplug: %04x:%04x report descriptor read failed (interface=%u, errno=%d: %s)",
                 dev->vid, dev->pid, dev->interface, errno, strerror(errno));
        return 0;
    }
    if (request.ucr_actlen>sizeof(descriptor) ||
        !generic_descriptor_is_gamepad(descriptor,(int)request.ucr_actlen)) {
        log_line("usb_hotplug: %04x:%04x report descriptor is not a gamepad (interface=%u, bytes=%u)",
                 dev->vid, dev->pid, dev->interface, (unsigned)request.ucr_actlen);
        return 0;
    }
    pad_state_reset(&dev->generic_state);
    int ready = generic_setup(dev->generic.bytes,descriptor,(int)request.ucr_actlen,
        dev->vid,dev->pid,LIBREPAD_STATE_DIR "/maps");
    if (!ready) log_line("usb_hotplug: %04x:%04x gamepad descriptor could not be parsed (bytes=%u)",
                         dev->vid, dev->pid, (unsigned)request.ucr_actlen);
    return ready;
}

static int wake_device(usb_slot_device_t *dev)
{
    if ((dev->driver==CTRL_XBOX_XINPUT || dev->driver==CTRL_8BITDO_ADAPTER) && !dev->gip) {
        /* xpad arms the input URB before its optional vendor request. XInput
         * LED commands use the OUT endpoint, not HID SET_REPORT on EP0. */
        if (!usb_fs_start_input(dev)) return 0;
        usb_init_controller_handshake(dev->fd,dev->driver,dev->interface);
        const uint8_t led[]={1,3,2};
        int sent=usb_output(dev,led,sizeof(led));
        log_line("usb_ctrl: XInput input armed; interrupt LED %s (OUT=0x%02x)",sent ? "sent" : "unavailable",dev->out_address);
        return 1; /* The optional LED must not prevent input. */
    }
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
    if ((dev->driver==CTRL_XBOX_XINPUT || dev->driver==CTRL_8BITDO_ADAPTER) && dev->gip) {
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
    return usb_init_controller_handshake(dev->fd,dev->driver,dev->interface)==0;
}

static int usb_fs_recv_packet(usb_slot_device_t *dev, uint8_t *buffer, uint32_t length, int timeout_ms)
{
    if (!dev->use_fs) return -1;
    struct usb_fs_endpoint *ep = &dev->endpoints[dev->in_index];
    (void)timeout_ms;
    if (!usb_fs_start_input(dev)) return -1;
    if (!dev->in_ready && usb_fs_collect(dev)<0) return -1;
    if (dev->in_ready) {
        dev->in_ready=0;
        if (ep->status==USB_ERR_TIMEOUT) return 0;
        if (ep->status != USB_ERR_NORMAL_COMPLETION && ep->status != USB_ERR_SHORT_XFER) {
            long now=now_ms();
            if (!dev->last_read_error_log || now-dev->last_read_error_log>=3000) {
                dev->last_read_error_log=now;
                log_line("usb_hotplug: %04x:%04x IN=0x%02x transfer failed (status=%d, frames=%u, requested=%u)",
                         dev->vid,dev->pid,dev->in_address,ep->status,ep->aFrames,dev->in_requested);
            }
            return -1;
        }
        uint32_t count = dev->fs_len[dev->in_index];
        if (count>dev->in_requested || count>length || count>sizeof(dev->rx_buf)) return -1;
        memcpy(buffer, dev->rx_buf, count);
        return (int)count;
    }
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
            int parsed = librepad_profiles_parse(dev->vid, dev->pid, report, (size_t)n, &st);
            if (dev->driver==CTRL_XBOX_XINPUT || dev->driver==CTRL_8BITDO_ADAPTER) {
                parsed=usb_parse_stateful_report(dev->driver,report,(size_t)n,&dev->protocol_state,&st);
                if (parsed) librepad_profiles_remap(dev->vid,dev->pid,&st);
            }
            if (dev->driver==CTRL_GENERIC_HID &&
                generic_profile.parse(dev->generic.bytes,report,n,&dev->generic_state)!=PARSE_NONE) {
                pad_state_neutral(&st);
                st.buttons=dev->generic_state.buttons;
                st.lx=dev->generic_state.lx; st.ly=dev->generic_state.ly;
                st.rx=dev->generic_state.rx; st.ry=dev->generic_state.ry;
                st.l2=dev->generic_state.l2; st.r2=dev->generic_state.r2;
                librepad_profiles_remap(dev->vid,dev->pid,&st); parsed=1;
            }
#ifdef __PROSPERO__
            if ((dev->driver==CTRL_XBOX_XINPUT || dev->driver==CTRL_8BITDO_ADAPTER) && dev->gip && report[0]==7 && n>=5 && (report[1]&0x10)) {
                uint8_t ack[]={1,0x20,report[2],9,0,7,0x20,2,0,0,0,0,0};
                if (!usb_output(dev,ack,sizeof(ack))) { dev->stop_thread=1; break; }
            }
#endif
            if (parsed) {
                st.conn_type = dev->conn_type;
                st.vid = dev->vid;
                st.pid = dev->pid;
                snprintf(st.controller_name, sizeof(st.controller_name), "%s", dev->name);
                controller_update(dev, &st);
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
                controller_update(dev, &st);
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
        if (!g_usb_hotplug.devices[i].active) {
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
            if (fd < 0) {
                if (errno == ENOENT || errno == ENXIO) s_last_seen[bus][dev] = 0;
                continue;
            }

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

            /* Do not claim the idle receiver, even if a saved profile overrides
             * its ID. Keep polling so re-enumeration as a gamepad is detected. */
            if (usb_receiver_is_idle(vid, pid)) {
                uint32_t packed = ((uint32_t)vid << 16) | pid;
                if (s_last_seen[bus][dev] != packed) {
                    s_last_seen[bus][dev] = packed;
                    log_line("usb_hotplug: %04x:%04x on %s is an idle 8BitDo Adapter 2; waiting for a paired controller. Check adapter LED is solid, then hold View+Up (Xbox) or Share+Up (PS4) for 3 seconds for XInput mode", vid, pid, path);
                }
                close(fd); continue;
            }

            /* Never claim or send USB transfers to any other device. */
            /* The Xbox radio backend owns these receivers, even if a custom
             * USB profile has accidentally allowlisted one as generic HID. */
            if (vid == 0x045e && (pid == 0x02e6 || pid == 0x02fe)) {
                close(fd); continue;
            }
            if (!librepad_profiles_allowed(vid, pid)) {
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
            sdev->driver=librepad_profiles_driver(vid,pid);
            pad_state_neutral(&sdev->protocol_state);

#ifdef __PROSPERO__
            sdev->in_address=find_usb_input_endpoint(sdev);
            if (!sdev->in_address) {
                log_line("usb_hotplug: %04x:%04x has no supported HID/XInput interface (driver=%d); for 8BitDo Adapter 2 select XInput mode, then replug", vid, pid, sdev->driver);
                close(fd); memset(sdev,0,sizeof(*sdev)); continue;
            }
            log_line("usb_hotplug: %04x:%04x selected protocol=%s driver=%d interface=%u IN=0x%02x OUT=0x%02x GIP=%d",
                     vid, pid, usb_protocol_label(sdev->driver, sdev->gip), sdev->driver,
                     sdev->interface, sdev->in_address, sdev->out_address, sdev->gip);
            if (sdev->driver==CTRL_GENERIC_HID && !setup_generic(sdev)) {
                log_line("usb_hotplug: %04x:%04x HID descriptor rejected; for 8BitDo Adapter 2 select XInput mode, then replug", vid, pid);
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

            sdev->slot = controller_attach(sdev, sdev->conn_type,
                vid == 0x0e6f && pid == 0x0184 ? "PDP Switch controller" : ctrl_name);
            if (sdev->slot < 0) {
#ifdef __PROSPERO__
                struct usb_fs_uninit uninit={0}; ioctl(fd,USB_FS_UNINIT,&uninit);
#endif
                close(fd); memset(sdev,0,sizeof(*sdev)); continue;
            }
            int thread_error=pthread_create(&sdev->thread, NULL, usb_reader_worker, sdev);
            if (thread_error) {
                log_line("usb_hotplug: Reader creation failed (%d), closing %s", thread_error, path);
#ifdef __PROSPERO__
                struct usb_fs_uninit uninit={0}; ioctl(fd,USB_FS_UNINIT,&uninit);
#else
                for (int e=0; e<6; e++) if (sdev->ep_fd[e]>=0) close(sdev->ep_fd[e]);
#endif
                controller_detach(sdev);
                close(fd); memset(sdev,0,sizeof(*sdev)); continue;
            }
#ifdef __PROSPERO__
            notify_ps5("LibrePadPS5 USB: %s (%04x:%04x)",
                       usb_protocol_label(sdev->driver, sdev->gip), vid, pid);
#endif
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

int usb_hotplug_librepad_connected(void)
{
    int connected = 0;
    pthread_mutex_lock(&g_usb_hotplug.lock);
    for (int i = 0; i < MAX_SLOTS; i++) {
        if (g_usb_hotplug.devices[i].active && !g_usb_hotplug.devices[i].stop_thread &&
            librepad_profiles_allowed(g_usb_hotplug.devices[i].vid, g_usb_hotplug.devices[i].pid))
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
        controller_detach(&g_usb_hotplug.devices[i]);
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
            controller_detach(&g_usb_hotplug.devices[i]);
            memset(&g_usb_hotplug.devices[i], 0, sizeof(usb_slot_device_t));
        }
    }
}
