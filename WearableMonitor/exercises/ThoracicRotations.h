#pragma once
#include "ProfileTypes.h"
// ThoracicRotations: wearable A. Select while IDLE with: select 1
// Record elapsed time and enter one rep command per observed rotation.
// An accelerometer cannot measure a slow pure rotation about gravity/yaw.
// Do not interpret ordinary acceleration changes as a rotation angle.
// No automatic form or range-of-motion grading is implemented.
inline ExerciseProfile makeThoracicRotationsProfile() {
  return {"thoracic_rotation", false};
}
