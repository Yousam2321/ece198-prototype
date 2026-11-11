#pragma once
#include "ProfileTypes.h"
// LatPulldown: wearable A. Select while IDLE with: select 5
// Try a forearm placement with repeatable changing tilt across a full pull.
// teach at a comfortable speed; verify the proposed count against manual reps.
// The sensor does not measure cable force, selected weight or full technique.
// Use manual mode when the signal cannot distinguish complete repetitions.
inline ExerciseProfile makeLatPulldownProfile() {
  return {"lat_pulldown", true};
}
