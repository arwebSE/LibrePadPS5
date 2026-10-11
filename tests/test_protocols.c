#include "input/protocol_decoder.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void log_line(const char *format, ...) { (void)format; }

static void adapter_interfaces(void)
{
    /* Composite receiver: boot keyboard first, then alternate-zero XInput,
     * then a separate HID interface whose endpoints must not be mixed in. */
    uint8_t descriptor[] = {
        9,2,64,0,3,1,0,0x80,50,
        9,4,0,0,1,3,1,1,0, 7,5,0x81,3,8,0,1,
        9,4,1,0,2,0xff,0x5d,1,0,
        7,5,0x82,3,32,0,4, 7,5,0x02,3,32,0,4,
        9,4,2,0,1,3,0,0,0, 7,5,0x83,3,64,0,1
    };
    usb_input_interface_t selected;
    assert(usb_select_input_interface(descriptor,sizeof(descriptor),CTRL_8BITDO_ADAPTER,&selected));
    assert(selected.interface==1 && selected.in_address==0x82 && selected.out_address==2 && !selected.gip);
    assert(usb_select_input_interface(descriptor,sizeof(descriptor),CTRL_XBOX_XINPUT,&selected));
    assert(selected.interface==1);
    assert(usb_select_input_interface(descriptor,sizeof(descriptor),CTRL_GENERIC_HID,&selected));
    assert(selected.interface==2 && selected.in_address==0x83 && !selected.out_address);
    descriptor[15]=descriptor[16]=0; /* Non-boot HID ahead of XInput. */
    assert(usb_select_input_interface(descriptor,sizeof(descriptor),CTRL_8BITDO_ADAPTER,&selected));
    assert(selected.interface==1 && selected.in_address==0x82 && selected.out_address==2);
    assert(usb_select_input_interface(descriptor,sizeof(descriptor),CTRL_GENERIC_HID,&selected));
    assert(selected.interface==0 && selected.in_address==0x81);
    descriptor[15]=descriptor[16]=1;
    /* A vendor interface must also work when it is the only function. */
    assert(usb_select_input_interface(descriptor+25,23,CTRL_8BITDO_ADAPTER,&selected));
    assert(selected.interface==1);
    assert(!usb_select_input_interface(descriptor+25,23,CTRL_GENERIC_HID,&selected));
    descriptor[28]=1; /* Only alternate settings with index zero are supported. */
    assert(!usb_select_input_interface(descriptor+25,23,CTRL_8BITDO_ADAPTER,&selected));
    descriptor[28]=0;
    for (size_t length=1; length<23; length++) {
        if (length==16) continue; /* Complete interface plus IN endpoint. */
        assert(!usb_select_input_interface(descriptor+25,length,CTRL_8BITDO_ADAPTER,&selected));
    }
    descriptor[34]=0; /* Malformed endpoint length. */
    assert(!usb_select_input_interface(descriptor,sizeof(descriptor),CTRL_8BITDO_ADAPTER,&selected));
    assert(!selected.in_address);
    const uint16_t ids[]={0x3106,0x3109,0x310a,0x310b};
    for (size_t i=0; i<sizeof(ids)/sizeof(ids[0]); i++)
        assert(usb_identify_controller(0x2dc8,ids[i],NULL)==CTRL_8BITDO_ADAPTER);
    assert(usb_identify_controller(0x045e,0x028e,NULL)==CTRL_XBOX_XINPUT);
    assert(usb_identify_controller(0x2dc8,0x7777,NULL)==CTRL_GENERIC_HID);
    uint8_t xinput[20]={0,0x14,1,0x10,70,200};
    pad_state_t state;
    assert(usb_parse_input_report(CTRL_8BITDO_ADAPTER,xinput,sizeof(xinput),&state));
    assert((state.buttons & (PAD_BTN_CROSS|PAD_DPAD_UP))==(PAD_BTN_CROSS|PAD_DPAD_UP));
    assert(state.l2==70 && state.r2==200 && state.lx==128 && state.ly==128);
    assert(!usb_parse_input_report(CTRL_8BITDO_ADAPTER,xinput,13,&state));
}

int main(void)
{
    adapter_interfaces();
    uint8_t report[256]={0}; pad_state_t state, previous;
    report[0]=0x30; report[3]=4|0x40; report[4]=1; report[5]=2|0x40;
    assert(usb_parse_input_report(CTRL_NINTENDO_SWITCH_PRO,report,64,&state));
    assert(state.buttons==(PAD_BTN_CROSS|PAD_BTN_R1|PAD_BTN_SHARE|PAD_DPAD_UP|PAD_BTN_L1));
    report[0]=0x81; assert(!usb_parse_input_report(CTRL_NINTENDO_SWITCH_PRO,report,64,&state));
    memset(report,0,sizeof(report)); report[0]=0x20; report[4]=0x10;
    pad_state_neutral(&previous);
    assert(usb_parse_stateful_report(CTRL_XBOX_XINPUT,report,18,&previous,&state));
    assert(state.buttons==PAD_BTN_CROSS);
    uint8_t guide[]={7,0x30,1,2,1,0};
    assert(usb_parse_stateful_report(CTRL_XBOX_XINPUT,guide,6,&previous,&state));
    assert(state.buttons==(PAD_BTN_CROSS|PAD_BTN_PS));
    assert(usb_parse_stateful_report(CTRL_XBOX_XINPUT,report,18,&previous,&state));
    assert(state.buttons==(PAD_BTN_CROSS|PAD_BTN_PS));
    guide[4]=0;
    assert(usb_parse_stateful_report(CTRL_XBOX_XINPUT,guide,6,&previous,&state));
    assert(state.buttons==PAD_BTN_CROSS);
    unsigned seed=123;
    for (int iteration=0; iteration<20000; iteration++) {
        size_t length=(size_t)(iteration%257);
        for (size_t i=0; i<length; i++) { seed=seed*1664525u+1013904223u; report[i]=(uint8_t)(seed>>24); }
        for (int driver=0; driver<=CTRL_PDP_FACEOFF; driver++)
            usb_parse_input_report((usb_controller_type_t)driver,report,length,&state);
    }
    puts("USB protocol and guide-state tests passed");
}
