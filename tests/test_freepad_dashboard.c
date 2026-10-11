/* Host-only HTTP tests. Controller/console APIs are mocked; no hardware touched.
 * Run in the disposable SDK container, not on a console. */
#include "../src/dashboard/http_server.c"
#include <assert.h>
#include <signal.h>
#include <sys/stat.h>

static vpad_slot_info_t mock_pad;
static int connected = 1, removed, added, pressed, rebound, stopped;

void log_line(const char *fmt, ...) { (void)fmt; }
void freepad_request_stop(void) { stopped++; }
int usb_hotplug_freepad_connected(void) { return connected; }
int32_t vpad_get_protected_user(void) { return 0; }
void vpad_get_slot_info(int slot, vpad_slot_info_t *out)
{ assert(slot == 0); *out = mock_pad; }
int vpad_is_live(int slot)
{ assert(slot == 0); return mock_pad.status == VP_READY; }
void vpad_remove(int slot)
{ assert(slot == 0); removed++; memset(&mock_pad, 0, sizeof(mock_pad)); }
int vpad_add(int slot, pad_conn_type_t type, const char *name)
{
    assert(slot == 0 && type == CONN_USB_WIRED && name);
    assert(mock_pad.status == VP_FREE);
    added++; mock_pad.status = VP_QUEUED; return 1;
}
void vpad_press_ps_button(int slot) { assert(slot == 0); pressed++; }
int vpad_rebind_user(int slot, int32_t user)
{ assert(slot == 0); rebound++; return user == 0x1234; }

static void *serve(void *arg)
{ int fd = *(int *)arg; handle(fd); close(fd); return NULL; }

static void request(const char *raw, int status, const char *expected, int split)
{
    int fds[2]; assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    struct timeval timeout = {2, 0};
    setsockopt(fds[0], SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fds[1], SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    pthread_t worker_thread; assert(pthread_create(&worker_thread, NULL, serve, &fds[1]) == 0);
    size_t len = strlen(raw), part = split ? 7 : len;
    assert(send(fds[0], raw, part, 0) == (ssize_t)part);
    if (split) {
        usleep(10000);
        assert(send(fds[0], raw + part, len - part, 0) == (ssize_t)(len - part));
    }
    shutdown(fds[0], SHUT_WR);
    char response[20000] = {0}; size_t used = 0;
    ssize_t n;
    while ((n = recv(fds[0], response + used, sizeof(response) - used - 1, 0)) > 0)
        used += (size_t)n;
    close(fds[0]); pthread_join(worker_thread, NULL);
    char prefix[32]; snprintf(prefix, sizeof(prefix), "HTTP/1.1 %d ", status);
    assert(!strncmp(response, prefix, strlen(prefix)));
    assert(strstr(response, expected));
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);
    mkdir("/data", 0700); mkdir(FREEPAD_STATE_DIR, 0700);
    unlink(FREEPAD_PROFILES_PATH);
    assert(freepad_profiles_init(FREEPAD_PROFILES_PATH));
    unlink(FREEPAD_STATE_DIR "/pause");
    mock_pad.status = VP_READY;
    mock_pad.user_id = 0x1234;
    mock_pad.buttons = PAD_BTN_CROSS;
    mock_pad.lx = 128;
    request("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n", 200, "Release virtual pad", 1);
    assert(strstr(PAGE, "<title>FreePadUSB</title>"));
    assert(!strstr(PAGE, "PDP"));
    assert(!strstr(PAGE, "Switch controller"));
    request("GET /dashboard.css HTTP/1.1\r\nHost: localhost\r\n\r\n", 200, "text/css", 0);
    request("GET /dashboard.js HTTP/1.1\r\nHost: localhost\r\n\r\n", 200, "customLayout", 0);
    assert(control_header("POST /api/home HTTP/1.1\r\nX-FreePad-Control: 1\r\n\r\n"));
    assert(control_header("POST /api/home HTTP/1.1\r\nx-freepad-control:\t1\r\n\r\n"));
    assert(!control_header("POST /api/home HTTP/1.1\r\nX-FreePad-Control: 10\r\n\r\n"));
    assert(!control_header("POST /api/home HTTP/1.1\r\nX-FreePad-Control: 0\r\n\r\n"));
    request("GET /api/state HTTP/1.1\r\n\r\n", 200, "\"user\":\"00001234\"", 0);
    request("GET /api/profiles HTTP/1.1\r\n\r\n", 200, "0e6f:0184 switch8 triangle,circle,cross", 1);
    request("GET /api/report HTTP/1.1\r\n\r\n", 200, "No reports yet", 0);
    request("POST /api/profiles?data=bad HTTP/1.1\r\n\r\n", 403, "Missing control header", 0);
    request("POST /api/profiles?data=bad HTTP/1.1\r\nX-FreePad-Control: 1\r\n\r\n", 409, "Release the virtual pad", 0);
    request("POST /api/release HTTP/1.1\r\n\r\n", 403, "Missing control header", 0);
    assert(!removed);
    request("POST /api/release HTTP/1.1\r\nX-PDP-Control: 10\r\n\r\n", 403, "Missing control header", 0);
    request("GET /api/release HTTP/1.1\r\n\r\n", 400, "Unknown request", 0);
    request("POST /api/home HTTP/1.1\r\nX-FreePad-Control: 1\r\n\r\n", 200, "Home / PS sent", 0);
    assert(pressed == 1);
    request("POST /api/rebind?user=00001234 HTTP/1.1\r\nx-pdp-control:\t1\r\n\r\n", 200, "Profile assigned", 0);
    request("POST /api/rebind?user=1234junk HTTP/1.1\r\nX-PDP-Control: 1\r\n\r\n", 409, "Could not bind", 0);
    request("POST /api/rebind?user=ffffffff HTTP/1.1\r\nX-PDP-Control: 1\r\n\r\n", 409, "Could not bind", 0);
    assert(rebound == 1);
    request("POST /api/release HTTP/1.1\r\nX-PDP-Control: 1\r\n\r\n", 200, "Virtual pad released", 0);
    assert(removed == 1 && access(FREEPAD_STATE_DIR "/pause", F_OK) == 0);
    request("POST /api/profiles?data=%00 HTTP/1.1\r\nX-FreePad-Control: 1\r\n\r\n", 409, "existing settings unchanged", 1);
    request("POST /api/profiles?data=%z0 HTTP/1.1\r\nX-FreePad-Control: 1\r\n\r\n", 409, "existing settings unchanged", 0);
    request("POST /api/profiles?data=% HTTP/1.1\r\nX-FreePad-Control: 1\r\n\r\n", 409, "existing settings unchanged", 0);
    request("POST /api/profiles?data=0e6f%3A0184%20switch8%20triangle%2Ccircle%2Ccross%2Csquare%2Cl1%2Cr1%2Cl2%2Cr2%2Ccreate%2Coptions%2Cl3%2Cr3%2Cps%2Ctouchpad%0A HTTP/1.1\r\nX-FreePad-Control: 1\r\n\r\n", 200, "Profiles saved and applied", 1);
    request("GET /api/profiles HTTP/1.1\r\n\r\n", 200, "0e6f:0184 switch8 triangle,circle,cross", 0);
    request("POST /api/home HTTP/1.1\r\nX-PDP-Control: 1\r\n\r\n", 409, "not active", 0);
    assert(pressed == 1);
    connected = 0;
    request("POST /api/reconnect HTTP/1.1\r\nX-PDP-Control: 1\r\n\r\n", 409, "unavailable", 0);
    assert(!added && access(FREEPAD_STATE_DIR "/pause", F_OK) == 0);
    connected = 1;
    request("POST /api/reconnect HTTP/1.1\r\nX-PDP-Control: 1\r\n\r\n", 200, "Creating virtual pad", 0);
    assert(added == 1 && access(FREEPAD_STATE_DIR "/pause", F_OK) != 0);
    request("POST /api/reconnect HTTP/1.1\r\nX-PDP-Control: 1\r\n\r\n", 409, "unavailable", 0);
    assert(added == 1);
    mock_pad.status = VP_FAILED;
    request("POST /api/reconnect HTTP/1.1\r\nX-PDP-Control: 1\r\n\r\n", 200, "Creating virtual pad", 0);
    assert(added == 2 && removed == 2);
    request("POST /api/stop HTTP/1.1\r\nX-PDP-Control: 1\r\n\r\n", 200, "Stopping payload", 0);
    assert(stopped == 1);
    request("GET /api/log HTTP/1.1\r\n\r\n", 200, "text/plain", 0);
    char escaped[64]; escape_json(escaped, sizeof(escaped), "Name\"\\\n");
    assert(!strcmp(escaped, "Name\\\"\\\\ "));
    assert(freepad_web_init(0)); /* Exercise actual socket/thread startup and stop. */
    freepad_web_cleanup();
    unlink(FREEPAD_PROFILES_PATH);
    puts("FreePadUSB dashboard tests passed");
    return 0;
}
