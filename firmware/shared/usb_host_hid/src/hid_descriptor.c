#include "usb_host_hid.h"

#include <string.h>

/* HID 1.11 short items. A profile is compiled once at enumeration. */
#define USAGE_DESKTOP(id) (0x00010000UL | (id))
#define APPLICATION_JOYSTICK USAGE_DESKTOP(4U)
#define APPLICATION_GAMEPAD  USAGE_DESKTOP(5U)
#define MAX_COLLECTION_DEPTH 8U
#define MAX_GLOBAL_DEPTH 4U

typedef struct {
    uint16_t page;
    uint16_t size;
    uint16_t count;
    uint8_t report_id;
    int32_t minimum;
    uint32_t max_item;
    uint8_t max_bytes;
} globals_t;

typedef struct {
    uint32_t usages[USB_HOST_HID_MAX_BUTTONS];
    uint32_t first;
    uint32_t last;
    unsigned count;
    bool has_first;
    bool has_last;
} locals_t;

static int32_t signed_item(uint32_t value, unsigned bits)
{
    if (bits && (value & (UINT32_C(1) << (bits - 1U)))) {
        return (int32_t)((int64_t)value - (INT64_C(1) << bits));
    }
    return (int32_t)value;
}

static uint32_t usage_at(const locals_t *local, unsigned index)
{
    if (local->has_first && local->has_last && index <= local->last - local->first) {
        return local->first + index;
    }
    return index < local->count ? local->usages[index] : 0U;
}

static bool set_local(locals_t *local, uint8_t tag, uint32_t usage)
{
    switch (tag) {
    case 0: /* Usage */
        if (local->has_first || local->count >= USB_HOST_HID_MAX_BUTTONS) {
            return false;
        }
        local->usages[local->count++] = usage;
        break;
    case 1: /* Usage Minimum */
        if (local->count || local->has_first) {
            return false;
        }
        local->first = usage;
        local->has_first = true;
        break;
    case 2: /* Usage Maximum */
        if (!local->has_first || local->has_last || usage < local->first ||
            (usage >> 16) != (local->first >> 16)) {
            return false;
        }
        local->last = usage;
        local->has_last = true;
        break;
    case 3: case 4: case 5: case 7: case 8: case 9: /* Designator/string metadata */
        break;
    default: /* Including alternative usage sets (Delimiter). */
        return false;
    }
    return true;
}

static bool set_global(globals_t *global, uint8_t tag, uint32_t value, unsigned bytes)
{
    switch (tag) {
    case 0: /* Usage Page */
        if (value > UINT16_MAX) { return false; }
        global->page = (uint16_t)value;
        break;
    case 1: global->minimum = signed_item(value, bytes * 8U); break;
    case 2: global->max_item = value; global->max_bytes = (uint8_t)bytes; break;
    case 3: case 4: case 5: case 6: break; /* Physical limits, unit metadata */
    case 7: /* Report Size */
        if (value > USB_HOST_HID_MAX_REPORT_LEN * 8U) { return false; }
        global->size = (uint16_t)value;
        break;
    case 8: /* Report ID; zero is reserved. */
        if (!value || value > UINT8_MAX) { return false; }
        global->report_id = (uint8_t)value;
        break;
    case 9: /* Report Count */
        if (value > USB_HOST_HID_MAX_REPORT_LEN * 8U) { return false; }
        global->count = (uint16_t)value;
        break;
    default: return false;
    }
    return true;
}

static bool add_field(usb_host_hid_descriptor_t *plan, const globals_t *global,
                      uint32_t usage, uint16_t offset, uint32_t flags)
{
    usb_host_hid_bitfield_t *field;
    int32_t maximum;
    unsigned index;
    if (usage >= USAGE_DESKTOP(0x30) && usage <= USAGE_DESKTOP(0x37)) {
        index = (unsigned)(usage - USAGE_DESKTOP(0x30));
        field = &plan->axes[index];
        if (plan->axis_count <= index) { plan->axis_count = (uint8_t)(index + 1U); }
    } else if (usage == USAGE_DESKTOP(0x39)) {
        field = &plan->hat;
    } else if ((usage >> 16) == 9U) {
        unsigned button = usage & UINT16_MAX;
        if (!button || button > USB_HOST_HID_MAX_BUTTONS || global->size != 1U ||
            global->minimum != 0 || global->max_item != 1U || plan->button_bits[button - 1U]) {
            return false;
        }
        plan->button_bits[button - 1U] = (uint16_t)(offset + 1U);
        if (plan->button_count < button) { plan->button_count = (uint8_t)button; }
        return true;
    } else {
        /* A sized, absolute variable on a vendor page occupies bits but supplies no controls. */
        return (usage >> 16) >= 0xFF00U;
    }
    if (field->size || global->size > 16U || !global->max_bytes) { return false; }
    if (global->minimum < 0) {
        int32_t sign = INT32_C(1) << (global->size - 1U);
        maximum = signed_item(global->max_item, global->max_bytes * 8U);
        if (global->minimum < -sign || maximum >= sign) { return false; }
    } else {
        if (global->max_item >= (UINT32_C(1) << global->size)) { return false; }
        maximum = (int32_t)global->max_item;
    }
    if (global->minimum >= maximum) { return false; }
    if (field == &plan->hat && maximum - global->minimum != 7) { return false; }
    field->offset = offset;
    field->size = (uint8_t)global->size;
    field->minimum = global->minimum;
    field->maximum = maximum;
    field->null_state = (flags & 0x40U) != 0;
    return true;
}

static bool add_input(usb_host_hid_descriptor_t *plan, const globals_t *global,
                      const locals_t *local, uint32_t flags)
{
    uint32_t end = plan->payload_bits + (uint32_t)global->size * global->count;
    unsigned capacity = (USB_HOST_HID_MAX_REPORT_LEN - (global->report_id != 0U)) * 8U;
    if (!global->size || !global->count || end > capacity ||
        (plan->payload_bits && plan->report_id != global->report_id)) { return false; }
    plan->report_id = global->report_id;
    plan->has_report_id = global->report_id != 0U;
    if ((flags & 1U) == 0U) {
        /* Variable, absolute, bit-field data. No relative, array, wrap or buffered-byte controls. */
        if ((flags & ~UINT32_C(0x62)) || !(flags & 2U)) { return false; }
        if (local->has_first != local->has_last) { return false; }
        for (unsigned i = 0; i < global->count; i++) {
            uint16_t offset = (uint16_t)(plan->payload_bits + i * global->size);
            if (!add_field(plan, global, usage_at(local, i), offset, flags)) { return false; }
        }
    }
    plan->payload_bits = (uint16_t)end;
    return true;
}

static bool compile_items(const uint8_t *data, uint16_t length, usb_host_hid_descriptor_t *plan)
{
    globals_t global = {0}, stack[MAX_GLOBAL_DEPTH];
    locals_t local = {0};
    unsigned pos = 0, depth = 0, stack_used = 0;
    bool have_application = false;
    bool saw_report_id = false;

    while (pos < length) {
        uint8_t prefix = data[pos++];
        unsigned bytes = prefix & 3U;
        unsigned type = (prefix >> 2) & 3U;
        uint8_t tag = prefix >> 4;
        uint32_t value = 0;
        if (bytes == 3U) { bytes = 4U; }
        if (prefix == 0xFEU || pos + bytes > length) { return false; }
        for (unsigned i = 0; i < bytes; i++) {
            value |= (uint32_t)data[pos++] << (8U * i);
        }
        switch (type) {
        case 0: /* Main */
            switch (tag) {
            case 10: /* Collection */
                if (!depth) {
                    uint32_t app_usage = usage_at(&local, 0);
                    if (have_application || value != 1U ||
                        (app_usage != APPLICATION_JOYSTICK && app_usage != APPLICATION_GAMEPAD)) {
                        return false;
                    }
                    have_application = true;
                } else if (value != 0U && value != 2U) { /* Physical or logical nested collection */
                    return false;
                }
                if (++depth > MAX_COLLECTION_DEPTH) { return false; }
                break;
            case 12: /* End Collection */
                if (!depth || bytes) { return false; }
                depth--;
                break;
            case 8: /* Input */
                if (!depth || !add_input(plan, &global, &local, value)) { return false; }
                break;
            case 9: case 11: /* Output / Feature have separate bit offsets. */
                if (!depth) { return false; }
                break;
            default: return false;
            }
            memset(&local, 0, sizeof(local));
            break;
        case 1: /* Global */
            if (tag == 8) { saw_report_id = true; }
            if (tag == 10) { /* Push */
                if (bytes || stack_used == MAX_GLOBAL_DEPTH) { return false; }
                stack[stack_used++] = global;
            } else if (tag == 11) { /* Pop */
                if (bytes || !stack_used) { return false; }
                global = stack[--stack_used];
            } else if (!bytes || !set_global(&global, tag, value, bytes)) {
                return false;
            }
            break;
        case 2: /* Local */
            if (!bytes || !set_local(&local, tag, bytes == 4U ? value : ((uint32_t)global.page << 16) | value)) {
                return false;
            }
            break;
        default: return false;
        }
    }
    return have_application && !depth && !stack_used &&
           (!saw_report_id || plan->has_report_id) &&
           (plan->axis_count || plan->button_count || plan->hat.size);
}

bool usb_host_hid_compile_descriptor(const uint8_t *descriptor, uint16_t length, usb_host_hid_descriptor_t *plan)
{
    if (!plan) { return false; }
    memset(plan, 0, sizeof(*plan));
    if (!descriptor || !length || !compile_items(descriptor, length, plan)) {
        memset(plan, 0, sizeof(*plan));
        return false;
    }
    return true;
}

static uint32_t report_bits(const uint8_t *payload, uint16_t offset, uint8_t size)
{
    uint32_t result = 0;
    for (unsigned i = 0; i < size; i++) {
        unsigned bit = offset + i;
        result |= (uint32_t)((payload[bit / 8U] >> (bit % 8U)) & 1U) << i;
    }
    return result;
}

static int32_t field_value(const usb_host_hid_bitfield_t *field, const uint8_t *payload)
{
    uint32_t bits = report_bits(payload, field->offset, field->size);
    return field->minimum < 0 ? signed_item(bits, field->size) : (int32_t)bits;
}

static int16_t normalize_axis(int32_t value, int32_t minimum, int32_t maximum)
{
    int32_t midpoint = minimum + (maximum - minimum + 1) / 2;
    if (value == maximum) { return INT16_MAX; }
    /* With at most 16 input bits, each half-range is <= 32768 and its product fits int32_t. */
    if (value <= midpoint) {
        return (int16_t)(-((midpoint - value) * 32768 / (midpoint - minimum)));
    }
    return (int16_t)((value - midpoint) * 32767 / (maximum - midpoint));
}

bool usb_host_hid_decode_input(const usb_host_hid_descriptor_t *plan, const uint8_t *report,
                               uint16_t length, uint32_t now_ms, uint32_t sequence,
                               usb_host_hid_joystick_state_t *sample)
{
    usb_host_hid_joystick_state_t result = {0};
    if (!plan || !plan->payload_bits || !report || !sample ||
        length != (plan->payload_bits + 7U) / 8U + plan->has_report_id ||
        (plan->has_report_id && report[0] != plan->report_id)) { return false; }
    const uint8_t *payload = report + plan->has_report_id;
    for (unsigned i = 0; i < plan->axis_count; i++) {
        const usb_host_hid_bitfield_t *f = &plan->axes[i];
        if (f->size) {
            int32_t v = field_value(f, payload);
            if (v < f->minimum || v > f->maximum) { return false; }
            result.axes[i] = normalize_axis(v, f->minimum, f->maximum);
        }
    }
    for (unsigned i = 0; i < plan->button_count; i++) {
        uint16_t bit = plan->button_bits[i];
        if (bit && report_bits(payload, (uint16_t)(bit - 1U), 1U)) {
            result.buttons |= UINT64_C(1) << i;
        }
    }
    if (plan->hat.size) {
        int32_t hat = field_value(&plan->hat, payload);
        if (hat < plan->hat.minimum || hat > plan->hat.maximum) {
            if (!plan->hat.null_state) { return false; }
            result.hats[0] = -1;
        } else {
            result.hats[0] = (int8_t)(hat - plan->hat.minimum);
        }
        result.hat_count = 1;
    }
    result.connected = true;
    result.valid = true;
    result.axis_count = plan->axis_count;
    result.button_count = plan->button_count;
    result.report_id = plan->report_id;
    result.timestamp_ms = now_ms;
    result.sequence = sequence;
    *sample = result;
    return true;
}
