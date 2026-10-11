/* Mock the entry point to exercise startup failures before any device access. */
#define _DEFAULT_SOURCE
#define __PROSPERO__
#define LIBREPAD_STATE_DIR "build/startup-state"
#define LIBREPAD_LEGACY_STATE_DIR "build/startup-legacy"
#define main payload_main
#include "../src/app/main.c"
#undef main
#include "../src/core/storage.c"
#include <assert.h>
#include <stdarg.h>

static uint32_t firmware = 0x13600001;
static int profiles_ok=1, vpad_ok=1, web_ok=1, usb_starts, cleanups;
static char notifications[4096], logs[4096];
static void append(char *buffer, const char *fmt, va_list args)
{
    size_t used = strlen(buffer);
    vsnprintf(buffer+used,4096-used,fmt,args);
    used = strlen(buffer);
    if (used+1<4096) { buffer[used]='\n'; buffer[used+1]=0; }
}
void notify_ps5(const char *fmt, ...)
{ va_list args; va_start(args,fmt); append(notifications,fmt,args); va_end(args); }
void log_line(const char *fmt, ...)
{ va_list args; va_start(args,fmt); append(logs,fmt,args); va_end(args); }
void log_init(const char *path) { assert(!strcmp(path,LIBREPAD_LOG_PATH)); }
void log_close(void) {}
uint32_t kernel_get_fw_version(void) { return firmware; }
int librepad_profiles_init(const char *path) { assert(!strcmp(path,LIBREPAD_PROFILES_PATH)); return profiles_ok; }
int vpad_init(void) { return vpad_ok; }
void vpad_poll(long now) { (void)now; }
void vpad_cleanup_all(void) { cleanups++; }
int usb_hotplug_init(void) { usb_starts++; return 1; }
void usb_hotplug_poll(long now) { (void)now; librepad_request_stop(); }
void usb_hotplug_cleanup(void) {}
void bluetooth_poll(void) {}
void bluetooth_cleanup(void) {}
int librepad_web_init(int port) { assert(port==8096); return web_ok; }
void librepad_web_cleanup(void) {}
long now_ms(void) { return 10000; }

static void reset(void)
{
    notifications[0]=logs[0]=0; usb_starts=cleanups=0;
    running=1; atomic_store(&stop_requested,0);
}
static void remove_state(void)
{
    unlink(LIBREPAD_LEGACY_STATE_DIR);
    unlink(STATE_DIR "/pdp-user"); unlink(STATE_DIR "/pdp-pad.log");
    unlink(STATE_DIR "/instance.lock"); rmdir(STATE_DIR);
}
int main(void)
{
    mkdir("build",0700); mkdir(STATE_DIR,0700);
    reset(); assert(payload_main()==0);
    assert(strstr(notifications,"starting") && strstr(notifications,"ready"));
    assert(strstr(logs,"0x13600001") && usb_starts==1 && cleanups==1);
    reset(); firmware=0x14000000; assert(payload_main()==1);
    assert(strstr(notifications,"unsupported firmware") && !usb_starts);
    reset(); firmware=0x1360ffff; assert(payload_main()==0 && usb_starts==1);
    reset(); firmware=0; assert(payload_main()==1 && !usb_starts);
    reset(); firmware=0x13600001; profiles_ok=0; assert(payload_main()==1);
    assert(strstr(notifications,"controllers.conf") && !usb_starts);
    reset(); profiles_ok=1; vpad_ok=0; assert(payload_main()==1);
    assert(strstr(notifications,"initialization failed") && !usb_starts);
    reset(); vpad_ok=1; web_ok=0; assert(payload_main()==0);
    assert(strstr(notifications,"dashboard failed") && !strstr(notifications,"ready"));
    reset(); web_ok=1;
    int lock=open(STATE_DIR "/instance.lock",O_CREAT|O_RDWR,0600);
    assert(lock>=0 && flock(lock,LOCK_EX|LOCK_NB)==0);
    assert(payload_main()==1 && !usb_starts);
    assert(strstr(notifications,"already running"));
    close(lock);
    remove_state();
    reset(); assert(payload_main()==0); /* All failure paths release their lock. */
    remove_state();
    puts("Startup: 13.60, firmware rejection, profiles, virtual pads, HTTP and duplicate-instance diagnostics passed");
}
