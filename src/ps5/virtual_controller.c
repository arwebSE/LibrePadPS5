#include "ps5/virtual_controller.h"
#include "ps5/system_bridge.h"
#include "core/logging.h"
#include "core/time_utils.h"
#include "core/legacy_paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define VIRTUAL_DEVICE_DUALSENSE 3
#define T_IDENTIFY               3000   /* ms to wait for DEVICE_ADDED */
#define T_AMBIGUITY               300   /* ms after match before committing */
#define LINE_MAX_LEN             1024

#ifdef __PROSPERO__
extern int32_t sceUserServiceInitialize(void *params);
extern int32_t sceUserServiceGetInitialUser(int32_t *user);
extern int32_t sceUserServiceGetForegroundUser(int32_t *user);
extern int32_t sceUserServiceGetLoginUserIdList(int32_t list[4]);
extern int32_t sceUserServiceGetUserStatus(int32_t user_id, int32_t *status);
extern int32_t scePadInit(void);
extern int32_t scePadGetHandle(int32_t user_id, int32_t port_type, int32_t index);
extern int32_t scePadReadState(int32_t handle, PadData *data);
extern int32_t scePadSetProcessPrivilege(int32_t privilege);
extern int32_t scePadVirtualDeviceAddDevice(void *param, int32_t deviceType);
extern int32_t scePadVirtualDeviceInsertData(int32_t handle, const void *data);
extern int32_t scePadVirtualDeviceDeleteDevice(int32_t handle);
extern uint64_t sceKernelGetProcessTime(void);
#endif

typedef int32_t (*mbus_bind_fn)(uint64_t device_id, int32_t user);
typedef int32_t (*mbus_disconnect_fn)(uint64_t device_id);
typedef int32_t (*mbus_unbind_fn)(uint64_t device_id, int32_t user);

typedef struct {
    vpad_status_t   status;
    int             remove_when_found;
    int32_t         handle;
    int32_t         alt_handle;
    uint64_t        device_id;
    int32_t         user_id;
    pad_conn_type_t conn_type;
    char            name[64];
    uint8_t         battery_level;
    uint8_t         battery_charging;
    uint32_t        packets_injected;
    long            connected_time;
    long            last_update_time;
    pad_state_t     pad_state;      /* Latch state for continuous 250Hz injection */
    int             consecutive_insert_errors;
    int             ps_pending_on_ready;
    long            ps_until;
} internal_slot_t;

static internal_slot_t g_slots[MAX_SLOTS];
static pthread_mutex_t g_vpad_mutex = PTHREAD_MUTEX_INITIALIZER;
static mbus_bind_fn g_bind = NULL;
static mbus_disconnect_fn g_disconnect = NULL;
static mbus_unbind_fn g_unbind = NULL;
static int g_ready = 0;
static int32_t g_active_user = -1;
static int32_t g_protected_user;
static int32_t g_controller_user;

/* Log reader state */
static int g_pending = -1;
static long g_t_add = 0, g_t_match = 0;
static int g_matches = 0;
static uint64_t g_match_dev = 0;
static int g_klog_handle = -1;
static int g_log_fd = -1;
static int g_log_is_dev = 0;
static char g_line[LINE_MAX_LEN];
static size_t g_line_len = 0;

static inline int plausible_handle(int32_t h)
{
    return h > 0 && h < 64;
}



static void mbus_disconnect_device(uint64_t device_id, int32_t user_id)
{
    (void)user_id;
    if (device_id == 0) return;
#ifdef __PROSPERO__
    if (g_disconnect) {
        int32_t r = g_disconnect(device_id);
        log_line("vpad: local sceMbusDisconnectDevice(0x%llx) -> %d",
                 (unsigned long long)device_id, (int)r);
    }
#else
    (void)device_id;
#endif
}



static int is_user_logged_in(int32_t uid)
{
    if (uid <= 0) return 0;
#ifdef __PROSPERO__
    /* Use the login list rather than assuming a firmware-specific status enum. */
    int32_t users[4] = {-1, -1, -1, -1};
    if (sceUserServiceGetLoginUserIdList(users) != 0) return 0;
    for (int i = 0; i < 4; i++) {
        if (users[i] == uid) return 1;
    }
    return 0;
#endif
    return 0;
}

static int32_t get_user_id_for_slot(int slot)
{
#ifdef __PROSPERO__
    if (g_controller_user > 0) {
        if (g_controller_user == g_protected_user || !is_user_logged_in(g_controller_user) ||
            vpad_native_controller_connected(g_controller_user) == 1) return -1;
        return g_controller_user;
    }
    int32_t fg_user = -1;
    sceUserServiceGetForegroundUser(&fg_user);

    int32_t list[4] = {-1, -1, -1, -1};
    sceUserServiceGetLoginUserIdList(list);

    int32_t init_user = -1;
    sceUserServiceGetInitialUser(&init_user);

    /* Check if the foreground user already has an active physical DualSense */
    int native_pad_on_fg = 0;
    if (fg_user > 0 && is_user_logged_in(fg_user)) {
        native_pad_on_fg = fg_user == g_protected_user || vpad_native_controller_connected(fg_user) == 1;
    }

    /* Do not replace the native controller's profile assignment. A cached
     * handle alone is not proof that a native controller is connected. */
    if (native_pad_on_fg) {
        for (int i = 0; i < 4; i++) {
            if (list[i] > 0 && list[i] != fg_user && list[i] != g_protected_user && is_user_logged_in(list[i]) &&
                vpad_native_controller_connected(list[i]) != 1) return list[i];
        }
        log_line("FreePad: native controller occupies foreground profile; no free signed-in profile");
        return -1;
    }

    /* Slot 0:
     * If a native controller is already active on the foreground user,
     * assign Slot 0 to list[1] (Player 2) so it does not hijack the DualSense!
     * If no native controller on fg_user, Slot 0 takes fg_user (Player 1).
     */
    if (slot == 0) {
        if (native_pad_on_fg && list[1] > 0 && is_user_logged_in(list[1])) {
            log_line("vpad: slot 0 -> user 0x%08x (Player 2, native pad active on fg 0x%08x)",
                     (unsigned)list[1], (unsigned)fg_user);
            return list[1];
        }
        if (fg_user > 0 && is_user_logged_in(fg_user)) {
            log_line("vpad: slot 0 assigned to active foreground user 0x%08x", (unsigned)fg_user);
            return fg_user;
        }
        if (list[0] > 0 && is_user_logged_in(list[0])) {
            log_line("vpad: slot 0 assigned to login user list[0] 0x%08x", (unsigned)list[0]);
            return list[0];
        }
        if (init_user > 0 && is_user_logged_in(init_user)) {
            log_line("vpad: slot 0 assigned to initial user 0x%08x", (unsigned)init_user);
            return init_user;
        }
        log_line("vpad: slot 0 no user logged in — interactive profile assignment required");
        return -1;
    }

    /* Multi-slot (Slots 1..3):
     * If native pad is on foreground user, shift indexes by 1: Slot 1 -> list[2], Slot 2 -> list[3]...
     * Otherwise: Slot 1 -> list[1], Slot 2 -> list[2]...
     */
    int target_idx = native_pad_on_fg ? (slot + 1) : slot;
    if (target_idx < 4 && list[target_idx] > 0 && is_user_logged_in(list[target_idx])) {
        log_line("vpad: slot %d assigned to login user 0x%08x (index %d)",
                 slot, (unsigned)list[target_idx], target_idx);
        return list[target_idx];
    }
#endif
    return -1;
}

static int open_kernel_log(void)
{
    /* Try local klog server (port 3232, kstuff-1.13) first */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd >= 0) {
        struct sockaddr_in sin;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_port = htons(3232);
        sin.sin_addr.s_addr = inet_addr("127.0.0.1");
        if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) == 0) {
            fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
            g_log_is_dev = 0;
            return fd;
        }
        close(fd);
    }

    /* Fallback directly to FreeBSD /dev/klog */
    fd = open("/dev/klog", O_RDONLY | O_NONBLOCK);
    if (fd >= 0) {
        g_log_is_dev = 1;
        return fd;
    }

    log_line("vpad: unable to open klog server or /dev/klog (errno=%d)", errno);
    return -1;
}

static void close_kernel_log(void)
{
    if (g_log_fd >= 0) {
        close(g_log_fd);
        g_log_fd = -1;
    }
    g_line_len = 0;
}

static const char *find_word(const char *line, const char *word)
{
    size_t wlen = strlen(word);
    const char *p = line;
    while ((p = strstr(p, word)) != NULL) {
        if (p == line || !( (p[-1] >= 'A' && p[-1] <= 'Z') ||
                             (p[-1] >= 'a' && p[-1] <= 'z') ||
                             (p[-1] >= '0' && p[-1] <= '9') ||
                              p[-1] == '_')) {
            return p;
        }
        p += wlen;
    }
    return NULL;
}

static int parse_klog_handle(const char *line)
{
    const char *p = strstr(line, "Open Pad");
    if (!p) p = strstr(line, "open pad");
    if (p) {
        const char *r = strstr(p, "ret=");
        if (r) {
            int h = (int)strtol(r + 4, NULL, 0);
            if (plausible_handle(h)) return h;
        }
    }
    return -1;
}

static uint64_t parse_device_id_from_klog(const char *line)
{
    if (!strstr(line, "DEVICE_ADDED") && !strstr(line, "device_added") && !strstr(line, "DeviceAdded"))
        return 0;

    const char *st = strstr(line, "subType");
    if (!st) st = strstr(line, "subtype");
    if (!st) st = strstr(line, "SubType");
    if (st) {
        st += 7;
        while (*st == ' ' || *st == ':' || *st == '=') st++;
        char *subtype_end;
        unsigned long subtype = strtoul(st, &subtype_end, 0);
        if (subtype_end == st || subtype != 22 || isalnum((unsigned char)*subtype_end)) {
            return 0;
        }
    }
    else return 0; /* Never identify an untyped native device as ours. */

    const char *p = find_word(line, "DeviceId");
    if (!p) p = find_word(line, "deviceId");
    if (!p) p = find_word(line, "dev_id");
    if (!p) return 0;

    p += !strncmp(p, "dev_id", 6) ? 6 : 8;
    while (*p == ' ' || *p == ':' || *p == '=' ) p++;
    return hex_to_u64(p);
}

static void poll_kernel_log(void)
{
    char buf[512];
    for (int r = 0; r < 8 && g_log_fd >= 0; r++) {
        ssize_t n = read(g_log_fd, buf, sizeof(buf));
        if (n <= 0) break;
        for (ssize_t i = 0; i < n; i++) {
            if (buf[i] != '\n' && g_line_len < sizeof(g_line) - 1) {
                g_line[g_line_len++] = buf[i];
                continue;
            }
            g_line[g_line_len] = '\0';
            g_line_len = 0;

            int kh = parse_klog_handle(g_line);
            if (kh > 0 && g_klog_handle <= 0) {
                g_klog_handle = kh;
                log_line("vpad: captured Open Pad handle %d from klog", kh);
            }

            uint64_t dev = parse_device_id_from_klog(g_line);
            if (dev && g_pending >= 0) {
                if (g_matches == 0) {
                    g_match_dev = dev;
                    g_t_match = now_ms();
                }
                if (g_matches == 0 || dev != g_match_dev)
                    g_matches++;
            }
        }
    }
}

static int32_t read_user_config(const char *path)
{
    FILE *file = fopen(path, "r");
    if (!file) return 0;
    char value[32] = {0};
    int have_value = fgets(value, sizeof(value), file) != NULL;
    fclose(file);
    char *end;
    errno = 0;
    unsigned long user = strtoul(value, &end, 16);
    while (*end && isspace((unsigned char)*end)) end++;
    if (!have_value || errno || end == value || *end || !user || user > 0x7fffffffUL) return -1;
    return (int32_t)user;
}

int vpad_init(void)
{
    if (g_ready) return 1;

    g_protected_user = read_user_config(FREEPAD_NATIVE_USER_PATH);
    g_controller_user = read_user_config(FREEPAD_CONTROLLER_USER_PATH);
    if (g_protected_user < 0 || g_controller_user < 0 || (g_controller_user > 0 && g_controller_user == g_protected_user)) {
        log_line("FreePad: invalid/conflicting profile configuration; refusing to initialize");
        return 0;
    }
    log_line("FreePad: protected native user 0x%08x; preferred controller user 0x%08x", (unsigned)g_protected_user, (unsigned)g_controller_user);

    if (!elevate_privileges()) {
        log_line("vpad: failed to elevate privileges");
        return 0;
    }

#ifdef __PROSPERO__
    sceUserServiceInitialize(NULL);

    void *mbus = dlopen("/system/common/lib/libSceMbus.sprx", RTLD_NOW | RTLD_GLOBAL);
    if (!mbus) mbus = dlopen("libSceMbus.sprx", RTLD_NOW | RTLD_GLOBAL);
    if (mbus) {
        g_bind = (mbus_bind_fn)dlsym(mbus, "sceMbusBindDeviceWithUserId");
        g_disconnect = (mbus_disconnect_fn)dlsym(mbus, "sceMbusDisconnectDevice");
        g_unbind = (mbus_unbind_fn)dlsym(mbus, "sceMbusUnbindDeviceWithUserId");
        if (!g_unbind) g_unbind = (mbus_unbind_fn)dlsym(mbus, "sceMbusUnbindDevice");
        log_line("vpad: libSceMbus loaded: bind=%p, disconnect=%p, unbind=%p",
                 (void*)g_bind, (void*)g_disconnect, (void*)g_unbind);
    }

    if (!g_bind) {
        log_line("vpad: local MBus unavailable; refusing virtual pad creation");
        restore_privileges();
        return 0;
    }

    int r = scePadInit();
    if (r != 0) {
        log_line("vpad: scePadInit returned 0x%08x", (unsigned)r);
        restore_privileges();
        return 0;
    }

    /* Start with process privilege 1 to manage virtual pads */
    scePadSetProcessPrivilege(1);

    /* The dedicated FreePad payload must not delete pads owned by other payloads. */
#endif

    g_active_user = get_user_id_for_slot(0);
    log_line("vpad: initialized successfully, default user 0x%08x", (unsigned)g_active_user);
    g_ready = 1;
    return 1;
}

int vpad_add(int slot, pad_conn_type_t type, const char *name)
{
    if (!g_ready || slot < 0 || slot >= MAX_SLOTS) return 0;

    pthread_mutex_lock(&g_vpad_mutex);
    if (g_slots[slot].status != VP_FREE && g_slots[slot].status != VP_FAILED) {
        pthread_mutex_unlock(&g_vpad_mutex);
        return 0;
    }

    memset(&g_slots[slot], 0, sizeof(internal_slot_t));
    g_slots[slot].status = VP_QUEUED;
    g_slots[slot].handle = -1;
    g_slots[slot].alt_handle = -1;
    g_slots[slot].conn_type = type;
    if (name) snprintf(g_slots[slot].name, sizeof(g_slots[slot].name), "%s", name);
    g_slots[slot].connected_time = now_ms();
    pad_state_neutral(&g_slots[slot].pad_state);

#ifdef __PROSPERO__
    /* Ensure process privilege is active when a controller connects */
    scePadSetProcessPrivilege(1);
#endif
    pthread_mutex_unlock(&g_vpad_mutex);

    const char *conn_str = "Unknown";
    if (type == CONN_USB_WIRED) conn_str = "USB";
    else if (type == CONN_USB_DONGLE_24G) conn_str = "2.4G Dongle";
    else if (type == CONN_BLUETOOTH_CLASSIC || type == CONN_BLUETOOTH_LE) conn_str = "Bluetooth";
    else if (type == CONN_NETWORK_STREAM) conn_str = "LAN";

    log_line("vpad: slot %d queued (%s via %s)", slot,
             g_slots[slot].name, conn_str);
    return 1;
}

static void start_identification(int slot)
{
    struct {
        int32_t size;
        int32_t user_id;
        int32_t pad[6];
    } param;
    const int32_t sentinel = 0x7EADBEEF;

    memset(&param, 0, sizeof(param));
    param.size = (int32_t)sizeof(param);
    g_slots[slot].user_id = get_user_id_for_slot(slot);
    param.user_id = (g_slots[slot].user_id > 0) ? g_slots[slot].user_id : 1;
    /* This field is a device creation flag, not the user's account ID.
     * Bind the identified device to the account through MBus afterwards. */
    param.user_id = 1;
    for (int i = 0; i < 6; i++) param.pad[i] = sentinel;
    elevate_privileges();

#ifdef __PROSPERO__
    int priv_r = scePadSetProcessPrivilege(1);
    if (priv_r != 0) {
        log_line("vpad: scePadSetProcessPrivilege(1) returned 0x%08x", (unsigned)priv_r);
    }
#endif

    g_log_fd = open_kernel_log();
    if (g_log_fd >= 0) {
        /* Drain klog backlog before adding device */
        char drain[1024];
        while (read(g_log_fd, drain, sizeof(drain)) > 0);
        g_line_len = 0;
    } else {
        log_line("vpad: slot %d warning: klog unavailable, proceeding directly", slot);
        /* No AddDevice means no unidentified orphan and no native-handle
         * fallback when ownership cannot be established. */
        g_slots[slot].status = VP_FAILED;
        return;
    }

    g_matches = 0;
    g_match_dev = 0;
    g_klog_handle = -1;

    int32_t ret = 0;
#ifdef __PROSPERO__
    ret = scePadVirtualDeviceAddDevice(&param, VIRTUAL_DEVICE_DUALSENSE);
#endif
    log_line("vpad: slot %d AddDevice -> 0x%08x", slot, (unsigned)ret);

    int handle = -1;
    if (plausible_handle(ret)) {
        handle = ret;
    } else {
        /* Check if kernel output the allocated handle into param.pad[i] */
        for (int i = 0; i < 6; i++) {
            if (param.pad[i] != sentinel && plausible_handle(param.pad[i])) {
                handle = param.pad[i];
                log_line("vpad: slot %d handle found in param.pad[%d]: %d", slot, i, handle);
                break;
            }
        }
    }

    g_slots[slot].handle = handle;
    g_slots[slot].alt_handle = -1;

    g_pending = slot;
    g_t_add = now_ms();
    g_slots[slot].status = VP_PENDING;
    if ((uint32_t)ret == 0x803B0006u) {
        log_line("vpad: assignment pending; awaiting owned device identification");
    }
}

static void finish_identification(long now)
{
    internal_slot_t *s = &g_slots[g_pending];
    int slot = g_pending;

    if (g_matches == 0 && now - g_t_add < T_IDENTIFY) return;
    if (g_matches == 1 && now - g_t_match < T_AMBIGUITY && now - g_t_add < T_IDENTIFY) return;

    g_pending = -1;
    close_kernel_log();

    /* Only submit data to a device uniquely observed during our AddDevice. */
    if (g_matches != 1) {
        log_line("vpad: FreePad device identification failed (%d matches)", g_matches);
        s->status = VP_FAILED;
        return;
    }
    s->device_id = g_match_dev;
    s->handle = (int32_t)(g_match_dev & 0xffffffffu);
    s->alt_handle = -1;
    if (s->remove_when_found) {
        scePadVirtualDeviceDeleteDevice(s->handle);
        memset(s, 0, sizeof(*s));
        return;
    }
    s->user_id = get_user_id_for_slot(slot);
    if (s->user_id <= 0) {
        /* Keep only our uniquely identified device unbound. The native
         * profile picker can sign in the separate controller user; never bind
         * to the protected DualSense user just to get input working. */
        s->status = VP_READY;
        log_line("FreePad: virtual pad waiting unbound; press Home and choose the separate controller profile");
        return;
    }
    int32_t bind_result = -1;
    if (s->user_id > 0 && g_bind) bind_result = g_bind(s->device_id, s->user_id);
    log_line("vpad: FreePad bind device 0x%llx to user 0x%08x -> 0x%08x",
             (unsigned long long)s->device_id, (unsigned)s->user_id, (unsigned)bind_result);
    if (bind_result != 0) {
        scePadVirtualDeviceDeleteDevice(s->handle);
        s->handle = -1;
        s->device_id = 0;
        s->status = VP_FAILED;
        return;
    }
    s->status = VP_READY;
    log_line("vpad: FreePad ready (handle %d)", s->handle);
}

void vpad_poll(long now)
{
    if (!g_ready) return;

    pthread_mutex_lock(&g_vpad_mutex);
    if (g_pending >= 0) {
        poll_kernel_log();
        finish_identification(now);
        pthread_mutex_unlock(&g_vpad_mutex);
    } else {
        /* Check if we need to start an identification */
        for (int i = 0; i < MAX_SLOTS; i++) {
            if (g_slots[i].status == VP_QUEUED) {
                start_identification(i);
                break;
            }
        }
        pthread_mutex_unlock(&g_vpad_mutex);
    }


    static long last_binding_check;
    if (now - last_binding_check > 500) {
        last_binding_check = now;
        pthread_mutex_lock(&g_vpad_mutex);
        internal_slot_t *s = &g_slots[0];
        if (s->status == VP_READY && s->user_id <= 0 && s->device_id && g_bind) {
            int32_t target = get_user_id_for_slot(0);
            if (target > 0) {
                int32_t result = g_bind(s->device_id, target);
                if (result == 0) s->user_id = target;
                else s->status = VP_FAILED;
                log_line("FreePad: delayed profile bind user 0x%08x -> 0x%08x", (unsigned)target, (unsigned)result);
            }
        }
        pthread_mutex_unlock(&g_vpad_mutex);
    }

    /* Continuous 250Hz injection of the last known state for all READY pads */
    for (int i = 0; i < MAX_SLOTS; i++) {
        pthread_mutex_lock(&g_vpad_mutex);
        if (g_slots[i].status == VP_READY && (g_slots[i].handle > 0 || g_slots[i].alt_handle > 0)) {
            /* Reset latched state to neutral if no updates received for > 500ms (avoids stuck buttons on sleep) */

            if (g_slots[i].handle <= 0 && g_slots[i].alt_handle > 0) {
                g_slots[i].handle = g_slots[i].alt_handle;
                g_slots[i].alt_handle = -1;
            }
            PadData d;
            uint64_t pad_time = 0;
#ifdef __PROSPERO__
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            pad_time = (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
#else
            pad_time = (uint64_t)now * 1000ULL;
#endif
            pad_data_from_state(&d, &g_slots[i].pad_state, pad_time);
            if (now < g_slots[i].ps_until) d.buttons |= PAD_BTN_PS;

            int32_t handle = g_slots[i].handle;
#ifndef __PROSPERO__
            int32_t alt_handle = g_slots[i].alt_handle;
#endif
            g_slots[i].packets_injected++;

#ifdef __PROSPERO__
            int r = scePadVirtualDeviceInsertData(handle, &d);

            if (r == 0) {
                g_slots[i].consecutive_insert_errors = 0;
            } else {
                g_slots[i].consecutive_insert_errors++;
                static long s_last_insert_err[MAX_SLOTS] = {0};
                if (now - s_last_insert_err[i] > 3000) {
                    s_last_insert_err[i] = now;
                    log_line("vpad: slot %d InsertData(h=%d) error 0x%08x (fails: %d)",
                             i, handle, (unsigned)r, g_slots[i].consecutive_insert_errors);
                }
                /* If handle fails repeatedly for > 250 consecutive ticks (~1000ms), handle is dead */
                if (g_slots[i].consecutive_insert_errors >= 250) {
                    log_line("vpad: FreePad input rejected; stopping this virtual pad");
                    g_slots[i].status = VP_FAILED;
                }
            }
            pthread_mutex_unlock(&g_vpad_mutex);
#else
            (void)handle; (void)alt_handle;
            pthread_mutex_unlock(&g_vpad_mutex);
#endif
        } else {
            pthread_mutex_unlock(&g_vpad_mutex);
        }
    }
}

void vpad_remove(int slot)
{
    if (slot < 0 || slot >= MAX_SLOTS) return;

    pthread_mutex_lock(&g_vpad_mutex);
    internal_slot_t *s = &g_slots[slot];
    if (s->status == VP_PENDING) {
        s->remove_when_found = 1;
        pthread_mutex_unlock(&g_vpad_mutex);
        return;
    }

    int32_t handle = s->handle;
    int32_t alt_handle = s->alt_handle;
    uint64_t dev_id = s->device_id;
    int32_t uid = s->user_id;
    int had_device = (s->status != VP_FREE);
    memset(s, 0, sizeof(*s));

#ifdef __PROSPERO__
    if (had_device) {
        if (dev_id > 0) {
            mbus_disconnect_device(dev_id, uid);
        }
        if (handle > 0) {
            int del_r = scePadVirtualDeviceDeleteDevice(handle);
            log_line("vpad: slot %d delete handle %d -> %d", slot, handle, del_r);
        }
        if (alt_handle > 0 && alt_handle != handle) {
            int del_alt = scePadVirtualDeviceDeleteDevice(alt_handle);
            log_line("vpad: slot %d delete alt_handle %d -> %d", slot, alt_handle, del_alt);
        }
        log_line("vpad: slot %d removed (handle %d, dev 0x%llx, uid 0x%08x)",
                 slot, handle, (unsigned long long)dev_id, (unsigned)uid);
        notify_ps5("FreePad: Controller disconnected (Slot %d)", slot + 1);
    }
#endif
    pthread_mutex_unlock(&g_vpad_mutex);
}

int vpad_is_live(int slot)
{
    if (slot < 0 || slot >= MAX_SLOTS) return 0;
    pthread_mutex_lock(&g_vpad_mutex);
    int live = g_slots[slot].status == VP_READY;
    pthread_mutex_unlock(&g_vpad_mutex);
    return live;
}

int vpad_slot_is_free(int slot)
{
    if (slot < 0 || slot >= MAX_SLOTS) return 0;
    pthread_mutex_lock(&g_vpad_mutex);
    int free_slot = g_slots[slot].status == VP_FREE;
    pthread_mutex_unlock(&g_vpad_mutex);
    return free_slot;
}

void vpad_update(int slot, const pad_state_t *st)
{
    if (!st || slot < 0 || slot >= MAX_SLOTS) return;

    static uint32_t s_prev_btn[MAX_SLOTS] = {0};
    uint32_t prev = s_prev_btn[slot];
    int ps_pressed = (st->buttons & PAD_BTN_PS) && !(prev & PAD_BTN_PS);
    s_prev_btn[slot] = st->buttons;

    pthread_mutex_lock(&g_vpad_mutex);
    vpad_status_t status = g_slots[slot].status;
    int32_t handle = g_slots[slot].handle;
    int32_t uid = g_slots[slot].user_id;

    if (ps_pressed) {
        if (status == VP_READY && handle > 0) {
            log_line("vpad: physical PS button pressed on slot %d (handle %d, user 0x%08x)",
                     slot + 1, handle, (unsigned)uid);
            pthread_mutex_unlock(&g_vpad_mutex);
            vpad_press_ps_button(slot);
            pthread_mutex_lock(&g_vpad_mutex);
        } else if (status == VP_QUEUED || status == VP_PENDING) {
            log_line("vpad: slot %d PS pressed while identifying — queuing pending press", slot + 1);
            g_slots[slot].ps_pending_on_ready = 1;
        }
    }

    if (g_slots[slot].status != VP_READY || (g_slots[slot].handle <= 0 && g_slots[slot].alt_handle <= 0)) {
        pthread_mutex_unlock(&g_vpad_mutex);
        return;
    }

    g_slots[slot].battery_level = st->battery_level;
    g_slots[slot].battery_charging = st->battery_charging;
    g_slots[slot].last_update_time = now_ms();

    /* Latch state; vpad_poll handles continuous 250Hz injection */
    g_slots[slot].pad_state = *st;
    pthread_mutex_unlock(&g_vpad_mutex);
}

void vpad_press_ps_button(int slot)
{
    if (slot < 0 || slot >= MAX_SLOTS) return;
    pthread_mutex_lock(&g_vpad_mutex);
    if (g_slots[slot].status == VP_READY) g_slots[slot].ps_until = now_ms() + 150;
    pthread_mutex_unlock(&g_vpad_mutex);
}

void vpad_get_slot_info(int slot, vpad_slot_info_t *out)
{
    if (slot < 0 || slot >= MAX_SLOTS || !out) return;
    pthread_mutex_lock(&g_vpad_mutex);
    out->status = g_slots[slot].status;
    out->handle = g_slots[slot].handle;
    out->alt_handle = g_slots[slot].alt_handle;
    out->device_id = g_slots[slot].device_id;
    out->user_id = g_slots[slot].user_id;
    out->conn_type = g_slots[slot].conn_type;
    snprintf(out->name, sizeof(out->name), "%s", g_slots[slot].name);
    out->battery_level = g_slots[slot].battery_level;
    out->battery_charging = g_slots[slot].battery_charging;
    out->packets_injected = g_slots[slot].packets_injected;
    out->connected_time = g_slots[slot].connected_time;
    out->last_update_time = g_slots[slot].last_update_time;
    out->buttons = g_slots[slot].pad_state.buttons;
    out->lx = g_slots[slot].pad_state.lx;
    out->ly = g_slots[slot].pad_state.ly;
    out->rx = g_slots[slot].pad_state.rx;
    out->ry = g_slots[slot].pad_state.ry;
    pthread_mutex_unlock(&g_vpad_mutex);
}

int vpad_rebind_user(int slot, int32_t user_id)
{
    if (slot < 0 || slot >= MAX_SLOTS) return 0;
    pthread_mutex_lock(&g_vpad_mutex);
    internal_slot_t *s = &g_slots[slot];
    if (s->status != VP_READY) {
        pthread_mutex_unlock(&g_vpad_mutex);
        return 0;
    }

    int32_t target_user = user_id;
    if (!is_user_logged_in(target_user) || !g_bind || !s->device_id) {
        pthread_mutex_unlock(&g_vpad_mutex);
        return 0;
    }
    if (target_user == g_protected_user || vpad_native_controller_connected(target_user) == 1) {
        pthread_mutex_unlock(&g_vpad_mutex);
        log_line("FreePad: refusing to bind over a connected native controller");
        return 0;
    }
    int32_t result = g_bind(s->device_id, target_user);
    if (result == 0) s->user_id = target_user;
    pthread_mutex_unlock(&g_vpad_mutex);
    log_line("FreePad dashboard: bind to user 0x%08x -> 0x%08x", (unsigned)target_user, (unsigned)result);
    return result == 0;
}

void vpad_cleanup_all(void)
{
    for (int i = 0; i < MAX_SLOTS; i++) {
        vpad_remove(i);
    }
    /* A stop/release during AddDevice must still finish ownership discovery
     * and honor remove_when_found, rather than abandon a pending pad. */
    long deadline = now_ms() + T_IDENTIFY + T_AMBIGUITY;
    for (;;) {
        pthread_mutex_lock(&g_vpad_mutex);
        int pending = g_pending;
        pthread_mutex_unlock(&g_vpad_mutex);
        if (pending < 0 || now_ms() >= deadline) break;
        vpad_poll(now_ms());
        usleep(4000);
    }
    close_kernel_log();
    restore_privileges();
    g_ready = 0;
}

int vpad_native_controller_connected(int32_t user)
{
#ifdef __PROSPERO__
    int32_t handle = scePadGetHandle(user, 0, 0);
    PadData data = {0};
    if (handle <= 0 || scePadReadState(handle, &data) != 0) return -1;
    return data.connected != 0;
#else
    (void)user;
    return -1;
#endif
}

int32_t vpad_get_protected_user(void)
{
    return g_protected_user;
}

