#pragma once
#include "ProfileTypes.h"
// Plank: station B. Select while IDLE with: plank
// Fix the HC-SR04 below/beside a broad torso target, clear of the body.
// Calibrate in the chosen reference position before starting. Sensor movement,
// different body placement or another exercise requires another calibration.
// In-band time means near one reference distance, not verified clinical posture.
// +/-10 cm is an adjustable engineering tolerance, not a medical prescription.
inline HoldProfile makePlankProfile() { return {"plank",10.0f}; }
