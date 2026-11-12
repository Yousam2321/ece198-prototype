#pragma once
#include <stdint.h>
#include <math.h>

// These are distance-based prototype criteria, not a full-body form grade.
// Progress is signed and normalized: 0=start, 1=calibrated pressed position.
// This works with the sensor in front of OR behind the moving hand.
struct ChestPressCounter {
  enum Phase : uint8_t { NEED_START, READY, MOVING };
  Phase phase=NEED_START;
  uint16_t good=0, attempts=0;
  float peak=0, lastPeak=0;
  uint8_t startSamples=0, pressSamples=0;
  bool reachedPress=false;

  void breakContinuity(){
    phase=NEED_START;peak=0;startSamples=0;pressSamples=0;reachedPress=false;
  }
  void clear(){good=0;attempts=0;lastPeak=0;breakContinuity();}
  // Return true on a completed attempt (good or short); never count twice while
  // holding a position. Two successive samples confirm each endpoint.
  bool update(float progress,bool running,bool valid){
    if(!running || !valid || !isfinite(progress)){
      breakContinuity();return false;
    }
    // Returning means within 20% of travel on either side of the start.
    bool atStart=fabsf(progress)<=0.20f;
    if(progress < -0.20f){breakContinuity();return false;}
    if(phase==NEED_START){
      startSamples=atStart?uint8_t(startSamples+1):0;
      if(startSamples>=2){phase=READY;startSamples=0;}
      return false;
    }
    if(phase==READY){
      // Ignore tiny adjustments. A meaningful attempt starts at 30% travel.
      if(progress>=0.30f){phase=MOVING;peak=progress;pressSamples=progress>=0.80f?1:0;}
      return false;
    }
    if(progress>peak)peak=progress;
    if(progress>=0.80f){
      if(pressSamples<2)++pressSamples;
      if(pressSamples>=2)reachedPress=true;
    }else pressSamples=0;
    startSamples=atStart?uint8_t(startSamples+1):0;
    if(startSamples<2)return false;
    lastPeak=peak;
    if(attempts<UINT16_MAX){++attempts;if(reachedPress)++good;}
    phase=READY;peak=0;startSamples=0;pressSamples=0;reachedPress=false;
    return true;
  }
};
