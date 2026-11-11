#pragma once
#include "ProfileTypes.h"
// HipFlexorStretch: wearable A. Select while IDLE with: select 7
// Time a hold using go and finish. Record each side as a separate session.
// There is no stretch-depth or safety classifier.
// The observer decides when the prescribed hold begins and ends.
inline ExerciseProfile makeHipFlexorStretchProfile() {
  return {"hip_flexor", false};
}
