#pragma once
#include "ProfileTypes.h"
// GluteBridges: wearable A. Select while IDLE with: select 4
// Try a secure pelvis/torso placement that moves repeatably with the bridge.
// Re-teach for this placement; never reuse an arm calibration.
// Count a full taught signal cycle; compare with observed bridges.
// Sensor angle/movement does not prove hip extension or proper spinal posture.
inline ExerciseProfile makeGluteBridgesProfile() {
  return {"glute_bridge", true};
}
