#pragma once
#include <stdint.h>

// Only credit intervals bounded by consecutive valid, in-band measurements.
// Delayed loops may still count up to 150 ms; never credit an entire long stall.
// Invalid echoes, out-of-band samples, and pauses break the chain immediately.
struct HoldTiming {
  uint32_t total=0,last=0,lastInsideActive=0;
  bool sawInside=false;
  uint16_t gapMs=0;
  bool previousInside=false;
  void breakContinuity(){previousInside=false;}
  void clear(){total=0;last=0;gapMs=0;lastInsideActive=0;sawInside=false;breakContinuity();}
  void notePosition(uint32_t active,bool running,bool valid,bool inside){
    if(running && valid && inside){lastInsideActive=active;sawInside=true;}
  }
  uint32_t trailingMs(uint32_t active)const{
    return sawInside && active>=lastInsideActive?active-lastInsideActive:0;
  }
  void update(uint32_t now,bool running,bool valid,bool inside){
    uint32_t dt=uint32_t(now-last);last=now;
    gapMs=dt>65535?65535:uint16_t(dt);
    if(!running || !valid || !inside){breakContinuity();return;}
    if(previousInside)total+=dt>150?150:dt;
    previousInside=true;
  }
};
