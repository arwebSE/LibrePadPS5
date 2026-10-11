/* Host-only shared ownership tests; no files or console APIs are touched. */
extern "C" {
#include "input/controller_router.h"
#include "ps5/virtual_controller.h"
}
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <cerrno>
#include <cassert>
#include <cstring>
#include <set>
#include <string>
#include <thread>
#include <cstdio>
static std::set<std::string> files;
static int mock_access(const char *path, int) { return files.count(path) ? 0 : -1; }
static int mock_open(const char *path, int, ...) { files.insert(path); return 10; }
static int mock_close(int) { return 0; }
static int mock_unlink(const char *path) { if (files.erase(path)) return 0; errno=ENOENT; return -1; }
#define access mock_access
#define open mock_open
#define close mock_close
#define unlink mock_unlink
#include "../src/input/controller_router.c"
#undef access
#undef open
#undef close
#undef unlink
static vpad_slot_info_t pads[MAX_SLOTS];
static pad_state_t states[MAX_SLOTS];
static int additions[MAX_SLOTS], removals[MAX_SLOTS];
extern "C" int vpad_slot_is_free(int slot) { return pads[slot].status == VP_FREE; }
extern "C" int vpad_add(int slot, pad_conn_type_t type, const char *name) {
    assert(pads[slot].status == VP_FREE); ++additions[slot];
    pads[slot].status=VP_QUEUED; pads[slot].conn_type=type;
    snprintf(pads[slot].name,sizeof(pads[slot].name),"%s",name); return 1;
}
extern "C" void vpad_remove(int slot) { ++removals[slot]; pads[slot]={}; states[slot]={}; }
extern "C" void vpad_get_slot_info(int slot, vpad_slot_info_t *out) { *out=pads[slot]; }
extern "C" void vpad_update(int slot, const pad_state_t *state) { if (pads[slot].status != VP_FREE) states[slot]=*state; }
int main() {
    int pdp, xbox, extra, fourth, overflow;
    assert(controller_attach(&pdp,CONN_USB_WIRED,"PDP") == 0);
    assert(controller_attach(&xbox,CONN_USB_DONGLE_24G,"Xbox") == 1);
    assert(controller_attach(&pdp,CONN_USB_WIRED,"PDP") == 0 && additions[0]==1);
    pad_state_t a,b; pad_state_neutral(&a); pad_state_neutral(&b);
    a.buttons=PAD_BTN_CROSS; b.buttons=PAD_BTN_CIRCLE; a.lx=0; b.lx=255;
    std::thread first([&]{ for(int i=0;i<2000;i++) controller_update(&pdp,&a); });
    std::thread second([&]{ for(int i=0;i<2000;i++) controller_update(&xbox,&b); });
    first.join(); second.join();
    assert(states[0].buttons==PAD_BTN_CROSS && states[0].lx==0);
    assert(states[1].buttons==PAD_BTN_CIRCLE && states[1].lx==255);
    assert(controller_release(0) && controller_connected(0));
    assert(removals[0]==1 && removals[1]==0);
    controller_update(&pdp,&a); assert(pads[0].status==VP_FREE);
    assert(controller_attach(&extra,CONN_USB_WIRED,"Extra") == 2);
    assert(controller_attach(&fourth,CONN_USB_WIRED,"Fourth") == 3);
    assert(controller_attach(&overflow,CONN_USB_WIRED,"Overflow") == -1);
    assert(controller_reconnect(0) && states[0].buttons==PAD_BTN_CROSS);
    assert(additions[0]==2 && additions[1]==1);
    assert(controller_release(-1));
    files.erase(LIBREPAD_STATE_DIR "/pause-0");
    files.erase(LIBREPAD_STATE_DIR "/pause-2"); // Older builds only had a global pause.
    assert(controller_reconnect(1));
    assert(controller_paused(0) && !controller_paused(1) && controller_paused(2));
    controller_detach(&pdp); controller_detach(&extra);
    assert(!controller_connected(0) && controller_connected(1));
    assert(controller_attach(&pdp,CONN_USB_WIRED,"PDP") == 0);
    assert(pads[0].status==VP_FREE); // Unplug/replug retains release policy.
    assert(!controller_release(-2) && !controller_reconnect(MAX_SLOTS));
    controller_detach(&pdp); controller_detach(&xbox); controller_detach(&fourth);
    puts("Shared router: concurrent PDP/Xbox isolation, reserved released slots, capacity, reconnect and pause persistence passed");
}
