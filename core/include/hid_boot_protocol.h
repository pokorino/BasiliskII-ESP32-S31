#pragma once
#include <stdint.h>

// USB HID boot keyboard report (HID 1.11, appendix B). The host uses
// ESP-IDF directly and does not require TinyUSB's device stack headers.
constexpr uint8_t HID_SUBCLASS_BOOT = 1;
constexpr uint8_t HID_ITF_PROTOCOL_KEYBOARD = 1;
struct hid_keyboard_report_t {
    uint8_t modifier;
    uint8_t reserved;
    uint8_t keycode[6];
};
static_assert(sizeof(hid_keyboard_report_t) == 8, "HID boot report must be eight bytes");
