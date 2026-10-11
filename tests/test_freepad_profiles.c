/* Host-only profile tests: disposable container, no console access. */
#define _DEFAULT_SOURCE
#include "freepad_profiles.h"
#include "usb_controllers.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char normal[] = "0e6f:0184 switch8 triangle,circle,cross,square,l1,r1,l2,r2,create,options,l3,r3,ps,touchpad\n";
static const char remapped[] = "0e6f:0184 switch8 square,cross,circle,triangle,l1,r1,r2,l2,create,options,l3,r3,ps,none\n1234:5678 switch8 triangle,circle,cross,square,l1,r1,l2,r2,create,options,l3,r3,ps,touchpad\n";
static void *reader(void *unused)
{
    (void)unused;
    uint8_t data[8] = {4,0,15,128,128,128,128,0};
    for (int i = 0; i < 10000; i++) {
        pad_state_t st;
        assert(freepad_profiles_parse(0x0e6f,0x0184,data,8,&st));
        assert(st.buttons == PAD_BTN_CROSS || st.buttons == PAD_BTN_CIRCLE);
    }
    return NULL;
}
int main(void)
{
    const char *path = "/tmp/freepad-profile-test.conf";
    unlink(path);
    assert(freepad_profiles_init(path));
    assert(freepad_profiles_allowed(0x0e6f,0x0184));
    assert(!freepad_profiles_allowed(0x1234,0x5678));
    char text[FREEPAD_PROFILE_TEXT_MAX], small[4];
    assert(freepad_profiles_export(text,sizeof(text)) && !strcmp(text,normal));
    assert(!freepad_profiles_export(small,sizeof(small)));
    assert(!freepad_profiles_save(""));
    assert(!freepad_profiles_save("054c:0ce6 switch8 triangle,circle,cross,square,l1,r1,l2,r2,create,options,l3,r3,ps,touchpad"));
    assert(!freepad_profiles_save("0e6f:0184 unknown triangle,circle,cross,square,l1,r1,l2,r2,create,options,l3,r3,ps,touchpad"));
    assert(!freepad_profiles_save("0e6f:0184 switch8 triangle,circle,,square,l1,r1,l2,r2,create,options,l3,r3,ps,touchpad"));
    snprintf(text,sizeof(text),"%s%s",normal,normal);
    assert(!freepad_profiles_save(text));
    assert(freepad_profiles_export(text,sizeof(text)) && !strcmp(text,normal));
    uint8_t data[9] = {4,0,15,0,255,128,128,0,0};
    pad_state_t st;
    /* Default profile must exactly preserve the physically verified decoder. */
    for (int bit = 0; bit < 14; bit++) {
        uint8_t report[8] = {0,0,15,10,20,30,40,0};
        report[bit/8] = (uint8_t)(1u << (bit%8));
        pad_state_t original;
        assert(usb_parse_input_report(CTRL_PDP_FACEOFF,report,8,&original));
        assert(freepad_profiles_parse(0x0e6f,0x0184,report,8,&st));
        assert(st.buttons == original.buttons && st.l2 == original.l2 && st.r2 == original.r2);
        assert(st.lx == original.lx && st.ly == original.ly && st.rx == original.rx && st.ry == original.ry);
    }
    assert(freepad_profiles_parse(0x0e6f,0x0184,data,8,&st));
    assert(st.buttons == PAD_BTN_CROSS && st.lx == 0 && st.ly == 255);
    assert(!freepad_profiles_parse(0x0e6f,0x0184,data,7,&st));
    assert(!freepad_profiles_parse(0x0e6f,0x0184,data,9,&st));
    assert(!freepad_profiles_parse(0x1234,0x5678,data,8,&st));
    assert(st.buttons == 0);
    assert(freepad_profiles_save(remapped));
    assert(freepad_profiles_allowed(0x1234,0x5678));
    assert(freepad_profiles_parse(0x0e6f,0x0184,data,8,&st) && st.buttons == PAD_BTN_CIRCLE);
    assert(freepad_profiles_parse(0x1234,0x5678,data,8,&st) && st.buttons == PAD_BTN_CROSS);
    data[0] = 0x40;
    assert(freepad_profiles_parse(0x0e6f,0x0184,data,8,&st) && st.r2 == 255 && st.l2 == 0);
    data[0] = 0; data[1] = 0x20;
    assert(freepad_profiles_parse(0x0e6f,0x0184,data,8,&st) && st.buttons == 0);
    data[1] = 0; data[2] = 1;
    assert(freepad_profiles_parse(0x0e6f,0x0184,data,8,&st) && st.buttons == (PAD_DPAD_UP|PAD_DPAD_RIGHT));
    data[2] = 8;
    assert(!freepad_profiles_parse(0x0e6f,0x0184,data,8,&st));
    assert(freepad_profiles_init(path)); /* Persistent round trip. */
    assert(freepad_profiles_allowed(0x1234,0x5678));
    pthread_t thread; assert(!pthread_create(&thread,NULL,reader,NULL));
    for (int i = 0; i < 30; i++) assert(freepad_profiles_save(i%2 ? normal : remapped));
    pthread_join(thread,NULL);
    freepad_profiles_report(text,sizeof(text)); assert(strstr(text,"0e6f:0184 bytes=8 report="));
    /* Invalid file must fail closed, rather than silently re-enabling IDs. */
    FILE *file = fopen(path,"wb"); assert(file); fputs("bad",file); fclose(file);
    assert(!freepad_profiles_init(path));
    unlink(path);
    assert(freepad_profiles_init("/nonexistent-directory/controllers.conf"));
    assert(!freepad_profiles_save(remapped));
    assert(!freepad_profiles_allowed(0x1234,0x5678));
    puts("FreePad profile tests passed");
    return 0;
}
