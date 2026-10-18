/* Host-only HTTP tests. Controller/console APIs are mocked; no hardware touched.
 * Run in the disposable SDK container, not on a console. */
#define LIBREPAD_WITH_XBOX
#include "../src/dashboard/http_server.c"
#include <assert.h>
#include <signal.h>
#include <sys/stat.h>

static vpad_slot_info_t mock_pad, second_pad;
static int last_slot;
static int connected = 1, removed, added, pressed, rebound, stopped;
static int xbox_reloads;
static char last_response[DIAGNOSTIC_HEADER_LIMIT + DIAGNOSTIC_LOG_LIMIT + 1024];

void log_line(const char *fmt, ...) { (void)fmt; }
int librepad_xbox_is_connected(void) { return 0; }
void librepad_xbox_reload(void) { xbox_reloads++; }
void librepad_request_stop(void) { stopped++; }
int usb_hotplug_librepad_connected(void) { return connected; }
int32_t vpad_get_protected_user(void) { return 0; }
void vpad_get_slot_info(int slot, vpad_slot_info_t *out)
{ assert(slot >= 0 && slot < MAX_SLOTS); *out = slot == 0 ? mock_pad : slot == 1 ? second_pad : (vpad_slot_info_t){0}; }
int vpad_is_live(int slot)
{ last_slot=slot; return (slot==0 ? mock_pad.status : second_pad.status) == VP_READY; }
void vpad_remove(int slot)
{ last_slot=slot; if (slot==0) { removed++; memset(&mock_pad,0,sizeof(mock_pad)); } else memset(&second_pad,0,sizeof(second_pad)); }
int vpad_add(int slot, pad_conn_type_t type, const char *name)
{
    assert(slot == 0 && type == CONN_USB_WIRED && name);
    assert(mock_pad.status == VP_FREE);
    added++; mock_pad.status = VP_QUEUED; return 1;
}
void vpad_press_ps_button(int slot) { last_slot=slot; pressed++; }
int vpad_rebind_user(int slot, int32_t user)
{ assert(slot == 0); rebound++; return user == 0x1234; }

int vpad_user_is_protected(int32_t user) { return user==100; }
int vpad_protect_user(int32_t user, int reserved) { return user==101 && reserved==1; }
int controller_connected(int slot) { return slot==0 && connected; }
void controller_name(int slot, char *name, size_t size) { snprintf(name,size,"%s",slot==0 ? "Mock gamepad" : ""); }
int controller_paused(int slot) { (void)slot; return access(LIBREPAD_STATE_DIR "/pause",F_OK)==0; }
int controller_release(int slot) {
    int fd=open(LIBREPAD_STATE_DIR "/pause",O_CREAT|O_WRONLY,0600);
    if (fd<0) return 0; close(fd);
    if (slot<0) { vpad_remove(0); vpad_remove(1); } else vpad_remove(slot);
    return 1;
}
int controller_reconnect(int slot) {
    if (slot!=0 || !connected || (mock_pad.status!=VP_FREE && mock_pad.status!=VP_FAILED)) return 0;
    if (mock_pad.status==VP_FAILED) vpad_remove(0);
    if (unlink(LIBREPAD_STATE_DIR "/pause") && errno!=ENOENT) return 0;
    return vpad_add(0,CONN_USB_WIRED,"Mock gamepad");
}

static void *serve(void *arg)
{ int fd = *(int *)arg; handle(fd); close(fd); return NULL; }

static void request_bytes(const char *raw, size_t len, int status, const char *expected, int split)
{
    int fds[2]; assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    struct timeval timeout = {2, 0};
    setsockopt(fds[0], SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fds[1], SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    pthread_t worker_thread; assert(pthread_create(&worker_thread, NULL, serve, &fds[1]) == 0);
    size_t part = split ? 7 : len;
    assert(send(fds[0], raw, part, 0) == (ssize_t)part);
    if (split) {
        usleep(10000);
        assert(send(fds[0], raw + part, len - part, 0) == (ssize_t)(len - part));
    }
    shutdown(fds[0], SHUT_WR);
    char *response = last_response; size_t used = 0;
    memset(response,0,sizeof(last_response));
    ssize_t n;
    while ((n = recv(fds[0], response + used, sizeof(last_response) - used - 1, 0)) > 0)
        used += (size_t)n;
    close(fds[0]); pthread_join(worker_thread, NULL);
    char prefix[32]; snprintf(prefix, sizeof(prefix), "HTTP/1.1 %d ", status);
    assert(!strncmp(response, prefix, strlen(prefix)));
    assert(strstr(response, expected));
}
static void request(const char *raw, int status, const char *expected, int split)
{ request_bytes(raw, strlen(raw), status, expected, split); }

static void firmware_upload(size_t declared, size_t actual, int valid, const char *headers, int status)
{
    char *raw = calloc(1, actual + 512); assert(raw);
    int n = snprintf(raw, 512, "POST /api/xbox/firmware HTTP/1.1\r\nContent-Length: %zu\r\n%s\r\n", declared, headers);
    assert(n > 0 && n < 512);
    /* Large body includes NUL bytes and spans multiple recv calls. */
    if (valid && actual >= 32) {
        uint32_t ilm = (uint32_t)actual - 33;
        for (int i=0; i<4; i++) raw[n+i]=(char)(ilm >> (8*i));
        raw[n+4]=1;
    }
    request_bytes(raw, (size_t)n + actual, status, "application/json", 1);
    free(raw);
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);
    request("GET /api/state HTTP/1.1\r\nHost: localhost\r\n\r\n", 200, "\"version\":\"" LIBREPAD_VERSION "\"", 0);
    mkdir("/data", 0700); mkdir(LIBREPAD_STATE_DIR, 0700);
    mkdir(LIBREPAD_STATE_DIR "/xbox", 0700);
    unlink(LIBREPAD_XBOX_FIRMWARE_PATH);
    request("GET /api/xbox HTTP/1.1\r\n\r\n",200,"\"firmware\":\"bundled\"",0);
    firmware_upload(70620,70620,1,"X-LibrePad-Control: 1\r\n",200);
    assert(xbox_reloads==1);
    request("GET /api/xbox HTTP/1.1\r\n\r\n",200,"\"firmware\":\"installed\"",0);
    firmware_upload(70620,70620,0,"X-LibrePad-Control: 1\r\n",400);
    firmware_upload(70620,64,1,"X-LibrePad-Control: 1\r\n",400);
    request("POST /api/xbox/firmware HTTP/1.1\r\nContent-Length: 1048577\r\nX-LibrePad-Control: 1\r\n\r\n",400,"Invalid firmware upload length",0);
    request("POST /api/xbox/firmware HTTP/1.1\r\nContent-Length: 32\r\nContent-Length: 32\r\nX-LibrePad-Control: 1\r\n\r\n",400,"Invalid firmware upload length",0);
    request("POST /api/xbox/firmware HTTP/1.1\r\nContent-Length: 32\r\nTransfer-Encoding: chunked\r\nX-LibrePad-Control: 1\r\n\r\n",400,"Invalid firmware upload length",0);
    request("POST /api/xbox/firmware HTTP/1.1\r\nContent-Length: 32\r\n\r\nX-LibrePad-Control: 1\r\n",403,"Missing control header",0);
    assert(xbox_reloads==1);
    struct stat saved; assert(!stat(LIBREPAD_XBOX_FIRMWARE_PATH,&saved) && saved.st_size==70620);
    assert(access(LIBREPAD_XBOX_FIRMWARE_PATH ".tmp",F_OK)!=0);
    unlink(LIBREPAD_XBOX_FIRMWARE_PATH);
    unlink(LIBREPAD_PROFILES_PATH);
    assert(librepad_profiles_init(LIBREPAD_PROFILES_PATH));
    /* Downloads work without a log; only fixed, non-secret files are read. */
    unlink(LIBREPAD_LOG_PATH);
    request("GET /api/diagnostics HTTP/1.1\r\n\r\n",200,"Log file not created yet",0);
    assert(strstr(last_response,"Content-Disposition: attachment; filename=\"LibrePadPS5-diagnostics-" LIBREPAD_VERSION ".txt\""));
    assert(strstr(last_response,"Version: " LIBREPAD_VERSION));
    assert(strstr(last_response,"--- Saved controller mappings ---"));
    FILE *log = fopen(LIBREPAD_LOG_PATH,"wb"); assert(log);
    fputs("startup marker\nlog line: test controller failed\n",log); fclose(log);
    FILE *bond = fopen(LIBREPAD_STATE_DIR "/bluetooth.db","wb"); assert(bond);
    fputs("PRIVATE_BOND_SECRET",bond); fclose(bond);
    request("GET /api/diagnostics HTTP/1.1\r\n\r\n",200,"startup marker",0);
    assert(strstr(last_response,"Complete log at capture time"));
    assert(!strstr(last_response,"PRIVATE_BOND_SECRET"));
    const char *body = strstr(last_response,"\r\n\r\n")+4;
    size_t content_length=0;
    assert(sscanf(strstr(last_response,"Content-Length: "),"Content-Length: %zu",&content_length)==1);
    assert(content_length==strlen(body));
    log=fopen(LIBREPAD_LOG_PATH,"wb"); assert(log);
    fputs("OLDER_ENTRY_MUST_BE_OMITTED\n",log);
    for (int i=0; i<DIAGNOSTIC_LOG_LIMIT+1000; i++) fputc('a',log);
    fputs("\nlatest failure marker\n",log); fclose(log);
    request("GET /api/diagnostics HTTP/1.1\r\n\r\n",200,"latest failure marker",0);
    assert(strstr(last_response,"Log truncated: latest 256 KiB"));
    assert(!strstr(last_response,"OLDER_ENTRY_MUST_BE_OMITTED"));
    body=strstr(last_response,"\r\n\r\n")+4;
    assert(strlen(body)<DIAGNOSTIC_HEADER_LIMIT+DIAGNOSTIC_LOG_LIMIT);
    request("GET /api/diagnostics?path=bluetooth.db HTTP/1.1\r\n\r\n",400,"Unknown request",0);
    unlink(LIBREPAD_LOG_PATH); unlink(LIBREPAD_STATE_DIR "/bluetooth.db");
    mkdir(LIBREPAD_LOG_PATH,0700);
    request("GET /api/diagnostics HTTP/1.1\r\n\r\n",500,"Could not read the payload log",0);
    rmdir(LIBREPAD_LOG_PATH);
    unlink(LIBREPAD_STATE_DIR "/pause");
    mock_pad.status = VP_READY;
    mock_pad.user_id = 0x1234;
    mock_pad.buttons = PAD_BTN_CROSS;
    mock_pad.lx = 128;
    request("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n", 200, "id=\"app\"", 1);
    assert(strstr(PAGE, "<title>LibrePadPS5</title>"));
    assert(strstr(PAGE, "src=\"/dashboard.js\""));
    assert(strstr(PAGE, "href=\"/dashboard.css\""));
    assert(strstr(PAGE, "<noscript>"));
    assert(!strstr(PAGE, "https://")); /* Runtime assets are entirely local. */
    assert(strstr(DASHBOARD_CSS, "backdrop-filter"));
    request("GET /favicon.svg HTTP/1.1\r\nHost: localhost\r\n\r\n", 200, "image/svg+xml", 0);
    request("GET /dashboard.css HTTP/1.1\r\nHost: localhost\r\n\r\n", 200, "text/css", 0);
    request("GET /dashboard.js HTTP/1.1\r\nHost: localhost\r\n\r\n", 200, "Release virtual pad", 0);
    assert(control_header("POST /api/home HTTP/1.1\r\nX-LibrePad-Control: 1\r\n\r\n"));
    assert(control_header("POST /api/home HTTP/1.1\r\nX-FreePad-Control: 1\r\n\r\n")); /* Released clients. */
    assert(!control_header("POST /api/home HTTP/1.1\r\nX-FreePad-Control: 10\r\n\r\n"));
    assert(control_header("POST /api/home HTTP/1.1\r\nx-librepad-control:\t1\r\n\r\n"));
    assert(!control_header("POST /api/home HTTP/1.1\r\nX-LibrePad-Control: 10\r\n\r\n"));
    assert(!control_header("POST /api/home HTTP/1.1\r\nX-LibrePad-Control: 0\r\n\r\n"));
    request("GET /api/state HTTP/1.1\r\n\r\n", 200, "\"user\":\"00001234\"", 0);
    request("GET /api/bluetooth HTTP/1.1\r\n\r\n",200,"Host-only preview",0);
    request("POST /api/bluetooth/pair?address=AA:BB:CC:DD:EE:FF HTTP/1.1\r\n\r\n",403,"Missing control header",0);
    request("POST /api/bluetooth/pair?address=bad HTTP/1.1\r\nX-LibrePad-Control: 1\r\n\r\n",409,"Invalid address",0);
    request("POST /api/bluetooth/disable HTTP/1.1\r\nX-LibrePad-Control: 1\r\n\r\n",200,"shutdown queued",0);
    request("GET /api/profiles HTTP/1.1\r\n\r\n", 200, "0e6f:0184 switch8 triangle,circle,cross", 1);
    request("GET /api/report HTTP/1.1\r\n\r\n", 200, "No reports yet", 0);
    request("POST /api/profiles?data=bad HTTP/1.1\r\n\r\n", 403, "Missing control header", 0);
    request("POST /api/profiles?data=bad HTTP/1.1\r\nX-LibrePad-Control: 1\r\n\r\n", 409, "Release the virtual pad", 0);
    request("POST /api/release HTTP/1.1\r\n\r\n", 403, "Missing control header", 0);
    assert(!removed);
    request("POST /api/release HTTP/1.1\r\nX-PDP-Control: 10\r\n\r\n", 403, "Missing control header", 0);
    request("GET /api/release HTTP/1.1\r\n\r\n", 400, "Unknown request", 0);
    request("POST /api/home HTTP/1.1\r\nX-LibrePad-Control: 1\r\n\r\n", 200, "Home / PS sent", 0);
    assert(pressed == 1);
    request("POST /api/rebind?user=00001234 HTTP/1.1\r\nx-pdp-control:\t1\r\n\r\n", 200, "Profile assigned", 0);
    request("POST /api/rebind?user=1234junk HTTP/1.1\r\nX-PDP-Control: 1\r\n\r\n", 400, "Invalid controller parameters", 0);
    request("POST /api/rebind?user=ffffffff HTTP/1.1\r\nX-PDP-Control: 1\r\n\r\n", 400, "Invalid controller parameters", 0);
    assert(rebound == 1);
    request("POST /api/release HTTP/1.1\r\nX-PDP-Control: 1\r\n\r\n", 200, "Virtual pad released", 0);
    assert(removed == 1 && access(LIBREPAD_STATE_DIR "/pause", F_OK) == 0);
    request("POST /api/profiles?data=%00 HTTP/1.1\r\nX-LibrePad-Control: 1\r\n\r\n", 409, "existing settings unchanged", 1);
    request("POST /api/profiles?data=%z0 HTTP/1.1\r\nX-LibrePad-Control: 1\r\n\r\n", 409, "existing settings unchanged", 0);
    request("POST /api/profiles?data=% HTTP/1.1\r\nX-LibrePad-Control: 1\r\n\r\n", 409, "existing settings unchanged", 0);
    request("POST /api/profiles?data=0e6f%3A0184%20switch8%20triangle%2Ccircle%2Ccross%2Csquare%2Cl1%2Cr1%2Cl2%2Cr2%2Ccreate%2Coptions%2Cl3%2Cr3%2Cps%2Ctouchpad%0A HTTP/1.1\r\nX-LibrePad-Control: 1\r\n\r\n", 200, "Profiles saved and applied", 1);
    request("GET /api/profiles HTTP/1.1\r\n\r\n", 200, "0e6f:0184 switch8 triangle,circle,cross", 0);
    request("POST /api/home HTTP/1.1\r\nX-PDP-Control: 1\r\n\r\n", 409, "not active", 0);
    assert(pressed == 1);
    connected = 0;
    request("POST /api/reconnect HTTP/1.1\r\nX-PDP-Control: 1\r\n\r\n", 409, "unavailable", 0);
    assert(!added && access(LIBREPAD_STATE_DIR "/pause", F_OK) == 0);
    connected = 1;
    request("POST /api/reconnect HTTP/1.1\r\nX-PDP-Control: 1\r\n\r\n", 200, "Creating virtual pad", 0);
    assert(added == 1 && access(LIBREPAD_STATE_DIR "/pause", F_OK) != 0);
    request("POST /api/reconnect HTTP/1.1\r\nX-PDP-Control: 1\r\n\r\n", 409, "unavailable", 0);
    assert(added == 1);
    mock_pad.status = VP_FAILED;
    request("POST /api/reconnect HTTP/1.1\r\nX-PDP-Control: 1\r\n\r\n", 200, "Creating virtual pad", 0);
    assert(added == 2 && removed == 2);
    second_pad.status=VP_READY;
    request("POST /api/home?slot=3 HTTP/1.1\r\nX-LibrePad-Control: 1\r\n\r\n",200,"Home / PS sent",0);
    assert(last_slot==3);
    request("POST /api/home?slot=1 HTTP/1.1\r\nX-LibrePad-Control: 1\r\n\r\n",200,"Home / PS sent",0);
    assert(last_slot==1);
    request("POST /api/release?slot=1 HTTP/1.1\r\nX-LibrePad-Control: 1\r\n\r\n",200,"Virtual pad released",0);
    assert(second_pad.status==VP_FREE && mock_pad.status==VP_QUEUED && added==2);
    request("POST /api/home?slot=4 HTTP/1.1\r\nX-LibrePad-Control: 1\r\n\r\n",400,"Invalid controller parameters",0);
    request("POST /api/home?slot=1&slot=0 HTTP/1.1\r\nX-LibrePad-Control: 1\r\n\r\n",400,"Invalid controller parameters",0);
    request("POST /api/release?slot=-1 HTTP/1.1\r\nX-LibrePad-Control: 1\r\n\r\n",400,"Invalid controller parameters",0);
    request("POST /api/protect?user=65&reserved=1 HTTP/1.1\r\nX-LibrePad-Control: 1\r\n\r\n",200,"reservation saved",0);
    request("POST /api/stop HTTP/1.1\r\nX-PDP-Control: 1\r\n\r\n", 200, "Stopping payload", 0);
    assert(stopped == 1);
    request("GET /api/log HTTP/1.1\r\n\r\n", 200, "text/plain", 0);
    char escaped[64]; escape_json(escaped, sizeof(escaped), "Name\"\\\n");
    assert(!strcmp(escaped, "Name\\\"\\\\ "));
    assert(librepad_web_init(0)); /* Exercise actual socket/thread startup and stop. */
    librepad_web_cleanup();
    unlink(LIBREPAD_PROFILES_PATH);
    puts("LibrePadPS5 dashboard tests passed");
    return 0;
}
