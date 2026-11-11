#pragma once
#include <stdint.h>
// Use 0 for a Serial-only test when OLED supply/interface is unconfirmed.
#ifndef ENABLE_OLED
#define ENABLE_OLED 1
#endif
// Photos do not establish the controller. Try 0 (SSD1306) if SH1106 fails.
#ifndef OLED_SH1106
#define OLED_SH1106 1
#endif
const uint8_t OLED_ADDRESS=0x3C; // Seven-bit address. Printed 0x78 means 0x3C.
