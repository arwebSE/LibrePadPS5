#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include "bluetooth/controller_host.h"
#include "core/legacy_paths.h"
#include "core/logging.h"
#include "core/time_utils.h"
#include "input/controller_profiles.h"
#include "input/usb_device.h"
#include "input/controller_router.h"
#include "ps5/virtual_controller.h"
#include "host.h"
#include "hci_usb.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

enum command { BT_NONE, BT_PAIR, BT_DISABLE, BT_RECONNECT, BT_FORGET };
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static enum command pending;
static unsigned char requested[6], allowed[6];
static host_t *host;
static hci_t transport;
static int enabled, connected_slot = -1, owns_pad;
static int virtual_slot = -1;
static pad_info info;
static pad_conn_type_t connection=CONN_BLUETOOTH_CLASSIC;
static char status[512] = "{\"enabled\":false,\"connected\":false,\"pairing\":0,\"message\":\"Bluetooth disabled\"}";

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c-'0';
    if (c >= 'a' && c <= 'f') return c-'a'+10;
    if (c >= 'A' && c <= 'F') return c-'A'+10;
    return -1;
}
static int parse_address(const char *text, unsigned char out[6])
{
    if (!text || strlen(text) != 17) return 0;
    unsigned nonzero = 0;
    for (int i = 0; i < 6; i++) {
        int a = hex_digit(text[i*3]), b = hex_digit(text[i*3+1]);
        if (a < 0 || b < 0 || (i < 5 && text[i*3+2] != ':')) return 0;
        out[5-i] = (unsigned char)(a*16+b); nonzero |= out[5-i];
    }
    return nonzero && memcmp(out,"\xff\xff\xff\xff\xff\xff",6) != 0;
}
static int enqueue(enum command command, const unsigned char *address)
{
    pthread_mutex_lock(&mutex);
    int ok = pending == BT_NONE;
    if (ok) { pending = command; if (address) memcpy(requested,address,6); }
    pthread_mutex_unlock(&mutex);
    return ok;
}
int bluetooth_pair_address(const char *address)
{
    unsigned char parsed[6];
    return parse_address(address,parsed) && enqueue(BT_PAIR,parsed);
}
int bluetooth_disable(void) { return enqueue(BT_DISABLE,NULL); }
int bluetooth_reconnect(void) { return enqueue(BT_RECONNECT,NULL); }
int bluetooth_forget(void) { return enqueue(BT_FORGET,NULL); }
int bluetooth_enabled(void)
{
    pthread_mutex_lock(&mutex); int result = enabled; pthread_mutex_unlock(&mutex);
    return result;
}
void bluetooth_status(char *out, size_t size)
{
    pthread_mutex_lock(&mutex); snprintf(out,size,"%s",status); pthread_mutex_unlock(&mutex);
}
static void publish(const char *message)
{
    pthread_mutex_lock(&mutex);
    enabled = host != NULL;
    snprintf(status,sizeof(status),
        "{\"enabled\":%s,\"connected\":%s,\"pairing\":%d,\"address\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"vid\":%u,\"pid\":%u,\"message\":\"%s\"}",
        enabled ? "true":"false", connected_slot >= 0 ? "true":"false",
        host ? host_pairing_left(host):0,allowed[5],allowed[4],allowed[3],allowed[2],allowed[1],allowed[0],
        info.vid,info.pid,message);
    pthread_mutex_unlock(&mutex);
}
static int approve(void *ud, const unsigned char address[6])
{ (void)ud; return memcmp(address,allowed,6) == 0; }
static void idle(void *ud)
{ (void)ud; vpad_poll(now_ms()); }
static int create_pad(void)
{
    if (connected_slot < 0) return 0;
    virtual_slot = controller_attach(&info,connection,"LibrePadPS5 Bluetooth gamepad");
    owns_pad = virtual_slot >= 0;
    return owns_pad;
}
static void on_connect(void *ud, int slot, const pad_info *device)
{
    (void)ud;
    if (connected_slot >= 0) return;
    connected_slot = slot; info = *device;
    /* BLE can replace the selected private address with its bonded identity. */
    memcpy(allowed,device->addr,6);
    connection=CONN_BLUETOOTH_CLASSIC;
    for (int i=0; i<host_paired_count(host); i++) {
        host_paired_info paired;
        if (host_paired_get(host,i,&paired) && !memcmp(paired.addr,allowed,6) && paired.le)
            connection=CONN_BLUETOOTH_LE;
    }
    create_pad();
    log_line("Bluetooth connected: %04x:%04x (%s)",info.vid,info.pid,info.profile);
}
static void on_state(void *ud, int slot, const pad_state *input)
{
    (void)ud;
    if (slot != connected_slot || !owns_pad) return;
    pad_state_t output; pad_state_neutral(&output);
    output.buttons = input->buttons;
    output.lx=input->lx; output.ly=input->ly; output.rx=input->rx; output.ry=input->ry;
    output.l2=input->l2; output.r2=input->r2;
    output.vid=info.vid; output.pid=info.pid; output.connected=1;
    output.conn_type=connection;
    if (input->has_motion) {
        memcpy(output.gyro,input->gyro,sizeof(output.gyro));
        memcpy(output.accel,input->accel,sizeof(output.accel));
    }
    if (input->has_touch) { output.touch_down=input->touch[0].down; output.touch_x=input->touch[0].x; output.touch_y=input->touch[0].y; }
    if (input->has_battery) { output.battery_level=input->battery_pct; output.battery_charging=input->charging; }
    librepad_profiles_remap(output.vid,output.pid,&output);
    controller_update(&info,&output);
}
static void on_disconnect(void *ud, int slot)
{
    (void)ud;
    if (slot != connected_slot) return;
    if (owns_pad) controller_detach(&info);
    owns_pad=0; connected_slot=-1; memset(&info,0,sizeof(info));
}
void bluetooth_cleanup(void)
{
    if (host) { host_close(host); host=NULL; memset(&transport,0,sizeof(transport)); }
    if (transport.ops) { transport.ops->close(transport.ctx); memset(&transport,0,sizeof(transport)); }
    if (owns_pad) controller_detach(&info);
    owns_pad=0; connected_slot=-1; memset(&info,0,sizeof(info));
    publish("Bluetooth disabled");
}
void bluetooth_poll(void)
{
    unsigned char address[6];
    pthread_mutex_lock(&mutex);
    enum command command = pending; pending = BT_NONE; memcpy(address,requested,6);
    pthread_mutex_unlock(&mutex);
    switch (command) {
    case BT_PAIR: {
        vpad_slot_info_t pad={0}; vpad_get_slot_info(0,&pad);
        if (usb_hotplug_librepad_connected() || controller_connected(0) || controller_connected(1) || controller_connected(2) || (!host && pad.status != VP_FREE)) {
            publish("Unplug USB controllers and release the virtual pad first"); break;
        }
    if (host) {
        bluetooth_cleanup();
        vpad_get_slot_info(0,&pad);
        if (pad.status!=VP_FREE) { publish("Wait for virtual pad removal, then pair again"); break; }
    }
        memcpy(allowed,address,6);
        host_events events={.on_connect=on_connect,.on_state=on_state,
            .on_disconnect=on_disconnect,.idle=idle,.allow_device=approve};
        if (!hci_usb_open(&transport)) { publish("Bluetooth transport unavailable or unverified layout"); break; }
        host=host_open(transport,LIBREPAD_STATE_DIR "/bluetooth.db",&events);
        if (!host) { bluetooth_cleanup(); publish("Bluetooth host did not initialize; see log"); break; }
        if (!host_pair(host,120)) { bluetooth_cleanup(); publish("Could not begin pairing; see log"); break; }
        break;
    }
    case BT_DISABLE: bluetooth_cleanup(); return;
    case BT_FORGET: if (host) host_forget(host,allowed); bluetooth_cleanup(); return;
    case BT_RECONNECT:
        if (host && connected_slot >= 0) {
            if (!owns_pad) create_pad();
            if (owns_pad) controller_reconnect(virtual_slot);
            pad_state input;
            pad_info device;
            if (host_pad_get(host,connected_slot,&device,&input)) on_state(NULL,connected_slot,&input);
        }
        break;
    default: break;
    }
    if (host) {
        host_poll(host,0);
        if (host_transport_lost(host)) { bluetooth_cleanup(); publish("Bluetooth transport lost; enable again manually"); return; }
        publish(connected_slot >= 0 ? "Bluetooth controller connected" : "Waiting for the selected controller");
    }
}
