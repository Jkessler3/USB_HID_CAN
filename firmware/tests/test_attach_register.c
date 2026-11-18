/* Compile the target register-write implementation with only the CPU interrupt
 * intrinsics substituted. A word of local RAM captures the exact HPRT write. */
#include "usbh_core.h"

static uint32_t irq_mask;
static unsigned lock_calls;
static unsigned unlock_calls;

#define __get_PRIMASK() (irq_mask)
#define __disable_irq() do { irq_mask = 1U; ++lock_calls; } while (0)
#define __set_PRIMASK(value) do { irq_mask = (value); ++unlock_calls; } while (0)

#include "usbh_conf.c"

static uint32_t registers[USB_OTG_HOST_PORT_BASE / sizeof(uint32_t) + 1U];

int main(void)
{
    HCD_HandleTypeDef hcd = {0};
    USBH_HandleTypeDef host = {0};
    uint32_t *port = &registers[USB_OTG_HOST_PORT_BASE / sizeof(uint32_t)];
    const uint32_t preserve = USB_OTG_HPRT_PPWR | USB_OTG_HPRT_PSPD_0;
    const uint32_t inhibit = USB_OTG_HPRT_PENA | USB_OTG_HPRT_PCDET |
                             USB_OTG_HPRT_PENCHNG | USB_OTG_HPRT_POCCHNG;

    hcd.Instance = (USB_OTG_GlobalTypeDef *)registers;
    host.pData = &hcd;
    for (uint32_t initial_mask = 0; initial_mask <= 1; ++initial_mask) {
        irq_mask = initial_mask;
        *port = preserve | inhibit;
        USBH_LL_SetPortReset(&host, 1U);
        if (*port != (preserve | USB_OTG_HPRT_PRST) || irq_mask != initial_mask) {
            fprintf(stderr, "FAIL asserting PRST changed port control or interrupt mask\n");
            return EXIT_FAILURE;
        }
        /* Model change bits becoming set during the 100 ms reset pulse. To keep
         * them pending on a real peripheral the subsequent write must contain 0. */
        *port |= inhibit;
        USBH_LL_SetPortReset(&host, 0U);
        if (*port != preserve || irq_mask != initial_mask) {
            fprintf(stderr, "FAIL releasing PRST changed other port bits or interrupt mask\n");
            return EXIT_FAILURE;
        }
    }
    if (lock_calls != 4U || unlock_calls != 4U) {
        fprintf(stderr, "FAIL port writes were not guarded against concurrent HCD IRQ writes\n");
        return EXIT_FAILURE;
    }
    puts("PASS target HPRT reset writes preserve port power, pending IRQ flags and PRIMASK");
    return EXIT_SUCCESS;
}
