/* Exercise the same USBH_Process called by the foreground loop. The host clock and
 * USB peripheral are simulated. No target board, SOF timer, or USB traffic is needed. */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "usbh_core.h"

static USBH_HandleTypeDef host;
static uint32_t tick;
static uint32_t reset_on_at;
static uint32_t reset_off_at;
static uint32_t connection_at;
static unsigned reset_pulses;
static unsigned connection_events;
static unsigned disconnection_events;
static unsigned open_pipes;
static unsigned foreground_calls;
static int reset_asserted;
static int enable_after_reset;
static const char *scenario;

static void check(int condition, const char *description)
{
    if (!condition) {
        fprintf(stderr, "FAIL %s at tick %" PRIu32 ": %s\n", scenario, tick, description);
        exit(EXIT_FAILURE);
    }
}

static void user_event(USBH_HandleTypeDef *handle, uint8_t event)
{
    (void)handle;
    if (event == HOST_USER_CONNECTION) {
        connection_at = tick;
        ++connection_events;
    } else if (event == HOST_USER_DISCONNECTION) {
        ++disconnection_events;
    }
}

static void start(const char *name, uint32_t now)
{
    scenario = name;
    tick = now;
    memset(&host, 0, sizeof(host));
    reset_asserted = 0;
    enable_after_reset = 1;
    reset_pulses = connection_events = disconnection_events = open_pipes = foreground_calls = 0;
    USBH_Init(&host, user_event, HOST_FS);
    USBH_LL_Connect(&host);
}

static void service(void)
{
    uint32_t entered_at = tick;
    USBH_Process(&host);
    /* HAL_Delay/USBH_Delay consume virtual time. A blocking implementation skips
     * every foreground CAN opportunity in the interval, even on this fast host. */
    if (tick != entered_at) {
        fprintf(stderr, "USB service consumed %" PRIu32 " ms before foreground returned\n",
                tick - entered_at);
    }
    check(tick == entered_at, "USB attach/reset blocked foreground work");
    ++foreground_calls;
}

static void advance(unsigned count)
{
    for (unsigned i = 0; i < count; ++i) {
        service();
        ++tick;
    }
}

static void wait_for_enumeration(void)
{
    for (unsigned i = 0; i < 1000 && host.gState != HOST_ENUMERATION; ++i) {
        service();
        if (host.gState != HOST_ENUMERATION) {
            ++tick;
        }
    }
    check(host.gState == HOST_ENUMERATION, "device failed to reach enumeration");
}

static void attach_sequence(const char *name, uint32_t start_at)
{
    start(name, start_at);
    wait_for_enumeration();
    check(reset_pulses == 1 && !reset_asserted, "expected one completed reset pulse");
    check(reset_on_at - start_at >= 200 && reset_on_at - start_at <= 204,
          "port reset missed 200 ms attach settle");
    check(reset_off_at - reset_on_at >= 100 && reset_off_at - reset_on_at <= 104,
          "reset pulse did not maintain 100 ms assertion");
    check(connection_events == 1 && connection_at - reset_off_at >= 10,
          "connection announced before reset recovery or announced multiple times");
    check(tick - connection_at >= 100 && tick - start_at <= 425,
          "enumeration violated the 100 ms post-connection settle or stalled");
    check(open_pipes == 2 && foreground_calls >= 410,
          "control pipes or foreground scheduling opportunities missing");
    printf("PASS %s\n", name);
}

static void delayed_poll_and_port_enable(void)
{
    start("late foreground poll, reset-generated port-disable IRQ and delayed enable", 0);
    enable_after_reset = 0;
    for (unsigned i = 0; i < 500; ++i) {
        service();  /* Repeated foreground calls in the same millisecond. */
    }
    check(!reset_asserted, "loop count advanced the attach timer");
    tick = 500;  /* Simulate other foreground work missing the settle deadline. */
    advance(5);
    check(reset_asserted, "missed polling deadline collapsed the reset pulse");
    USBH_LL_PortDisabled(&host); /* A reset of a previously enabled port can raise PENCHNG. */
    advance(105);
    check(!reset_asserted && reset_off_at - reset_on_at >= 100,
          "later polling shortened the reset assertion");
    advance(75);
    check(disconnection_events == 0 && connection_events == 0 && open_pipes == 0,
          "reset-generated port disable triggered disconnect or premature enumeration");
    USBH_LL_PortEnabled(&host);
    wait_for_enumeration();
    check(connection_events == 1 && tick - connection_at >= 100 && open_pipes == 2,
          "late enable did not receive the full post-connection settle");
    printf("PASS %s\n", scenario);
}

static void detach_during_reset(int immediate_reconnect)
{
    const char *name = immediate_reconnect ? "disconnect+reconnect before polling" :
                                             "disconnect while reset asserted";
    start(name, 0);
    advance(240);
    check(reset_asserted, "test did not reach reset assertion");
    enable_after_reset = 0;
    USBH_LL_Disconnect(&host);
    if (immediate_reconnect) {
        USBH_LL_Connect(&host);
    }
    advance(5);
    check(disconnection_events == 1 && !reset_asserted, "old attachment did not release reset and disconnect");
    check(connection_events == 0 && open_pipes == 0, "obsolete device advanced to enumeration");
    if (!immediate_reconnect) {
        advance(300);
        check(reset_pulses == 1 && host.gState == HOST_IDLE, "reset restarted with no device");
        USBH_LL_Connect(&host);
    }
    uint32_t earliest_settle = immediate_reconnect ? 240U : tick;
    enable_after_reset = 1;
    wait_for_enumeration();
    check(reset_on_at - earliest_settle >= 200 && reset_pulses == 2,
          "replacement device skipped its attach settle/reset");
    check(connection_events == 1 && open_pipes == 2, "fresh enumeration did not complete");
    printf("PASS %s\n", name);
}

static void failed_reset(void)
{
    start("port-enable timeout, bounded retries and replug after abort", 0);
    enable_after_reset = 0;
    advance(6500);
    check(host.gState == HOST_ABORT_STATE && reset_pulses == 4 && !reset_asserted,
          "failed port enable did not exhaust four attempts");
    check(disconnection_events == 0 && connection_events == 0 && open_pipes == 0,
          "failed resets incorrectly advanced to enumeration");
    USBH_LL_Disconnect(&host);
    advance(2);
    USBH_LL_Connect(&host);
    enable_after_reset = 1;
    wait_for_enumeration();
    check(connection_events == 1 && disconnection_events == 1 && reset_pulses == 5,
          "fresh attach did not recover after retries exhausted");
    printf("PASS %s\n", scenario);
}

/* Hardware boundary. Real core, pipe allocation, requests and USB timing state
 * transitions are linked from the production sources. */
uint32_t USBH_LL_GetTick(void) { return tick; }

void USBH_LL_SetPortReset(USBH_HandleTypeDef *handle, uint8_t asserted)
{
    if (reset_asserted == !!asserted) {
        return;
    }
    reset_asserted = !!asserted;
    if (asserted) {
        ++reset_pulses;
        reset_on_at = tick;
    } else {
        reset_off_at = tick;
        if (handle->device.is_connected && enable_after_reset) {
            USBH_LL_PortEnabled(handle);
        }
    }
}

void USBH_Delay(uint32_t delay) { tick += delay; }
/* Compatibility with the blocking baseline. Its request consumes hardware time. */
USBH_StatusTypeDef USBH_LL_ResetPort(USBH_HandleTypeDef *h)
{
    USBH_LL_SetPortReset(h, 1U);
    USBH_Delay(100U);
    USBH_LL_SetPortReset(h, 0U);
    USBH_Delay(10U);
    return USBH_OK;
}

USBH_StatusTypeDef USBH_LL_Start(USBH_HandleTypeDef *h) { (void)h; return USBH_OK; }
USBH_StatusTypeDef USBH_LL_Stop(USBH_HandleTypeDef *h) { (void)h; return USBH_OK; }
USBH_StatusTypeDef USBH_LL_Init(USBH_HandleTypeDef *h) { (void)h; return USBH_OK; }
USBH_StatusTypeDef USBH_LL_DriverVBUS(USBH_HandleTypeDef *h, uint8_t state)
{ (void)h; (void)state; return USBH_OK; }
USBH_SpeedTypeDef USBH_LL_GetSpeed(USBH_HandleTypeDef *h)
{ (void)h; return USBH_SPEED_FULL; }

USBH_StatusTypeDef USBH_LL_OpenPipe(USBH_HandleTypeDef *h, uint8_t pipe, uint8_t endpoint,
                                    uint8_t address, uint8_t speed, uint8_t type, uint16_t mps)
{
    (void)h; (void)pipe; (void)endpoint; (void)address; (void)speed; (void)type; (void)mps;
    ++open_pipes;
    return USBH_OK;
}
USBH_StatusTypeDef USBH_LL_ClosePipe(USBH_HandleTypeDef *h, uint8_t pipe)
{ (void)h; (void)pipe; return USBH_OK; }
/* These attachment tests stop before any control transfer completes. */
uint32_t USBH_LL_GetLastXferSize(USBH_HandleTypeDef *h, uint8_t pipe)
{
    (void)h;
    (void)pipe;
    return 0;
}

USBH_URBStateTypeDef USBH_LL_GetURBState(USBH_HandleTypeDef *h, uint8_t pipe)
{ (void)h; (void)pipe; return USBH_URB_IDLE; }
USBH_StatusTypeDef USBH_LL_SubmitURB(USBH_HandleTypeDef *h, uint8_t pipe, uint8_t dir,
                                    uint8_t type, uint8_t token, uint8_t *buf, uint16_t size,
                                    uint8_t ping)
{
    (void)h; (void)pipe; (void)dir; (void)type; (void)token; (void)buf; (void)size; (void)ping;
    check(0, "unexpected transfer before enumeration begins");
    return USBH_FAIL;
}

int main(void)
{
    attach_sequence("normal attach with foreground service each millisecond", 100U);
    attach_sequence("attach across HAL tick rollover", UINT32_MAX - 150U);
    delayed_poll_and_port_enable();
    detach_during_reset(0);
    detach_during_reset(1);
    failed_reset();
    return 0;
}
