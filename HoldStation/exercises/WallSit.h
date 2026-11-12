#pragma once
#include "ProfileTypes.h"
// WallSit: station B. Select while IDLE with: wall
// Fix the HC-SR04 on a stand aimed at a broad torso/hip target.
// Calibrate in the chosen reference position before starting. Sensor movement,
// different body placement or another exercise requires another calibration.
// In-band time means near one reference distance, not verified clinical posture.
// +/-10 cm is an adjustable engineering tolerance, not a medical prescription.
inline HoldProfile makeWallSitProfile() { return {"wall_sit",10.0f}; }
