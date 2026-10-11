#include "input/protocol_decoder.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void log_line(const char *format, ...) { (void)format; }
int main(void)
{
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
