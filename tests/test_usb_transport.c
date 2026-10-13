/* Exercise the real PS5 USB reader with device-wide, interleaved completions. */
#define _DEFAULT_SOURCE
#define __PROSPERO__
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>
static int test_ioctl(int fd, unsigned long request, ...);
static int test_usleep(unsigned int microseconds);
static int test_select(int n, fd_set *r, fd_set *w, fd_set *e, struct timeval *t);
#define ioctl test_ioctl
#define usleep test_usleep
#define select test_select
#include "../src/input/usb_device.c"
#include "../src/input/protocol_decoder.c"

static usb_slot_device_t *device;
static int starts[2], queued[8], queue_count, queue_head;
static int input_status, input_length, output_status, fail_start, fail_complete;
static int controls, cancels, timeouts, transfer_logs, fail_output;
static int ds3_fail_request, ds3_short_request, ds3_short_length;
static int ds3_wake_logs, ds3_failure_logs, ds3_input_logs, complete_logs;
static long clock_ms=10000;
static uint8_t packet[64];

long now_ms(void) { return clock_ms; }
size_t librepad_profiles_report_size(uint16_t v, uint16_t p) { (void)v; (void)p; return 64; }
void log_line(const char *format, ...)
{
    if (strstr(format,"transfer failed")) transfer_logs++;
    if (strstr(format,"DS3 USB wake-up completed")) ds3_wake_logs++;
    if (strstr(format,"DS3 USB wake-up failed")) ds3_failure_logs++;
    if (strstr(format,"DS3 first USB input")) ds3_input_logs++;
    if (strstr(format,"USB_FS_COMPLETE failed")) complete_logs++;
}
static int test_usleep(unsigned int us) { clock_ms+=us/1000; return 0; }
static int test_select(int n, fd_set *r, fd_set *w, fd_set *e, struct timeval *t)
{ (void)n; (void)r; (void)w; (void)e; (void)t; clock_ms+=10; return 0; }
static void queue(int index) { assert(queue_count<8); queued[queue_count++]=index; }
static int test_ioctl(int fd, unsigned long request, ...)
{
    assert(fd==device->fd);
    va_list args; va_start(args,request); void *arg=va_arg(args,void *); va_end(args);
    switch (request) {
    case USB_FS_START: {
        int index=((struct usb_fs_start *)arg)->ep_index;
        assert(index<2);
        if (fail_start) { errno=ENXIO; return -1; }
        starts[index]++;
        if (index==1 && !fail_output) queue(1);
        return 0;
    }
    case USB_DO_REQUEST: {
        struct usb_ctl_request *req=arg;
        assert(starts[0]==1); /* Input must be armed before initialization. */
        assert(UGETW(req->ucr_request.wIndex)==device->interface);
        controls++;
        if (device->driver==CTRL_SONY_DS3) {
            assert(controls<=2);
            unsigned length=controls==1 ? 17 : 8;
            assert(req->ucr_request.bmRequestType==0xa1 && req->ucr_request.bRequest==1);
            assert(UGETW(req->ucr_request.wValue)==(controls==1 ? 0x03f2 : 0x03f5));
            assert(UGETW(req->ucr_request.wLength)==length);
            assert(req->ucr_flags==USB_SHORT_XFER_OK && req->ucr_actlen==0);
            for (unsigned i=0; i<length; i++) assert(((uint8_t *)req->ucr_data)[i]==0);
            if (controls==ds3_fail_request) { errno=EPIPE; return -1; }
            req->ucr_actlen=(uint16_t)(controls==ds3_short_request ? ds3_short_length : length);
            memset(req->ucr_data,0x42,(size_t)length);
            if (controls==2) queue(0); /* Input arrives during wake-up. */
            return 0;
        }
        assert(req->ucr_request.bmRequestType==0xc1 && req->ucr_request.bRequest==1);
        errno=EPIPE; return -1; /* Optional vendor request may be unsupported. */
    }
    case USB_FS_COMPLETE: {
        if (fail_complete) { errno=ENXIO; return -1; }
        if (queue_head==queue_count) { errno=EBUSY; return -1; }
        int index=queued[queue_head++];
        ((struct usb_fs_complete *)arg)->ep_index=index;
        if (index==0) {
            device->endpoints[0].status=input_status;
            device->endpoints[0].aFrames=input_status==USB_ERR_TIMEOUT ? 0 : 1;
            device->fs_len[0]=(uint32_t)input_length;
            memcpy(device->rx_buf,packet,sizeof(packet));
        } else device->endpoints[1].status=output_status;
        return 0;
    }
    case USB_FS_STOP: cancels++; return 0;
    default: assert(!"Unexpected ioctl"); return -1;
    }
}
static void reset(usb_slot_device_t *d)
{
    memset(d,0,sizeof(*d)); device=d;
    d->fd=7; d->vid=0x2dc8; d->pid=0x3106; d->driver=CTRL_8BITDO_ADAPTER;
    d->interface=2; d->in_address=0x81; d->out_address=2; d->use_fs=d->has_output=1;
    memset(starts,0,sizeof(starts)); queue_count=queue_head=0;
    controls=cancels=timeouts=transfer_logs=fail_output=fail_start=fail_complete=0;
    ds3_fail_request=ds3_short_request=ds3_short_length=0;
    ds3_wake_logs=ds3_failure_logs=ds3_input_logs=complete_logs=0;
    memset(packet,0,sizeof(packet));
    memcpy(packet,(uint8_t[]){0,20,1,0x10,70,200},6);
    input_status=output_status=USB_ERR_NORMAL_COMPLETION; input_length=20;
    clock_ms=10000;
}
static void reset_ds3(usb_slot_device_t *d)
{
    reset(d);
    d->vid=0x054c; d->pid=0x0268; d->driver=CTRL_SONY_DS3; d->has_output=0;
    input_length=49;
    memset(packet,0,sizeof(packet));
    packet[0]=1; packet[2]=0x10; packet[3]=0x40; packet[4]=1;
    packet[6]=13; packet[7]=27; packet[8]=201; packet[9]=250;
}
static void ds3_initialization(void)
{
    usb_slot_device_t d; uint8_t buffer[64]; pad_state_t state;
    assert(usb_init_controller_handshake(-1,CTRL_SONY_DS3,0)==-1);
    /* Exercise both the stock interface and a nonzero interface. */
    for (int interface=0; interface<=2; interface+=2) {
        reset_ds3(&d); d.interface=(uint8_t)interface;
        assert(wake_device(&d));
        assert(controls==2 && starts[0]==1 && starts[1]==0 && ds3_wake_logs==1);
        assert(!ds3_failure_logs && !ds3_input_logs);
        assert(usb_fs_recv_packet(&d,buffer,sizeof(buffer),15)==49 && starts[0]==1);
        assert(ds3_input_logs==1 && d.input_seen);
        assert(usb_parse_input_report(CTRL_SONY_DS3,buffer,49,&state));
        assert(state.buttons==(PAD_BTN_CROSS|PAD_BTN_PS|PAD_DPAD_UP));
        assert(state.lx==13 && state.ly==27 && state.rx==201 && state.ry==250);
        queue(0);
        assert(usb_fs_recv_packet(&d,buffer,sizeof(buffer),15)==49 && ds3_input_logs==1);
    }
    for (int request=1; request<=2; request++) {
        reset_ds3(&d); ds3_fail_request=request;
        assert(!wake_device(&d) && controls==request);
        assert(ds3_wake_logs==0 && ds3_failure_logs==1);
        /* Empty, short and oversized results must not claim initialization. */
        int expected=request==1 ? 17 : 8;
        int lengths[]={0,expected-1,expected+1};
        for (size_t i=0; i<sizeof(lengths)/sizeof(lengths[0]); i++) {
            reset_ds3(&d); ds3_short_request=request; ds3_short_length=lengths[i];
            assert(!wake_device(&d) && controls==request);
            assert(ds3_wake_logs==0 && ds3_failure_logs==1);
        }
    }
    reset_ds3(&d); fail_start=1;
    assert(!wake_device(&d) && !controls && !ds3_wake_logs);
}
int main(void)
{
    assert(!strcmp(usb_protocol_label(CTRL_8BITDO_ADAPTER,0),"XInput"));
    assert(!strcmp(usb_protocol_label(CTRL_XBOX_XINPUT,1),"Xbox GIP"));
    assert(!strcmp(usb_protocol_label(CTRL_GENERIC_HID,0),"Generic HID"));
    usb_slot_device_t d; uint8_t buffer[64]; pad_state_t state;
    reset(&d);
    queue(0); /* Report arrives during the LED write; it must be preserved. */
    assert(wake_device(&d));
    assert(controls==1 && starts[0]==1 && starts[1]==1 && d.in_ready);
    assert(!memcmp(d.tx_buf,(uint8_t[]){1,3,2},3));
    assert(usb_fs_recv_packet(&d,buffer,sizeof(buffer),15)==20 && starts[0]==1);
    assert(usb_parse_input_report(CTRL_8BITDO_ADAPTER,buffer,20,&state));
    assert((state.buttons&(PAD_BTN_CROSS|PAD_DPAD_UP))==(PAD_BTN_CROSS|PAD_DPAD_UP));
    assert(state.l2==70 && state.r2==200);
    assert(usb_fs_recv_packet(&d,buffer,sizeof(buffer),15)==0 && starts[0]==2);
    assert(usb_fs_recv_packet(&d,buffer,sizeof(buffer),15)==0 && starts[0]==2);

    reset(&d);
    assert(usb_fs_start_input(&d));
    queue(1); /* An output completion never cancels pending input. */
    assert(usb_fs_recv_packet(&d,buffer,sizeof(buffer),15)==0 && d.in_pending);
    queue(0); input_status=USB_ERR_SHORT_XFER;
    assert(usb_fs_recv_packet(&d,buffer,sizeof(buffer),15)==20 && starts[0]==1);

    reset(&d); queue(0); input_status=USB_ERR_TIMEOUT;
    assert(usb_fs_recv_packet(&d,buffer,sizeof(buffer),15)==0 && !d.in_pending);
    queue(0); input_status=USB_ERR_IOERROR;
    assert(usb_fs_recv_packet(&d,buffer,sizeof(buffer),15)==-1 && transfer_logs==1);
    queue(0);
    assert(usb_fs_recv_packet(&d,buffer,sizeof(buffer),15)==-1 && transfer_logs==1);
    clock_ms+=3000; queue(0);
    assert(usb_fs_recv_packet(&d,buffer,sizeof(buffer),15)==-1 && transfer_logs==2);

    reset(&d); queue(0); input_length=65;
    assert(usb_fs_recv_packet(&d,buffer,sizeof(buffer),15)==-1);
    reset(&d); fail_start=1;
    assert(!wake_device(&d) && controls==0);
    reset(&d); fail_complete=1;
    assert(usb_fs_recv_packet(&d,buffer,sizeof(buffer),15)==-1 && complete_logs==1);
    assert(usb_fs_recv_packet(&d,buffer,sizeof(buffer),15)==-1 && complete_logs==1);
    clock_ms+=3000;
    assert(usb_fs_recv_packet(&d,buffer,sizeof(buffer),15)==-1 && complete_logs==2);

    reset(&d); queue(0); fail_output=1;
    assert(wake_device(&d) && cancels==1 && !d.has_output && d.in_ready);
    assert(usb_fs_recv_packet(&d,buffer,sizeof(buffer),15)==20);
    assert(!usb_output(&d,packet,3) && starts[1]==1);
    reset(&d); d.has_output=0;
    assert(wake_device(&d) && controls==1 && starts[0]==1 && !starts[1]);
    ds3_initialization();
    puts("PS5 USB transport: DS3 F2/F5 wake-up, queued input, failed/short transfers, XInput, timeout and error tests passed");
}
