/* Real Bluetooth adapter + host core, simulated HCI, mocked PS5. */
#include "../src/bluetooth/controller_host.c"
#include "bluetooth/sim.h"
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
static sim_t *simulator;
static vpad_slot_info_t virtual_pad;
static int usb_present, opened, added, removed, updates;
int hci_usb_open(hci_t *out) { opened++; *out=sim_hci(simulator); return 1; }
long now_ms(void) { return sim_now(); }
int usb_hotplug_freepad_connected(void) { return usb_present; }
void vpad_poll(long now) { (void)now; }
void vpad_get_slot_info(int slot, vpad_slot_info_t *out) { assert(slot==0); *out=virtual_pad; }
int vpad_slot_is_free(int slot) { assert(slot==0); return virtual_pad.status==VP_FREE; }
int vpad_add(int slot, pad_conn_type_t type, const char *name) {
    assert(slot==0 && type==CONN_BLUETOOTH_CLASSIC && name && virtual_pad.status==VP_FREE);
    added++; virtual_pad.status=VP_READY; return 1;
}
void vpad_remove(int slot) { assert(slot==0); removed++; virtual_pad.status=VP_FREE; }
void vpad_update(int slot, const pad_state_t *state) {
    assert(slot==0 && state->vid==0x054c && state->pid==0x05c4); updates++;
}
int controller_attach(void *owner, pad_conn_type_t type, const char *name) {
    (void)owner; if (access(FREEPAD_STATE_DIR "/pause",F_OK)) vpad_add(0,type,name); return 0;
}
void controller_update(void *owner, const pad_state_t *state) { (void)owner; if (virtual_pad.status!=VP_FREE) vpad_update(0,state); }
void controller_detach(void *owner) { (void)owner; if (virtual_pad.status!=VP_FREE) vpad_remove(0); }
int controller_connected(int slot) { (void)slot; return 0; }
int controller_reconnect(int slot) { return vpad_add(slot,CONN_BLUETOOTH_CLASSIC,"Mock Bluetooth gamepad"); }
static void run(int milliseconds) {
    long end=sim_now()+milliseconds;
    while (sim_now()<end) { bluetooth_poll(); host_poll(host,5); }
}
int main(void)
{
    mkdir("/data",0700); mkdir(FREEPAD_STATE_DIR,0700);
    unlink(FREEPAD_STATE_DIR "/bluetooth.db"); unlink(FREEPAD_STATE_DIR "/pause");
    assert(freepad_profiles_init(FREEPAD_STATE_DIR "/adapter-test.conf"));
    assert(!bluetooth_enabled());
    assert(!bluetooth_pair_address("00:00:00:00:00:00"));
    assert(!bluetooth_pair_address("FF:FF:FF:FF:FF:FF"));
    assert(!bluetooth_pair_address("01:02:03:04:05:06junk"));
    assert(!bluetooth_pair_address("GG:02:03:04:05:06"));
    usb_present=1; assert(bluetooth_pair_address("06:05:04:03:02:01"));
    assert(!bluetooth_disable()); /* Pending commands cannot overwrite each other. */
    bluetooth_poll(); assert(!opened && !bluetooth_enabled());
    usb_present=0; virtual_pad.status=VP_READY;
    assert(bluetooth_pair_address("06:05:04:03:02:01")); bluetooth_poll(); assert(!opened);
    virtual_pad.status=VP_FREE;
    sim_pad pad={0}; pad.mode=SIM_WAIT_PAIRING; pad.cod=0x002508; pad.vid=0x054c; pad.pid=0x05c4;
    memcpy(pad.addr,"\x01\x02\x03\x04\x05\x06",6);
    pad.report_len=78; pad.needs_output=1;
    pad.report[0]=0x11; pad.report[1]=0xc0; pad.report[3]=pad.report[4]=pad.report[5]=pad.report[6]=128;
    pad.report[7]=0x28; pad.report[32]=8;
    simulator=sim_new(&pad);
    int pause=open(FREEPAD_STATE_DIR "/pause",O_CREAT|O_WRONLY,0600); assert(pause>=0); close(pause);
    assert(bluetooth_pair_address("06:05:04:03:02:01")); bluetooth_poll(); run(10000);
    assert(bluetooth_enabled() && connected_slot==0 && !added && owns_pad);
    assert(bluetooth_reconnect()); bluetooth_poll(); run(1000);
    assert(added==1 && updates>0 && owns_pad);
    assert(bluetooth_disable()); bluetooth_poll();
    assert(!bluetooth_enabled() && removed==1);
    sim_free(simulator);
    /* No owned Bluetooth pad means shutdown must not remove a native/USB pad. */
    virtual_pad.status=VP_READY; bluetooth_cleanup(); assert(removed==1);
    puts("Bluetooth adapter ownership, pause, queue and address tests passed");
    return 0;
}
