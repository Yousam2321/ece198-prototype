#pragma once
#include "ProfileTypes.h"
// FarmersCarry: wearable A. Select while IDLE with: select 6
// Record carry duration. Enter distance 10 to record 10 metres manually.
// That command records an observer entry; it does not estimate distance.
// Neither carried weight nor grip force is measured.
// Do not integrate accelerometer readings into an unvalidated distance estimate.
inline ExerciseProfile makeFarmersCarryProfile() {
  return {"farmers_carry", false};
}
