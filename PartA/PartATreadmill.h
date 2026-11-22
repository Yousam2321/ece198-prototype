#pragma once
#include <stdint.h>

struct Treadmill {
  enum Phase { READY, BASELINE, WORKOUT, FINAL_CHECK, ALERT, RETAKE, RESUME, DONE, STOPPED };
  // Prototype workout pacing rules. These percentages are not health limits.
  static constexpr unsigned paceMultiplier=2;
  static constexpr unsigned retakePercent=80;
  static constexpr unsigned demoRestingBpm=70;
  Phase phase=READY, resumePhase=READY;
  uint32_t elapsed=0, last=0, phaseAt=0, firstBeat=0, lastBeat=0, sum=0;
  unsigned beats=0, bpm=0, baseline=0, high=0, alertLimit=0;
  bool checked=false, missed=false, demo=false;
  static bool walking(uint32_t t){return (t>=180000 && t<240000)||(t>=420000 && t<480000);}
  void resetWindow(){beats=0;sum=0;}
  void enter(Phase next,uint32_t now){phase=next;phaseAt=last=now;resetWindow();}
  void start(uint32_t now,bool simulate=false){
    *this=Treadmill();
    demo=simulate;
    enter(BASELINE,now);
  }
  bool measuring() const {
    return phase==BASELINE || phase==FINAL_CHECK || phase==RETAKE ||
      (phase==WORKOUT && walking(elapsed) && !checked);
  }
  void beat(unsigned estimate,uint32_t now){
    if(!measuring() || !estimate)return;
    if(beats && uint32_t(now-lastBeat)>2500)resetWindow();
    if(!beats)firstBeat=now;
    lastBeat=now;sum+=estimate;++beats;
    if(uint32_t(now-firstBeat)<15000)return;
    bpm=(sum+beats/2)/beats;resetWindow();missed=false;
    if(phase==BASELINE){
      baseline=bpm;
      alertLimit=baseline*paceMultiplier;
      enter(WORKOUT,now);
      return;
    }
    if(phase==RETAKE){
      if(bpm<=alertLimit && uint32_t(bpm)*100<=uint32_t(high)*retakePercent)enter(RESUME,now);
      else enter(ALERT,now);
      return;
    }
    checked=true;
    if(alertLimit && bpm>alertLimit){
      high=bpm;resumePhase=phase;enter(ALERT,now);
    }else if(phase==FINAL_CHECK)enter(DONE,now);
  }
  void tick(uint32_t now){
    uint32_t delta=now-last;last=now;
    if(phase==WORKOUT){
      // Cap elapsed at the next phase boundary if the main loop was delayed.
      uint32_t end=boundary();
      if(delta<end-elapsed){elapsed+=delta;return;}
      bool leavingWalk=walking(elapsed);
      elapsed=end;
      if(leavingWalk && !checked){missed=true;bpm=0;}
      if(!leavingWalk)missed=false;
      checked=false;resetWindow();
      if(elapsed==660000){bpm=0;enter(FINAL_CHECK,now);}
      phaseAt=now;
    }else if((phase==BASELINE || phase==FINAL_CHECK || phase==RETAKE)
      && uint32_t(now-phaseAt)>=60000){
      missed=true;bpm=0;
      if(phase==BASELINE)enter(WORKOUT,now);
      else if(phase==FINAL_CHECK)enter(DONE,now);
      else enter(ALERT,now);
    }
  }
  uint32_t boundary() const {
    if(elapsed<180000)return 180000;
    if(elapsed<240000)return 240000;
    if(elapsed<420000)return 420000;
    if(elapsed<480000)return 480000;
    return 660000;
  }
  void simulatedPulse(uint32_t now){
    if(!demo || !measuring() || (beats && uint32_t(now-lastBeat)<1000))return;
    if(phase==BASELINE)beat(demoRestingBpm,now);
    else if(phase==RETAKE)beat(120,now);
    else if(phase==FINAL_CHECK)beat(115,now);
    else beat(elapsed<240000?160:125,now);
  }
  void go(uint32_t now,bool simulate=false){
    if(phase==READY || phase==DONE || phase==STOPPED)start(now,simulate);
    else if(phase==ALERT)enter(RETAKE,now);
    else if(phase==RESUME)enter(resumePhase==FINAL_CHECK?DONE:WORKOUT,now);
  }
  bool idle() const{return phase==READY || phase==DONE || phase==STOPPED;}
};
