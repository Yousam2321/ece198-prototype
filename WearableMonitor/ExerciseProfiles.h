#pragma once
// Profile order is the Serial select ID. Keep IDs stable for log compatibility.
#include "exercises/Treadmill.h"
#include "exercises/ThoracicRotations.h"
#include "exercises/Airplane.h"
#include "exercises/ChestPress.h"
#include "exercises/GluteBridges.h"
#include "exercises/LatPulldown.h"
#include "exercises/FarmersCarry.h"
#include "exercises/HipFlexorStretch.h"
#include "exercises/CatCow.h"
const uint8_t PROFILE_COUNT = 9;
const ExerciseProfile EXERCISES[PROFILE_COUNT] = {
  makeTreadmillProfile(),
  makeThoracicRotationsProfile(),
  makeAirplaneProfile(),
  makeChestPressProfile(),
  makeGluteBridgesProfile(),
  makeLatPulldownProfile(),
  makeFarmersCarryProfile(),
  makeHipFlexorStretchProfile(),
  makeCatCowProfile()
};
