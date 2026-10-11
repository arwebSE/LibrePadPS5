/* Host-only regression tests for the LibrePadPS5 virtual-pad lifecycle. */
#define _DEFAULT_SOURCE
#define __PROSPERO__
#include "../src/ps5/virtual_controller.c"
#include <assert.h>

enum { NATIVE_USER = 100, CONTROLLER_USER = 200, OWN_HANDLE = 0x4030d };
static int32_t users[4] = {NATIVE_USER, -1, -1, -1};
static int native_mode = -1, binds, deletes, disconnects, insertions, bind_result;
static int login_result, allow_add, additions, delete_result;
static uint32_t inserted_buttons;
static int32_t bound_user;

void log_line(const char *fmt, ...) { (void)fmt; }
void notify_ps5(const char *fmt, ...) { (void)fmt; }
int elevate_privileges(void) { return 1; }
void restore_privileges(void) {}
int shellui_press_ps_button(int32_t handle) { (void)handle; return -1; }
int shellui_remote_bind_device(uint64_t dev, int32_t user)
{ (void)dev; (void)user; assert(!"Remote binding forbidden"); return -1; }
int shellui_remote_disconnect_device(uint64_t dev) { (void)dev; return -1; }
int shellcore_vda(int *code) { (void)code; return -1; }
int32_t sceUserServiceInitialize(void *p) { (void)p; return 0; }
int32_t sceUserServiceGetInitialUser(int32_t *user) { *user = NATIVE_USER; return 0; }
int32_t sceUserServiceGetForegroundUser(int32_t *user) { *user = NATIVE_USER; return 0; }
int32_t sceUserServiceGetLoginUserIdList(int32_t out[4]) { memcpy(out, users, sizeof(users)); return login_result; }
int32_t sceUserServiceGetUserStatus(int32_t user, int32_t *status)
{ (void)user; *status = 1; return 0; }
int32_t scePadInit(void) { return 0; }
int32_t scePadGetHandle(int32_t user, int32_t type, int32_t index)
{ (void)user; assert(type == 0); assert(index == 0); return native_mode < 0 ? -1 : 9; }
int32_t scePadReadState(int32_t handle, PadData *data)
{ assert(handle == 9); data->connected = native_mode == 1; return 0; }
int32_t scePadSetProcessPrivilege(int32_t p) { (void)p; return 0; }
int32_t scePadVirtualDeviceAddDevice(void *param, int32_t type)
{
    assert(allow_add && type==VIRTUAL_DEVICE_DUALSENSE);
    const int32_t *fields=param;
    assert(fields[0]==32 && fields[1]==1);
    additions++; return (int32_t)0x803b0006u;
}
int32_t scePadVirtualDeviceInsertData(int32_t handle, const void *raw)
{
    assert(handle >= OWN_HANDLE && handle < OWN_HANDLE+64);
    const PadData *data = raw;
    assert(data->connected && data->orient_w == 1.0f);
    insertions++; inserted_buttons = data->buttons; return 0;
}
int32_t scePadVirtualDeviceDeleteDevice(int32_t handle)
{ assert(handle >= OWN_HANDLE && handle < OWN_HANDLE+64); deletes++; return delete_result; }
uint64_t sceKernelGetProcessTime(void) { return 0; }
static int32_t bind_pad(uint64_t dev, int32_t user)
{
    assert(dev >= OWN_HANDLE && dev < OWN_HANDLE+64 && user != NATIVE_USER && user != 101);
    binds++; bound_user = user; return bind_result;
}
static int32_t disconnect_pad(uint64_t dev)
{ assert(dev >= OWN_HANDLE && dev < OWN_HANDLE+64); disconnects++; return 0; }

static void logout_tests(void)
{
    memset(g_slots,0,sizeof(g_slots)); g_pending=-1; g_controller_user=0;
    g_last_profile_check=-1; g_had_logged_users=g_empty_login_checks=0;
    long base=now_ms();
    for (int i=0; i<2; i++) {
        g_slots[i].status=VP_READY; g_slots[i].handle=OWN_HANDLE+i;
        g_slots[i].device_id=OWN_HANDLE+i; g_slots[i].user_id=i ? -1 : CONTROLLER_USER;
        g_slots[i].conn_type=CONN_USB_WIRED;
        snprintf(g_slots[i].name,sizeof(g_slots[i].name),"Pad %d",i);
        pad_state_neutral(&g_slots[i].pad_state);
        g_slots[i].pad_state.buttons=PAD_BTN_CROSS;
    }
    int before_delete=deletes, before_disconnect=disconnects;
    for (int i=0; i<4; i++) users[i]=-1;
    vpad_poll(base); vpad_poll(base+500);
    assert(deletes==before_delete); /* Starting at the login screen is not logout. */
    users[0]=NATIVE_USER; users[1]=CONTROLLER_USER;
    vpad_poll(base+1000);
    users[0]=-1; vpad_poll(base+1500);
    assert(deletes==before_delete); /* One remaining player keeps both pads. */
    users[1]=-1; login_result=-1; vpad_poll(base+2000);
    assert(deletes==before_delete); /* An API error must not imply logout. */
    login_result=0; vpad_poll(base+2500);
    assert(deletes==before_delete);
    users[1]=CONTROLLER_USER; vpad_poll(base+3000);
    users[1]=-1; vpad_poll(base+3500);
    assert(deletes==before_delete); /* A single empty snapshot was transient. */
    vpad_poll(base+4000);
    assert(deletes==before_delete+2 && disconnects==before_disconnect+2);
    for (int i=0; i<2; i++) {
        assert(g_slots[i].status==VP_QUEUED && !g_slots[i].device_id && g_slots[i].handle==-1);
        assert(g_slots[i].conn_type==CONN_USB_WIRED && strstr(g_slots[i].name,"Pad"));
        assert(!g_slots[i].pad_state.buttons && !g_slots[i].ps_until);
    }
    assert(g_slots[2].status==VP_FREE && g_slots[3].status==VP_FREE);
    assert(g_protected_user==NATIVE_USER && g_reserved_users[0]==101);

    /* Exercise serialized AddDevice and fresh ownership discovery through a
     * local fake klog server. The physical transport is not reconnected. */
    int server=socket(AF_INET,SOCK_STREAM,0); assert(server>=0);
    int reuse=1; assert(!setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse)));
    struct sockaddr_in address={0}; address.sin_family=AF_INET;
    address.sin_port=htons(3232); address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    assert(!bind(server,(struct sockaddr *)&address,sizeof(address)) && !listen(server,2));
    allow_add=1;
    for (int slot=0; slot<2; slot++) {
        vpad_poll(base+4004+slot*400);
        assert(g_pending==slot && g_slots[slot].status==VP_PENDING && additions==slot+1);
        int client=accept(server,NULL,NULL); assert(client>=0);
        if (!slot) {
            pad_state_t state; pad_state_neutral(&state); state.buttons=PAD_BTN_PS;
            vpad_update(slot,&state); /* A fresh Home press survives identification. */
        }
        char line[96]; int length=snprintf(line,sizeof(line),
            "DEVICE_ADDED DeviceId=0x%x subType=22\n",OWN_HANDLE+16+slot);
        assert(write(client,line,(size_t)length)==length);
        vpad_poll(base+4308+slot*400);
        assert(g_slots[slot].status==VP_READY && g_slots[slot].handle==OWN_HANDLE+16+slot);
        if (!slot) assert(inserted_buttons & PAD_BTN_PS);
        close(client);
    }
    close(server); allow_add=0;
    for (int i=0; i<10; i++) vpad_poll(base+5000+i*500);
    assert(additions==2 && deletes==before_delete+2); /* Empty login screen never loops. */
    users[0]=CONTROLLER_USER; vpad_poll(base+10500);
    assert(vpad_rebind_user(0,CONTROLLER_USER));
    assert(!vpad_rebind_user(1,NATIVE_USER));

    /* Failed deletion must not create another pad over the old registration. */
    int old_handle=g_slots[0].handle;
    g_slots[0].restart_after_logout=1; delete_result=-1;
    recover_logout_locked(0);
    assert(g_slots[0].status==VP_FAILED && g_slots[0].handle==old_handle);
    assert(!g_slots[0].restart_after_logout && additions==2);
    delete_result=0;

    /* An in-flight device cannot be deleted until ownership is identified.
     * Explicit release during identification cancels automatic recovery. */
    for (int i=0; i<4; i++) users[i]=-1;
    g_slots[2].status=VP_PENDING; g_slots[2].handle=-1;
    g_had_logged_users=1; g_empty_login_checks=1;
    check_logout_locked(); int count=deletes;
    recover_logout_locked(2);
    assert(g_slots[2].restart_after_logout && deletes==count);
    g_slots[2].remove_when_found=1; g_pending=2; g_matches=1; g_match_dev=OWN_HANDLE+18;
    g_t_add=now_ms()-T_IDENTIFY-1; g_t_match=now_ms()-T_AMBIGUITY-1;
    finish_identification(now_ms()); recover_logout_locked(2);
    assert(g_slots[2].status==VP_FREE && deletes==count+1 && additions==2);
}

static void pending(int matches)
{
    memset(g_slots, 0, sizeof(g_slots));
    g_slots[0].status = VP_PENDING;
    g_slots[0].handle = -1;
    pad_state_neutral(&g_slots[0].pad_state);
    g_pending = 0; g_matches = matches; g_match_dev = OWN_HANDLE;
    g_t_add = now_ms() - T_IDENTIFY - 1; g_t_match = now_ms() - T_AMBIGUITY - 1;
}

int main(void)
{
    assert(parse_device_id_from_klog("DEVICE_ADDED DeviceId=0x4030d subType=22") == OWN_HANDLE);
    assert(parse_device_id_from_klog("DEVICE_ADDED deviceId=0x4030d subType=0x16") == OWN_HANDLE);
    assert(parse_device_id_from_klog("DEVICE_ADDED dev_id=0x4030d subType=22") == OWN_HANDLE);
    assert(!parse_device_id_from_klog("DEVICE_ADDED DeviceId=0x4030d"));
    assert(!parse_device_id_from_klog("DEVICE_ADDED DeviceId=0x4030d subType=12"));
    assert(!parse_device_id_from_klog("DEVICE_ADDED DeviceId=0x4030d subType=220"));
    assert(!parse_device_id_from_klog("DEVICE_ADDED DeviceId=0x4030d subType=22junk"));
    assert(vpad_native_controller_connected(NATIVE_USER) == -1);
    native_mode = 0; assert(vpad_native_controller_connected(NATIVE_USER) == 0);
    native_mode = 1; assert(vpad_native_controller_connected(NATIVE_USER) == 1);
    native_mode = -1;
    g_ready = 1; g_protected_user = NATIVE_USER; g_controller_user = CONTROLLER_USER;
    g_bind = bind_pad; g_disconnect = disconnect_pad;
    assert(get_user_id_for_slot(0) == -1); /* Controller profile not signed in. */
    pending(1); finish_identification(now_ms());
    assert(g_slots[0].status == VP_READY && g_slots[0].user_id == -1 && !binds);
    users[1] = CONTROLLER_USER;
    vpad_poll(now_ms());
    assert(binds == 1 && bound_user == CONTROLLER_USER && g_slots[0].user_id == CONTROLLER_USER);
    assert(!vpad_rebind_user(0, NATIVE_USER));
    assert(binds == 1);
    assert(vpad_rebind_user(0, CONTROLLER_USER));
    bind_result = -1;
    assert(!vpad_rebind_user(0, CONTROLLER_USER));
    assert(g_slots[0].user_id == CONTROLLER_USER);
    bind_result = 0;
    pad_state_t input; pad_state_neutral(&input); input.buttons = PAD_BTN_CROSS;
    vpad_update(0, &input); vpad_press_ps_button(0); vpad_poll(now_ms());
    assert(inserted_buttons == (PAD_BTN_CROSS | PAD_BTN_PS));
    vpad_poll(now_ms() + 200); assert(inserted_buttons == PAD_BTN_CROSS);
    vpad_remove(0);
    assert(deletes == 1 && disconnects == 1 && g_slots[0].status == VP_FREE);
    pending(1); vpad_remove(0); assert(g_slots[0].remove_when_found);
    finish_identification(now_ms());
    assert(deletes == 2 && g_slots[0].status == VP_FREE);
    int previous_binds = binds;
    pending(2); finish_identification(now_ms());
    assert(g_slots[0].status == VP_FAILED && deletes == 2 && binds == previous_binds);
    assert(insertions > 0);
    /* Two native users plus two independent virtual players. */
    memset(g_slots,0,sizeof(g_slots));
    users[2]=101; users[3]=201; g_reserved_users[0]=101;
    g_slots[0].status=VP_READY; g_slots[0].handle=OWN_HANDLE;
    g_slots[0].device_id=OWN_HANDLE; g_slots[0].user_id=CONTROLLER_USER;
    g_slots[1].status=VP_READY; g_slots[1].handle=OWN_HANDLE+1;
    g_slots[1].device_id=OWN_HANDLE+1; g_slots[1].user_id=-1;
    assert(vpad_user_is_protected(NATIVE_USER) && vpad_user_is_protected(101));
    assert(get_user_id_for_slot(1)==-1);
    assert(!vpad_rebind_user(1,CONTROLLER_USER));
    assert(!vpad_rebind_user(1,101));
    assert(vpad_rebind_user(1,201));
    assert(g_slots[0].user_id==CONTROLLER_USER && g_slots[1].user_id==201);
    input.buttons=PAD_BTN_CROSS; vpad_update(0,&input);
    input.buttons=PAD_BTN_CIRCLE; vpad_update(1,&input);
    assert(g_slots[0].pad_state.buttons==PAD_BTN_CROSS && g_slots[1].pad_state.buttons==PAD_BTN_CIRCLE);
    /* All four profiles may be virtual when no signed-in native profile is reserved. */
    users[0]=CONTROLLER_USER; users[1]=201; users[2]=202; users[3]=203;
    for (int slot=2; slot<MAX_SLOTS; slot++) {
        g_slots[slot].status=VP_READY; g_slots[slot].handle=OWN_HANDLE+slot;
        g_slots[slot].device_id=OWN_HANDLE+slot; g_slots[slot].user_id=-1;
        assert(vpad_rebind_user(slot,users[slot]));
    }
    assert(g_slots[3].user_id==203 && !vpad_rebind_user(0,203));
    users[0]=NATIVE_USER; users[1]=CONTROLLER_USER; users[2]=101; users[3]=201;
    pending(1); input.buttons=PAD_BTN_PS; vpad_update(0,&input);
    finish_identification(now_ms());
    assert(g_slots[0].status==VP_READY && g_slots[0].ps_until > now_ms());
    logout_tests();
    puts("LibrePadPS5 lifecycle tests passed");
    return 0;
}
