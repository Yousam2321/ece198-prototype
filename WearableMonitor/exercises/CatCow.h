#pragma once
#include "ProfileTypes.h"
// CatCow: wearable A. Select while IDLE with: select 8
// Try a secure upper-back placement; re-teach for that placement.
// If tilt produces a repeatable cycle, propose a full cat-to-cow-to-cat count.
// An accelerometer measures local motion, not spinal curvature.
// Small changes may be too weak; use manual confirmation when necessary.
inline ExerciseProfile makeCatCowProfile() {
  return {"cat_cow", true};
}
