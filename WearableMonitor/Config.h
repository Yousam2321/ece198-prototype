#pragma once
#include <stdint.h>
// Start with motion/timers. Set 1 only after the GY breakout interface checks.
#ifndef ENABLE_MAX30102
#define ENABLE_MAX30102 0
#endif
// B500K is optional; use Serial select commands by default.
#ifndef USE_SELECTOR_POT
#define USE_SELECTOR_POT 0
#endif
// These are prototype timer settings, not generated exercise prescriptions.
const uint32_t REAL_WARMUP_MS = 180000UL; // 3 minutes, adjustable to the assigned plan.
const uint32_t REAL_CARDIO_MS = 900000UL; // 15 minutes, required cardio block.
const uint32_t DEMO_WARMUP_MS = 5000UL;  // Short presentation mode, visibly labelled.
const uint32_t DEMO_CARDIO_MS = 15000UL;
