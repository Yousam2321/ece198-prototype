#pragma once
#include "ProfileTypes.h"
// Treadmill: wearable A. Select while IDLE with: select 0
// Warm-up is 180 seconds and cardio 900 seconds by default.
// The demo command switches to 5+15 seconds and labels logs DEMO.
// The main loop times these phases, excludes pauses and ends the session.
// MAX30102 HR is optional and must be enabled in Config.h after wiring checks.
// This profile does not read treadmill speed, distance or belt state.
inline ExerciseProfile makeTreadmillProfile() {
  return {"treadmill", false};
}
