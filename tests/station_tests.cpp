#include <cassert>
#include <iostream>
#ifndef ENABLE_OLED
#define ENABLE_OLED 0
#endif
#include "stubs/Arduino.h"
#include "stubs/EEPROM.h"
uint32_t fakeNow=0;
uint32_t fakePulse=0;
int fakePins[20]={};
FakeSerial Serial;
unsigned int fakeOledWrites=0;
char fakeScreen[8][17]={};
EEPROMClass EEPROM;
#include "../HoldStation/HoldStation.ino"

// Run the actual firmware loop, including distance measurements and buttons.
void advance(uint32_t milliseconds){
  uint32_t end=fakeNow+milliseconds;
  while(fakeNow<end){++fakeNow;loop();}
}
void pressGo(uint32_t milliseconds){
  fakePins[GO_PIN]=LOW;advance(milliseconds);
  fakePins[GO_PIN]=HIGH;advance(60);
}
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
  for(int i=1;i<=25;++i){SavedSession a={0,1000,500,439,100,1,0,0,false,false};assert(log.append(a));assert(a.id==uint32_t(i));}
  assert(!log.readId(5,r));assert(log.readId(6,r));assert(log.readId(25,r));
  // Interrupted writes never return a partially written record as valid.
  for(int cut=0;cut<22;++cut){
    EEPROMClass interrupted=memory;interrupted.writes=0;interrupted.failAfter=cut;
    SessionArchive<EEPROMClass> trial(interrupted);
    SavedSession a={0,1000,500,439,100,0,0,0,false,false};
    try{trial.append(a);}catch(int){}
    assert(!trial.readId(26,r));assert(trial.readId(25,r));
  }
  memory.bytes[(25-1)%20*21+7]^=1;assert(!log.readId(25,r)); // CRC detects damage.
  // Integration regression: actual pulse processing must count in-band time,
  // and physical-button events must complete and restart two consecutive sessions.
  for(int& pin:fakePins)pin=HIGH;
  goButton=Button();modeButton=Button();
  resetSession();profile=1;calibrated=false;fakePulse=2915; // Approximately 50 cm.
  startCalibration();advance(2200);
  assert(calibrated && distanceValid && inside);
  pressGo(1500);assert(session.state==IDLE); // Long idle GO cannot finish.
  uint32_t firstId=archive.latestId();
  for(int attempt=0;attempt<2;++attempt){
    pressGo(200);assert(session.state==COUNTDOWN);
    assert(session.active==0 && rangeTimer.total==0);
    advance(3100);assert(session.state==RUNNING);
    advance(5000);
    assert(rangeTimer.total>4800 && rangeTimer.total<=session.active);
    pressGo(200);assert(session.state==PAUSED);
    uint32_t held=rangeTimer.total,activeHeld=session.active;
    advance(1000);assert(rangeTimer.total==held && session.active==activeHeld);
    pressGo(200);assert(session.state==RUNNING);advance(1000);
    pressGo(1500);assert(session.state==DONE);
    assert(archive.latestId()==firstId+attempt+1);
    advance(1000);assert(session.state==DONE);
    pressGo(1500);assert(session.state==DONE); // Repeated long GO cannot duplicate save.
  }
#if ENABLE_OLED
  // Completed results are written once even when live range/inside change.
  updateDisplay();
  assert(finalResultShown);
  assert(strstr(fakeScreen[4], "SAVED TO EEPROM"));
  unsigned int resultWrites=fakeOledWrites;
  fakePulse=0;advance(1500);updateDisplay();
  assert(fakeOledWrites==resultWrites);
  assert(strstr(fakeScreen[1], "DONE"));
  pressGo(200);advance(300);
  assert(session.state==COUNTDOWN && !finalResultShown);
  assert(fakeOledWrites>resultWrites);
  // Save failure stays visible as a failure, never a false saved message.
  advance(3100);
  // Exercise the display failure state explicitly.
  session.finish(fakeNow);lastSaveSucceeded=false;finalResultShown=false;
  updateDisplay();assert(strstr(fakeScreen[4], "SAVE FAILED"));
#endif
  // Button-only history erase: explicit menu, release, confirm; MODE cancels.
  // Start from IDLE; completed records remain until the final short GO press.
  resetSession();uint32_t savedBeforeErase=archive.latestId();assert(savedBeforeErase>0);
  auto bothButtons=[](uint32_t duration){
    fakePins[GO_PIN]=LOW;fakePins[MODE_PIN]=LOW;advance(duration);
    fakePins[GO_PIN]=HIGH;fakePins[MODE_PIN]=HIGH;advance(60);
  };
  bothButtons(500);assert(!erasePrompt && session.state==IDLE);
  assert(archive.latestId()==savedBeforeErase);
  fakePins[GO_PIN]=LOW;fakePins[MODE_PIN]=LOW;advance(3200);
  assert(erasePrompt && eraseWaitRelease);
#if ENABLE_OLED
  updateDisplay();assert(strstr(fakeScreen[0],"ERASE HISTORY?"));
#endif
  fakePins[GO_PIN]=HIGH;fakePins[MODE_PIN]=HIGH;advance(60);
  assert(erasePrompt && !eraseWaitRelease && archive.latestId()==savedBeforeErase);
  fakePins[MODE_PIN]=LOW;advance(200);fakePins[MODE_PIN]=HIGH;advance(60);
  assert(!erasePrompt && archive.latestId()==savedBeforeErase && session.state==IDLE);
  bothButtons(3200);assert(erasePrompt && !eraseWaitRelease);
  pressGo(200);assert(!erasePrompt && eraseResult==1 && archive.latestId()==0);
  assert(session.state==IDLE && session.active==0 && rangeTimer.total==0);
#if ENABLE_OLED
  updateDisplay();assert(strstr(fakeScreen[0],"HISTORY CLEARED"));
#endif
  SessionArchive<EEPROMClass> afterErase(EEPROM);assert(afterErase.latestId()==0);
  SavedSession fresh={0,1000,500,500,100,0,0,0,false,false};assert(archive.append(fresh) && fresh.id==1);
  // Both buttons during a running session must not erase, finish or calibrate.
  go();advance(3100);assert(session.state==RUNNING);
  bothButtons(3200);assert(!erasePrompt && session.state==RUNNING && archive.latestId()==1);
  finishSession();assert(session.state==DONE);
  bothButtons(3200);assert(erasePrompt && session.state==DONE);
  pressGo(200);assert(eraseResult==1 && archive.latestId()==0 && session.state==IDLE);
  // Erase touches archive bytes only.
  memory.bytes[990]=0x5A;assert(log.clear());assert(memory.bytes[990]==0x5A);
  for(int i=0;i<740;++i)assert(memory.bytes[i]==0);
  // Full-loop capture: leave the target at the end, then reach GO later.
  resetSession();profile=0;calibrated=true;targetCm=50;toleranceCm=10;fakePulse=2915;
  go();advance(3100);advance(5000);
  uint32_t beforeExit=rangeTimer.total;assert(beforeExit>4800);
  fakePulse=6000;advance(5000);finishSession();
  SavedSession ended;assert(archive.readId(archive.latestId(),ended));
  assert(ended.tailKnown && ended.hasHold && ended.trailingMs>4700);
  assert(ended.inBandMs<=ended.activeMs-ended.trailingMs);
  assert(ended.activeMs-ended.trailingMs<6000);
  // A later return in band means the earlier excursion remains in the denominator.
  resetSession();fakePulse=2915;go();advance(3100);advance(3000);
  fakePulse=6000;advance(2000);fakePulse=2915;advance(3000);
  fakePulse=6000;advance(2000);finishSession();
  assert(archive.readId(archive.latestId(),ended));
  assert(ended.trailingMs>1700 && ended.trailingMs<2200);
  assert(ended.activeMs-ended.trailingMs-ended.inBandMs>1500);
  // B2.4: exclude only the last departure; earlier out-of-band gaps stay assessed.
  HoldTiming ending;
  ending.notePosition(1000,true,true,true);
  ending.notePosition(2000,true,true,false); // Middle departure.
  ending.notePosition(3000,true,true,true);  // Return in band clears the earlier tail.
  ending.notePosition(4000,true,true,false);
  assert(ending.trailingMs(6000)==3000);
  ending.notePosition(6000,false,true,true); // Paused samples cannot move the endpoint.
  assert(ending.trailingMs(6000)==3000);
  ending.clear();assert(!ending.sawInside && ending.trailingMs(5000)==0);

  EEPROMClass migratedMemory;SessionArchive<EEPROMClass> migrated(migratedMemory);
  SavedSession legacy={0,1000,500,500,100,0,0,0,false,false};
  assert(migrated.append(legacy));assert(migrated.readId(1,r) && !r.tailKnown);
  SavedSession modern={0,35000,27000,500,100,1,5000,0,true,true};
  assert(migrated.append(modern));assert(migrated.readId(2,r));
  assert(r.tailKnown && r.trailingMs==5000 && r.historyId==1 && r.hasHold);
  assert(migrated.readId(1,r) && !r.tailKnown); // Older summaries still readable.
  EEPROMClass safe=migratedMemory;SessionArchive<EEPROMClass> safeLog(safe);
  int startWrites=safe.writes;SavedSession newer=modern;assert(safeLog.append(newer));
  int writeCount=safe.writes-startWrites;
  for(int cut=0;cut<writeCount;++cut){
    EEPROMClass powerCut=migratedMemory;powerCut.writes=0;powerCut.failAfter=cut;
    SessionArchive<EEPROMClass> partial(powerCut);SavedSession a=modern;
    try{partial.append(a);}catch(int){}
    assert(!partial.readId(3,r));assert(partial.readId(2,r) && r.tailKnown);
  }
  migratedMemory.bytes[420+16+6]^=1; // Damaged extension retains base, marks metadata unknown.
  assert(migrated.readId(2,r) && !r.tailKnown);
  assert(migrated.clear());assert(migrated.historyId()==2);
  modern.id=0;assert(migrated.append(modern) && modern.id==1 && modern.historyId==2);
  SessionArchive<EEPROMClass> sameAfterPower(migratedMemory);
  assert(sameAfterPower.readId(1,r) && r.tailKnown && r.historyId==2);
  // Chest press: two distinct stable endpoint calibrations, using real pulse
  // conversion/filtering and the existing physical GO/MODE controller.
  auto atDistance=[](float cm){fakePulse=uint32_t(cm*2/.0343f+.5f);advance(650);};
  resetSession();chooseMode(2);assert(profile==2 && !calibrated);
  atDistance(80);startCalibration();advance(2200);
  assert(chestAwaitEnd && !calibrated && fabsf(targetCm-80)<.1f);
  go();assert(session.state==IDLE); // One point is not enough to start.
  atDistance(76);startCalibration();advance(2200);
  assert(chestAwaitEnd && !calibrated); // Reject a noise-sized reference travel.
  atDistance(50);startCalibration();advance(2200);
  assert(calibrated && !chestAwaitEnd && fabsf(chestTravel()-30)<.1f);
  uint32_t holdLatest=archive.latestId();
  pressGo(200);advance(3100);atDistance(50);
  assert(session.state==RUNNING && reps.good==0 && reps.phase==ChestPressCounter::NEED_START);
  atDistance(80);assert(reps.phase==ChestPressCounter::READY);
  atDistance(70);atDistance(80);assert(reps.good==0 && reps.attempts==1);
  atDistance(55);advance(2000);assert(reps.good==0); // Holding the press earns no extra reps.
  atDistance(80);assert(reps.good==1 && reps.attempts==2);
  // Pause mid-press discards just that unfinished cycle; completed reps remain.
  atDistance(50);pressGo(200);assert(session.state==PAUSED && reps.good==1);
  atDistance(80);pressGo(200);advance(600);assert(reps.good==1 && reps.attempts==2);
  // Missing echo cannot supply the return half of a rep.
  atDistance(50);fakePulse=0;advance(300);atDistance(80);
  assert(reps.good==1 && reps.attempts==2);
  atDistance(50);atDistance(80);assert(reps.good==2 && reps.attempts==3);
  // Distance in the return band is enough: 84 cm is within +/-6 cm of 80 cm.
  atDistance(55);atDistance(84);assert(reps.good==3 && reps.attempts==4);
  pressGo(1500);assert(session.state==DONE && lastSaveSucceeded);
  SavedRepSession chestSaved;
  assert(repArchive.latestId()==1 && repArchive.readId(1,chestSaved));
  assert(chestSaved.good==3 && chestSaved.attempts==4 && chestSaved.activeMs>0);
  assert(chestSaved.startMm>=799 && chestSaved.startMm<=801);
  assert(chestSaved.pressedMm>=499 && chestSaved.pressedMm<=501);
  assert(archive.latestId()==holdLatest); // Rep saves do not consume hold slots.
  RepSessionArchive<EEPROMClass> repReboot(EEPROM);
  assert(repReboot.readId(1,chestSaved) && chestSaved.good==3);
  finishSession();assert(repArchive.latestId()==1); // Never save DONE twice.
  Serial.output.clear();showHistory();
  assert(Serial.output.find("# END_HISTORY")!=std::string::npos);
  assert(Serial.output.find("chest_press")==std::string::npos); // Old PC app still reads holds.
  Serial.output.clear();showRepHistory();
  assert(Serial.output.find("REPSAVED,1,chest_press,")!=std::string::npos);
#if ENABLE_OLED
  updateDisplay();assert(strstr(fakeScreen[0],"CHEST B2.6"));
  assert(strstr(fakeScreen[2],"Good 3/4"));assert(strstr(fakeScreen[4],"SAVED TO EEPROM"));
  unsigned int fixedChestWrites=fakeOledWrites;
  atDistance(100);updateDisplay();assert(fakeOledWrites==fixedChestWrites);
#endif
  pressGo(200);assert(session.state==COUNTDOWN && reps.good==0 && reps.attempts==0 && calibrated);
  advance(3100);atDistance(80);atDistance(50);atDistance(80);
  assert(reps.good==1);pressGo(1500);assert(repArchive.latestId()==2);
  // MODE cycles all three modes, clearing each exercise's calibration.
  auto pressMode=[](){fakePins[MODE_PIN]=LOW;advance(200);fakePins[MODE_PIN]=HIGH;advance(60);};
  pressMode();assert(profile==0 && !calibrated);
  pressMode();assert(profile==1 && !calibrated);
  pressMode();assert(profile==2 && !calibrated);
  // Clearing the menu's ALL history removes both independent archives.
  eraseHistory();assert(archive.latestId()==0 && repArchive.latestId()==0);
  assert(repReboot.latestId()==0);

  // The separate chest archive never writes hold records or the shared journal.
  EEPROMClass repMemory;SessionArchive<EEPROMClass> untouchedHolds(repMemory);
  SavedSession untouched={0,10000,9000,500,100,0,1000,0,true,true};
  assert(untouchedHolds.append(untouched));
  RepSessionArchive<EEPROMClass> repLog(repMemory);
  for(int i=1;i<=11;++i){
    SavedRepSession v={0,30000,untouchedHolds.historyId(),5,6,800,500,250};
    assert(repLog.append(v) && v.id==uint32_t(i));
  }
  assert(!repLog.readId(1,chestSaved) && repLog.readId(2,chestSaved) && repLog.readId(11,chestSaved));
  assert(untouchedHolds.readId(1,untouched) && untouched.inBandMs==9000 && untouched.tailKnown);
  for(int cut=0;cut<27;++cut){
    EEPROMClass interrupted=repMemory;interrupted.writes=0;interrupted.failAfter=cut;
    RepSessionArchive<EEPROMClass> partial(interrupted);
    SavedRepSession v={0,30000,1,4,5,800,500,250};
    try{partial.append(v);}catch(int){}
    assert(!partial.readId(12,chestSaved) && partial.readId(11,chestSaved));
  }
  assert(repLog.clear() && repLog.latestId()==0);
  assert(untouchedHolds.readId(1,untouched) && untouched.tailKnown && untouchedHolds.historyId()==1);
  // Cancel confirmation across all modes: no EEPROM changes, no lost counts
  // on declining, no accidental save from long GO in the confirmation menu.
  auto holdMode=[](uint32_t ms){fakePins[MODE_PIN]=LOW;advance(ms);fakePins[MODE_PIN]=HIGH;advance(60);};
  for(uint8_t exercise=0;exercise<3;++exercise){
    resetSession();profile=exercise;calibrated=true;targetCm=80;pressedCm=50;
    fakePulse=uint32_t(80*2/.0343f);go();advance(3100);advance(500);
    pressGo(200);assert(session.state==PAUSED);
    uint32_t heldActive=session.active,heldBand=rangeTimer.total;
    reps.good=2;reps.attempts=3;
    uint8_t savedBytes[1024];memcpy(savedBytes,EEPROM.bytes,1024);
    // Both-buttons gesture must not become cancel when released separately.
    fakePins[GO_PIN]=LOW;fakePins[MODE_PIN]=LOW;advance(2200);
    fakePins[GO_PIN]=HIGH;advance(60);fakePins[MODE_PIN]=HIGH;advance(60);
    assert(!cancelPrompt && session.state==PAUSED);
    holdMode(1500);assert(!cancelPrompt && session.state==PAUSED);
    holdMode(2200);assert(cancelPrompt && session.state==PAUSED);
#if ENABLE_OLED
    updateDisplay();assert(strstr(fakeScreen[0],"CANCEL SESSION?"));
#endif
    pressGo(1500);assert(cancelPrompt && session.state==PAUSED);
    finishSession();go();assert(cancelPrompt && session.state==PAUSED);
    holdMode(200);assert(!cancelPrompt && session.state==PAUSED);
    assert(session.active==heldActive && rangeTimer.total==heldBand && reps.good==2);
    pressGo(200);assert(session.state==RUNNING);pressGo(200);assert(session.state==PAUSED);
    holdMode(2200);assert(cancelPrompt);
    pressGo(200);assert(!cancelPrompt && discardResult && session.state==IDLE);
    assert(session.active==0 && rangeTimer.total==0 && reps.good==0 && reps.attempts==0);
    assert(calibrated && targetCm==80 && pressedCm==50);
    assert(memcmp(savedBytes,EEPROM.bytes,1024)==0);
#if ENABLE_OLED
    updateDisplay();assert(strstr(fakeScreen[0],"CANCELLED"));
    assert(strstr(fakeScreen[1],"NOT SAVED"));
#endif
    pressGo(200);assert(session.state==COUNTDOWN && !discardResult);
    advance(3100);pressGo(1500);assert(session.state==DONE && lastSaveSucceeded);
  }
  std::cout<<"Station restart, button, timing and EEPROM tests passed\n";
}
