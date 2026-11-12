#include <cassert>
#include <iostream>
#define ENABLE_OLED 0
#include "stubs/Arduino.h"
#include "stubs/EEPROM.h"
uint32_t fakeNow=0;
int fakePins[20]={};
FakeSerial Serial;
EEPROMClass EEPROM;
#include "../HoldStation/HoldStation.ino"

int main(){
  // Actual station controller: a long press while idle cannot finish/save.
  resetSession();finishSession();
  assert(session.state==IDLE && archive.latestId()==0);
  calibrated=true;targetCm=43.9f;toleranceCm=10;
  go();assert(session.state==COUNTDOWN);
  finishSession();assert(session.state==COUNTDOWN && archive.latestId()==0);
  fakeNow=3000;session.tick(fakeNow);assert(session.state==RUNNING);
  rangeTimer.update(3000,true,true,true);
  rangeTimer.update(3075,true,true,true);
  rangeTimer.update(3275,true,true,true); // A 200 ms display/scheduling delay.
  assert(rangeTimer.total==225);
  fakeNow=4000;finishSession();
  assert(session.state==DONE && session.active==1000 && archive.latestId()==1);
  SavedSession r;assert(archive.readId(1,r));
  assert(r.activeMs==1000 && r.inBandMs==225 && r.toleranceMm==100);
  finishSession();assert(archive.latestId()==1); // No duplicate on repeated long GO.
  go();assert(session.state==COUNTDOWN && session.active==0 && rangeTimer.total==0);
  fakeNow=7000;session.tick(fakeNow);fakeNow=8000;
  go();assert(session.state==PAUSED);go();assert(session.state==RUNNING);
  fakeNow=9000;finishSession();assert(archive.latestId()==2);
  SessionArchive<EEPROMClass> reboot(EEPROM);
  assert(reboot.readId(1,r) && r.activeMs==1000); // EEPROM survives new controller.
  assert(reboot.readId(2,r) && r.activeMs==2000);

  // In-band timing: invalid/outside/paused samples and restart break continuity.
  HoldTiming timer;
  timer.update(0,true,true,true);timer.update(75,true,true,true);
  assert(timer.total==75);
  timer.update(150,true,true,false);timer.update(225,true,true,true);
  assert(timer.total==75);timer.update(300,true,true,true);assert(timer.total==150);
  timer.update(375,true,false,true);timer.update(450,true,true,true);
  assert(timer.total==150);timer.update(525,false,true,true);
  timer.update(600,true,true,true);assert(timer.total==150);
  timer.update(1600,true,true,true);assert(timer.total==300); // Stall capped.
  timer.clear();assert(timer.total==0 && !timer.previousInside);
  timer.update(UINT32_MAX-50,true,true,true);timer.update(24,true,true,true);
  assert(timer.total==75); // millis wrap.

  // Ring capacity and chronological IDs; old entries overwritten, newest retained.
  EEPROMClass memory;SessionArchive<EEPROMClass> log(memory);
  assert(log.latestId()==0);
  for(int i=1;i<=25;++i){SavedSession a={0,1000,500,439,100,1};assert(log.append(a));assert(a.id==uint32_t(i));}
  assert(!log.readId(5,r));assert(log.readId(6,r));assert(log.readId(25,r));
  // Interrupted writes never return a partially written record as valid.
  for(int cut=0;cut<22;++cut){
    EEPROMClass interrupted=memory;interrupted.writes=0;interrupted.failAfter=cut;
    SessionArchive<EEPROMClass> trial(interrupted);
    SavedSession a={0,1000,500,439,100,0};
    try{trial.append(a);}catch(int){}
    assert(!trial.readId(26,r));assert(trial.readId(25,r));
  }
  memory.bytes[(25-1)%20*21+7]^=1;assert(!log.readId(25,r)); // CRC detects damage.
  std::cout<<"Station restart, button, timing and EEPROM tests passed\n";
}
