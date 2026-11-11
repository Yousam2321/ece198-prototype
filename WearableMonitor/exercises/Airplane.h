#pragma once
#include "ProfileTypes.h"
// Airplane: wearable A. Select while IDLE with: select 2
// Start the hold with go; finish when the observer ends the hold.
// The timer records active time; it does not establish balance or body alignment.
// Each side can be recorded as a separate session.
inline ExerciseProfile makeAirplaneProfile() {
  return {"airplane", false};
}
