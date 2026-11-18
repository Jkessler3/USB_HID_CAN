/* Real HID short items: Game Pad, unsigned 8-bit X/Y, 64 independent buttons. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "usb_host_hid.h"

static const uint8_t gamepad[] = {
    0x05,1, 0x09,5, 0xA1,1, 0x15,0, 0x26,255,0,
    0x75,8, 0x95,2, 0x09,0x30, 0x09,0x31, 0x81,2,
    0x05,9, 0x19,1, 0x29,64, 0x15,0, 0x25,1, 0x75,1, 0x95,64, 0x81,2, 0xC0
};
static const uint8_t packed[] = {
    0x05,1, 0x09,4, 0xA1,1, 0x85,0x31,
    0x09,0x39, 0x15,0, 0x25,7, 0x75,4, 0x95,1, 0x81,0x42,
    0x09,0x35, 0x16,0x00,0xF8, 0x26,0xFF,0x07, 0x75,12, 0x81,2, 0xC0
};
static const uint8_t full_axis[] = {
    0x05,1, 0x09,4, 0xA1,1, 0x09,0x30, 0x15,0, 0x26,255,255,
    0x75,16, 0x95,1, 0x81,2, 0xC0
};

int main(void)
{
    usb_host_hid_descriptor_t plan;
    usb_host_hid_joystick_state_t state;
    uint8_t data[10] = {0,255,1,0,0,128,1,0,0,128};
    uint8_t changed[sizeof(gamepad)];
    uint8_t numbered[] = {0x31,0x08,0x80};
    uint8_t multi[sizeof(packed)+6];
    assert(usb_host_hid_compile_descriptor(gamepad,sizeof(gamepad),&plan));
    assert(usb_host_hid_decode_input(&plan,data,sizeof(data),123,5,&state));
    assert(state.valid && state.connected && !state.stale && state.sequence==5 && state.timestamp_ms==123);
    assert(state.axes[0]==-32768 && state.axes[1]==32767 && state.axis_count==2);
    assert(state.buttons==UINT64_C(0x8000000180000001) && state.button_count==64);
    data[0]=5; data[1]=128;
    for (unsigned i=0; i<10; i++) {
        assert(usb_host_hid_decode_input(&plan,data,sizeof(data),124,i,&state));
        assert(state.axes[0]<0 && state.axes[1]==0 && state.report_id==0);
    }
    assert(!usb_host_hid_decode_input(&plan,data,1,125,11,&state));
    assert(!usb_host_hid_decode_input(&plan,data,11,125,11,&state));
    assert(usb_host_hid_compile_descriptor(packed,sizeof(packed),&plan));
    assert(usb_host_hid_decode_input(&plan,numbered,3,200,1,&state));
    assert(state.axis_count==6 && state.axes[5]==-32768 && state.axes[0]==0 && state.hats[0]==-1 && state.buttons==0);
    numbered[1]=0xF2; numbered[2]=0x7F;
    assert(usb_host_hid_decode_input(&plan,numbered,3,200,1,&state));
    assert(state.axes[5]==32767 && state.hats[0]==2);
    numbered[0]=0x30;
    assert(!usb_host_hid_decode_input(&plan,numbered,3,200,1,&state));
    assert(!usb_host_hid_decode_input(&plan,numbered,2,200,1,&state));
    assert(usb_host_hid_compile_descriptor(full_axis,sizeof(full_axis),&plan));
    uint8_t mid[]={0,128};
    assert(usb_host_hid_decode_input(&plan,mid,2,0,0,&state) && state.axes[0]==0);
    memcpy(changed,gamepad,sizeof(gamepad)); changed[3]=6; /* Keyboard */
    assert(!usb_host_hid_compile_descriptor(changed,sizeof(changed),&plan));
    changed[3]=2; /* Mouse */
    assert(!usb_host_hid_compile_descriptor(changed,sizeof(changed),&plan));
    assert(!usb_host_hid_compile_descriptor(gamepad,sizeof(gamepad)-1,&plan)); /* Unclosed application */
    memcpy(changed,gamepad,sizeof(changed)); changed[20]=6; /* relative axes */
    assert(!usb_host_hid_compile_descriptor(changed,sizeof(changed),&plan));
    memcpy(changed,gamepad,sizeof(changed)); changed[26]=65; changed[34]=65; /* 65 buttons */
    assert(!usb_host_hid_compile_descriptor(changed,sizeof(changed),&plan));
    memcpy(changed,gamepad,sizeof(changed)); changed[18]=0x30; /* duplicate X */
    assert(!usb_host_hid_compile_descriptor(changed,sizeof(changed),&plan));
    memcpy(multi,packed,sizeof(packed)-1);
    const uint8_t extra[]={0x85,0x32,0x09,0x30,0x81,2,0xC0};
    memcpy(multi+sizeof(packed)-1,extra,sizeof(extra));
    assert(!usb_host_hid_compile_descriptor(multi,sizeof(multi),&plan));
    assert(!usb_host_hid_decode_input(&plan,numbered,3,200,1,&state)); /* failed layout cannot decode */
    /* A numbered Feature cannot give an implicit-ID Input an unambiguous framing. */
    uint8_t mixed[sizeof(full_axis)+6];
    memcpy(mixed,full_axis,sizeof(full_axis)-1);
    const uint8_t feature[]={0x85,7,0x09,0x30,0xB1,2,0xC0};
    memcpy(mixed+sizeof(full_axis)-1,feature,sizeof(feature));
    assert(!usb_host_hid_compile_descriptor(mixed,sizeof(mixed),&plan));

    uint8_t four_way[sizeof(packed)];
    memcpy(four_way,packed,sizeof(packed)); four_way[13]=3;
    assert(!usb_host_hid_compile_descriptor(four_way,sizeof(four_way),&plan));

    /* Trimmed logical range: reject out-of-domain input, preserve prior decoded values. */
    uint8_t limited[sizeof(full_axis)];
    memcpy(limited,full_axis,sizeof(full_axis)); limited[11]=100; limited[12]=0;
    assert(usb_host_hid_compile_descriptor(limited,sizeof(limited),&plan));
    uint8_t legal[]={50,0}, outside[]={101,0};
    assert(usb_host_hid_decode_input(&plan,legal,2,123,9,&state));
    assert(!usb_host_hid_decode_input(&plan,outside,2,124,10,&state));
    assert(state.axes[0]==0 && state.sequence==9);

    usb_host_hid_device_info_t device;
    usb_host_hid_device_reset(&device);
    usb_host_hid_on_connection(&device,0);
    usb_host_hid_on_class_active(&device,0);
    usb_host_hid_on_report(&device,100);
    usb_host_hid_joystick_apply_stale_timeout(&device,351,&state);
    assert(state.connected && state.stale && !state.valid);
    puts("HID control/profile regressions passed");
    return 0;
}
