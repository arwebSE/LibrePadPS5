#define _DEFAULT_SOURCE
#include "core/legacy_paths.h"
#include "dashboard/http_server.h"
#include "ps5/virtual_controller.h"
#include "input/usb_device.h"
#include "core/logging.h"
#include "input/controller_profiles.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef __PROSPERO__
#include <ps5/kernel.h>
extern int32_t sceUserServiceGetLoginUserIdList(int32_t users[4]);
extern int32_t sceUserServiceGetForegroundUser(int32_t *user);
extern int32_t sceUserServiceGetUserName(int32_t user, char *name, size_t size);
#endif

static int server = -1;
static pthread_t thread;
static atomic_int running;

#include "dashboard/assets.h"

static void reply(int fd, int status, const char *type, const char *body)
{
    char header[256];
    const char *reason = status == 200 ? "OK" : status == 403 ? "Forbidden" : status == 409 ? "Conflict" : "Bad Request";
    size_t length = strlen(body);
    int n = snprintf(header, sizeof(header), "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n", status, reason, type, length);
    const char *parts[] = {header, body};
    size_t sizes[] = {(size_t)n, length};
    for (int i = 0; i < 2; i++) {
        size_t sent = 0;
        while (sent < sizes[i]) {
            ssize_t count = send(fd, parts[i] + sent, sizes[i] - sent, 0);
            if (count <= 0) return;
            sent += (size_t)count;
        }
    }
}

static void escape_json(char *out, size_t size, const char *in)
{
    size_t pos = 0;
    for (; *in && pos + 3 < size; in++) {
        unsigned char c = (unsigned char)*in;
        if (c < 32) c = ' ';
        if (c == '"' || c == '\\') out[pos++] = '\\';
        out[pos++] = (char)c;
    }
    out[pos] = 0;
}

static void state(int fd)
{
    char body[4096], users[2048] = "", name[64], escaped[128];
    size_t used = 0;
    int32_t login[4] = {-1, -1, -1, -1}, foreground = -1;
    uint32_t firmware = 0;
#ifdef __PROSPERO__
    firmware = kernel_get_fw_version();
    sceUserServiceGetLoginUserIdList(login);
    sceUserServiceGetForegroundUser(&foreground);
#endif
    for (int i = 0; i < 4; i++) {
        if (login[i] <= 0) continue;
        snprintf(name, sizeof(name), "Profile %d", i + 1);
        int native_connected = -1;
#ifdef __PROSPERO__
        sceUserServiceGetUserName(login[i], name, sizeof(name));
        name[sizeof(name) - 1] = 0;
        native_connected = vpad_native_controller_connected(login[i]);
#endif
        escape_json(escaped, sizeof(escaped), name);
        int n = snprintf(users + used, sizeof(users) - used,
            "%s{\"id\":\"%08x\",\"name\":\"%s\",\"foreground\":%s,\"native_connected\":%s,\"protected\":%s}",
            used ? "," : "", (unsigned)login[i], escaped, login[i] == foreground ? "true" : "false",
            native_connected < 0 ? "null" : native_connected ? "true" : "false", login[i] == vpad_get_protected_user() ? "true" : "false");
        if (n < 0 || (size_t)n >= sizeof(users) - used) break;
        used += (size_t)n;
    }
    vpad_slot_info_t pad = {0};
    vpad_get_slot_info(0, &pad);
    snprintf(body, sizeof(body),
        "{\"firmware\":\"%x.%02x\",\"usb_connected\":%s,\"live\":%s,\"status\":%d,\"user\":\"%08x\",\"buttons\":%u,\"axes\":[%u,%u,%u,%u],\"frames\":%u,\"users\":[%s]}",
        (firmware >> 24) & 255, (firmware >> 16) & 255,
        usb_hotplug_freepad_connected() ? "true" : "false", pad.status == VP_READY ? "true" : "false", pad.status,
        (unsigned)pad.user_id, pad.buttons, pad.lx, pad.ly, pad.rx, pad.ry, pad.packets_injected, users);
    reply(fd, 200, "application/json", body);
}

static int control_header(const char *request)
{
    const char *line = strstr(request, "\r\n");
    while (line && line[2]) {
        line += 2;
        /* Keep old clients working without changing the strict value check. */
        size_t prefix = strncasecmp(line, "X-FreePad-Control:", 18) == 0 ? 18 :
                        strncasecmp(line, "X-PDP-Control:", 14) == 0 ? 14 : 0;
        if (prefix) {
            const char *value = line + prefix;
            while (*value == ' ' || *value == '\t') value++;
            return value[0] == '1' && value[1] == '\r' && value[2] == '\n';
        }
        line = strstr(line, "\r\n");
    }
    return 0;
}

static int decode_profile_text(const char *in, char out[FREEPAD_PROFILE_TEXT_MAX])
{
    size_t used = 0;
    while (*in) {
        unsigned char c = (unsigned char)*in++;
        if (c == '%') {
            if (!in[0] || !in[1]) return 0;
            int n[2];
            for (int i = 0; i < 2; i++) {
                unsigned char h = (unsigned char)in[i];
                n[i] = h >= '0' && h <= '9' ? h-'0' : h >= 'a' && h <= 'f' ? h-'a'+10 :
                       h >= 'A' && h <= 'F' ? h-'A'+10 : -1;
                if (n[i] < 0) return 0;
            }
            c = (unsigned char)(n[0]*16+n[1]); in += 2;
        }
        if (!c || c > 126 || (c < 32 && c != '\n' && c != '\r') || used+1 >= FREEPAD_PROFILE_TEXT_MAX) return 0;
        out[used++] = (char)c;
    }
    out[used] = 0;
    return 1;
}


static void profiles_reply(int fd)
{
    char text[FREEPAD_PROFILE_TEXT_MAX];
    if (freepad_profiles_export(text,sizeof(text))) reply(fd,200,"text/plain; charset=utf-8",text);
    else reply(fd,409,"text/plain","Profiles unavailable");
}
static void report_reply(int fd)
{
    char text[256];
    freepad_profiles_report(text,sizeof(text));
    reply(fd,200,"text/plain; charset=utf-8",text);
}
static void log_reply(int fd)
{
        char log[12001] = {0};
        FILE *file = fopen(FREEPAD_LOG_PATH, "r");
        if (file) {
            fseek(file, 0, SEEK_END);
            long size = ftell(file);
            fseek(file, size > 12000 ? size - 12000 : 0, SEEK_SET);
            fread(log, 1, sizeof(log) - 1, file);
            fclose(file);
        }
        reply(fd, 200, "text/plain; charset=utf-8", log);

}
/* Data-driven routing; validation and ownership checks stay explicit. */
static int get_route(int fd, const char *path)
{
    static const struct { const char *path, *type, *body; } assets[] = {
        {"/","text/html; charset=utf-8",PAGE},
        {"/dashboard.css","text/css; charset=utf-8",DASHBOARD_CSS},
        {"/dashboard.js","text/javascript; charset=utf-8",DASHBOARD_JS}
    };
    static const struct { const char *path; void (*handle)(int); } routes[] = {
        {"/api/state",state}, {"/api/profiles",profiles_reply},
        {"/api/report",report_reply}, {"/api/log",log_reply}
    };
    for (size_t i = 0; i < sizeof(assets)/sizeof(assets[0]); i++)
        if (!strcmp(path,assets[i].path)) {
            reply(fd,200,assets[i].type,assets[i].body); return 1;
        }
    for (size_t i = 0; i < sizeof(routes)/sizeof(routes[0]); i++)
        if (!strcmp(path,routes[i].path)) { routes[i].handle(fd); return 1; }
    return 0;
}

static void handle(int fd)
{
    char request[8192] = {0}, method[8], path[6400], version[16];
    size_t used = 0;
    /* A client sending one byte just before each socket timeout must not
     * keep this single-worker server occupied indefinitely. */
    struct timeval started, current;
    gettimeofday(&started, NULL);
    while (!strstr(request, "\r\n\r\n")) {
        gettimeofday(&current, NULL);
        if (current.tv_sec - started.tv_sec >= 2) return;
        if (used == sizeof(request) - 1) return;
        ssize_t count = recv(fd, request + used, sizeof(request) - 1 - used, 0);
        if (count <= 0) return;
        used += (size_t)count;
        request[used] = 0;
    }
    if (sscanf(request, "%7s %6399s %15s", method, path, version) != 3 ||
        (strcmp(version, "HTTP/1.1") && strcmp(version, "HTTP/1.0"))) return;
    if (!strcmp(method, "GET")) {
        if (!get_route(fd,path)) reply(fd,400,"application/json","{\"message\":\"Unknown request\"}");
    } else if (!strcmp(method, "POST")) {
        if (!control_header(request)) {
            reply(fd, 403, "application/json", "{\"message\":\"Missing control header\"}");
            return;
        }
        int ok = 1;
        const char *message = "Action completed";
        if (!strncmp(path, "/api/profiles?data=", 19)) {
            char text[FREEPAD_PROFILE_TEXT_MAX];
            vpad_slot_info_t pad = {0}; vpad_get_slot_info(0, &pad);
            if (pad.status != VP_FREE || access(FREEPAD_STATE_DIR "/pause", F_OK) != 0) {
                ok = 0; message = "Release the virtual pad and wait until it is removed before saving";
            } else {
                ok = decode_profile_text(path+19, text) && freepad_profiles_save(text);
                message = ok ? "Profiles saved and applied. Reconnect the pad; replug new devices if needed." : "Invalid profiles or save failed; existing settings unchanged";
            }
        } else if (!strcmp(path, "/api/stop")) {
            reply(fd, 200, "application/json", "{\"message\":\"Stopping payload\"}");
            freepad_request_stop();
            return;
        } else if (!strcmp(path, "/api/release")) {
            int pause = open(FREEPAD_STATE_DIR "/pause", O_WRONLY | O_CREAT, 0600);
            ok = pause >= 0;
            if (ok) {
                close(pause);
                vpad_remove(0);
            }
            message = ok ? "Virtual pad released or release pending. Paused until Reconnect." : "Could not persist paused state; no action taken";
        } else if (!strcmp(path, "/api/reconnect")) {
            vpad_slot_info_t pad = {0};
            vpad_get_slot_info(0, &pad);
            if (usb_hotplug_freepad_connected() && (pad.status == VP_FREE || pad.status == VP_FAILED)) {
                /* A failed insertion can still own a device. Release it
                 * before queuing a replacement rather than leaking it. */
                if (pad.status == VP_FAILED) vpad_remove(0);
                ok = unlink(FREEPAD_STATE_DIR "/pause") == 0 || errno == ENOENT;
                if (ok) ok = vpad_add(0, CONN_USB_WIRED, "FreePadUSBPS5 controller");
            } else ok = 0;
            message = ok ? "Creating virtual pad" : "Controller unavailable or already active";
        } else if (!strcmp(path, "/api/home")) {
            ok = vpad_is_live(0);
            if (ok) vpad_press_ps_button(0);
            message = ok ? "Home / PS sent" : "Virtual pad is not active";
        } else if (!strncmp(path, "/api/rebind?user=", 17)) {
            char *end = NULL;
            errno = 0;
            unsigned long user = strtoul(path + 17, &end, 16);
            ok = !errno && end != path + 17 && !*end && user > 0 && user <= 0x7fffffffUL;
            if (ok) ok = vpad_rebind_user(0, (int32_t)user);
            message = ok ? "Profile assigned" : "Could not bind to that signed-in profile";
        } else {
            reply(fd, 400, "application/json", "{\"message\":\"Unknown action\"}");
            return;
        }
        char body[256];
        snprintf(body, sizeof(body), "{\"message\":\"%s\"}", message);
        reply(fd, ok ? 200 : 409, "application/json", body);
    } else reply(fd, 400, "application/json", "{\"message\":\"Unknown request\"}");
}

static void *worker(void *arg)
{
    (void)arg;
    while (running) {
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(server, &reads);
        struct timeval wait = {0, 100000};
        if (select(server + 1, &reads, NULL, NULL, &wait) <= 0) continue;
        int fd = accept(server, NULL, NULL);
        if (fd < 0) continue;
        struct timeval timeout = {0, 500000};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        handle(fd);
        close(fd);
    }
    return NULL;
}

int freepad_web_init(int port)
{
    server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) return 0;
    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((uint16_t)port);
    if (bind(server, (struct sockaddr *)&address, sizeof(address)) != 0 || listen(server, 4) != 0) {
        close(server);
        server = -1;
        return 0;
    }
    running = 1;
    if (pthread_create(&thread, NULL, worker, NULL) != 0) {
        running = 0;
        close(server);
        server = -1;
        return 0;
    }
    log_line("FreePadUSBPS5 dashboard listening on port %d", port);
    return 1;
}

void freepad_web_cleanup(void)
{
    if (server < 0) return;
    running = 0;
    pthread_join(thread, NULL);
    close(server);
    server = -1;
}
