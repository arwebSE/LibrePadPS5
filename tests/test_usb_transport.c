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
static long clock_ms=10000;
static uint8_t packet[20]={0,20,1,0x10,70,200};

long now_ms(void) { return clock_ms; }
size_t librepad_profiles_report_size(uint16_t v, uint16_t p) { (void)v; (void)p; return 64; }
void log_line(const char *format, ...)
{
    if (strstr(format,"transfer failed")) transfer_logs++;
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
        assert(req->ucr_request.bmRequestType==0xc1 && req->ucr_request.bRequest==1);
        assert(UGETW(req->ucr_request.wIndex)==device->interface);
        controls++;
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
    input_status=output_status=USB_ERR_NORMAL_COMPLETION; input_length=20;
    clock_ms=10000;
}
int main(void)
{
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
    assert(usb_fs_recv_packet(&d,buffer,sizeof(buffer),15)==-1);

    reset(&d); queue(0); fail_output=1;
    assert(wake_device(&d) && cancels==1 && !d.has_output && d.in_ready);
    assert(usb_fs_recv_packet(&d,buffer,sizeof(buffer),15)==20);
    assert(!usb_output(&d,packet,3) && starts[1]==1);
    reset(&d); d.has_output=0;
    assert(wake_device(&d) && controls==1 && starts[0]==1 && !starts[1]);
    puts("PS5 USB transport: initialization ordering, queued input, optional output, timeout and error tests passed");
}
