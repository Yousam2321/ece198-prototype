#pragma once
#include <stdint.h>
// A profile describes WHAT is selected; the main sketch handles sensors/timing.
struct ExerciseProfile { const char* name; bool repCandidate; };
struct HoldProfile { const char* name; float toleranceCm; };
