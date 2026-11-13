#include <cassert>
#include <iostream>
#include "parta_stubs/Arduino.h"
#include "parta_stubs/EEPROM.h"
#include "parta_stubs/Wire.h"
uint32_t fakeNow=0,fakePulse=0;
int fakePins[20]={},fakeAnalog[3]={512,512,512};
FakeSerial Serial;EEPROMClass EEPROM;PartAWire Wire;
bool fakeSensorPresent=true,fakeBeat=false;
uint8_t fakeWireError=0;
long fakeIr=0;
#if defined(ARDUINO_ARCH_RENESAS_UNO)
bool fakeUsbInterruptsEnabled=true,fakeUsbCloseOnSubmit=false,fakeUsbFullOnSubmit=false;
#endif
#include "../PartA/PartA.ino"
void advanceA(uint32_t ms){for(uint32_t i=0;i<ms;++i){++fakeNow;loop();}}
void press(uint8_t pin,uint32_t ms=150){
  fakePins[pin]=LOW;advanceA(ms);fakePins[pin]=HIGH;advanceA(70);
}
void go(){press(START_BUTTON);}
void next(){press(EXERCISE_BUTTON);}
void finish(){press(START_BUTTON,3200);}
void beginAttempt(){
  go();assert(session.state==COUNTDOWN && hasAttempt);
  advanceA(3100);assert(session.state==RUNNING && session.active<400);
}
void squat(){fakeAnalog[Z_PIN]=412;advanceA(250);fakeAnalog[Z_PIN]=512;advanceA(350);}
void raise(){fakeAnalog[Y_PIN]=612;advanceA(250);fakeAnalog[Y_PIN]=512;advanceA(350);}
void openErase(){
  fakePins[START_BUTTON]=fakePins[EXERCISE_BUTTON]=LOW;advanceA(3200);
  assert(menu==ERASE_CONFIRM && session.state==PAUSED);
  fakePins[START_BUTTON]=fakePins[EXERCISE_BUTTON]=HIGH;advanceA(70);
  assert(menuArmed);
}
void commandA(const char* line){
  Serial.input=line;advanceA(250);
}
void testArchive(){
  EEPROMClass memory;PartAArchive<EEPROMClass> log(memory);PartASavedSession r;
  for(int i=1;i<=25;++i){
    PartASavedSession record={0,10000,0,5,0,0,uint8_t(i%2)};assert(log.append(record));
  }
  assert(!log.readId(5,r) && log.readId(6,r) && log.readId(25,r));
  PartAArchive<EEPROMClass> reboot(memory);
  assert(reboot.readId(25,r) && r.reps==5); // Existing A1.x byte format.
  for(int cut=0;cut<24;++cut){
    EEPROMClass broken=memory;broken.writes=0;broken.failAfter=cut;PartAArchive<EEPROMClass> partial(broken);
    PartASavedSession record={0,10000,0,3,750,3,0};try{partial.append(record);}catch(int){}
    assert(!partial.readId(26,r) && partial.readId(25,r));
  }
  memory.bytes[((25-1)%20)*23+11]^=1;assert(!log.readId(25,r));
  for(int cut=0;cut<477;++cut){
    EEPROMClass interrupted=memory;interrupted.writes=0;interrupted.failAfter=cut;
    PartAArchive<EEPROMClass> erasing(interrupted);try{erasing.clear();}catch(int){}
    if(erasing.historyId()==2)assert(!erasing.readId(24,r) && erasing.latestId()==0);
    else assert(erasing.readId(24,r));
  }
  assert(log.clear());PartASavedSession fresh={0,5000,0,2,800,2,1};
  assert(log.append(fresh) && fresh.id==1 && fresh.historyId==2);
}
int main(int argc,char** argv){
  for(int& value:fakePins)value=HIGH;
#if defined(ARDUINO_ARCH_RENESAS_UNO)
  Serial.rejectCoreWrite=Serial.rejectCorePoll=true;
#endif
  Serial.connected=false;
  setup();assert(calibrating && sessionArchive.latestId()==0);
  assert(Wire.timeout==5000);
  advanceA(1100);assert(!calibrating && session.state==IDLE && Serial.output.empty());
  // Fully standalone use: no monitor, no commands, no serial output.
  beginAttempt();squat();assert(repCount==1);
  advanceA(1300);squat();assert(repCount==2);
  go();assert(session.state==PAUSED);
  uint32_t paused=session.active;advanceA(5000);assert(session.active==paused);
  go();assert(session.state==RUNNING && repCount==2);
  // The saved mean includes accepted active beats only.
  uint32_t beatTime=fakeNow;acceptPulseSample(60000,beatTime);fakeBeat=true;
  acceptPulseSample(60000,beatTime+1000); // First detected beat.
  fakeBeat=true;acceptPulseSample(60000,beatTime+2000);
  assert(bpmSamples==1 && bpmSumTenths==600);
  finish();assert(session.state==DONE && saved && repCount==2);
  uint32_t firstDone=session.active;
  PartASavedSession result;
  assert(sessionArchive.readId(1,result) && result.reps==2 && result.bpmTenths==600);
  assert(result.activeMs==firstDone && result.bpmSamples==1);
  advanceA(5000);assert(session.state==DONE && session.active==firstDone && repCount==2);
  auto screen0=lcd.rows[0],screen1=lcd.rows[1];
  advanceA(2000);assert(lcd.rows[0]==screen0 && lcd.rows[1]==screen1);
  finish();assert(sessionArchive.latestId()==1 && session.state==DONE); // No duplicate.
  // One tap from DONE starts a completely fresh attempt, never IDLE/DONE loops.
  beginAttempt();assert(repCount==0 && bpmSamples==0 && bpmSumTenths==0);
  squat();assert(repCount==1);finish();assert(sessionArchive.latestId()==2);
  next();assert(currentExercise==1 && session.state==IDLE && repCount==0);
  beginAttempt();
  // Threshold 25 rejects the team's formerly accepted shallow 20-unit motion.
  fakeAnalog[Y_PIN]=540;advanceA(250);fakeAnalog[Y_PIN]=512;advanceA(350);
  assert(repCount==0 && !latRaiseDetected);
  fakeAnalog[Y_PIN]=612;advanceA(50);fakeAnalog[Y_PIN]=512;advanceA(350);
  assert(repCount==0);
  raise();assert(repCount==1);
  // Switching while active/paused cannot discard counts silently.
  next();assert(currentExercise==1 && session.state==RUNNING && repCount==1);
  go();next();assert(currentExercise==1 && session.state==PAUSED && repCount==1);
  press(EXERCISE_BUTTON,2200);assert(menu==CANCEL_CONFIRM);
  advanceA(70);assert(menuArmed);
  next();advanceA(2200);assert(menu==NORMAL && session.state==PAUSED && repCount==1);
  press(EXERCISE_BUTTON,2200);advanceA(70);go();advanceA(2200);
  assert(menu==NORMAL && session.state==IDLE && repCount==0 && sessionArchive.latestId()==2);
  // Save failure keeps the result paused; retry saves once and returns DONE.
  beginAttempt();raise();assert(repCount==1);
  EEPROM.ignoreWrites=true;finish();
  assert(session.state==PAUSED && saveFailed && repCount==1);
  EEPROM.ignoreWrites=false;finish();assert(session.state==DONE && saved && sessionArchive.latestId()==3);
  // Erase menu from DONE: opening hold cannot confirm; cancel preserves bytes.
  EEPROMClass before=EEPROM;openErase();advanceA(1500);
  assert(menu==ERASE_CONFIRM && memcmp(before.bytes,EEPROM.bytes,1024)==0);
  next();advanceA(2200);assert(menu==NORMAL && session.state==DONE);
  assert(memcmp(before.bytes,EEPROM.bytes,1024)==0 && repCount==1);
  // Running erase: pause/exclude all menu time, keep the pending attempt.
  beginAttempt();raise();uint16_t count=repCount;
  openErase();uint32_t insideMenu=session.active;advanceA(1200);assert(session.active==insideMenu);
  next();advanceA(2200);
  assert(session.state==RUNNING && repCount==count && menu==NORMAL);
  openErase();insideMenu=session.active;go();
  assert(menu==NOTICE && sessionArchive.latestId()==0 && repCount==count);
  advanceA(2200);assert(session.state==RUNNING && session.active-insideMenu<400);
  finish();assert(sessionArchive.readId(1,result) && result.reps==count && result.historyId==2);
  // Paused erase, cancelled and confirmed, both retain current statistics.
  beginAttempt();go();paused=session.active;count=repCount;
  openErase();next();advanceA(2200);
  assert(session.state==PAUSED && session.active==paused && repCount==count);
  openErase();go();advanceA(2200);
  assert(session.state==PAUSED && session.active==paused && repCount==count);
  finish();assert(session.state==DONE && sessionArchive.latestId()==1);
  // Serial output is demand-driven. A stall/disconnect never blocks the loop.
  Serial.connected=true;Serial.space=0;
  commandA("history\n");assert(txLength>0);
  beginAttempt();go();assert(session.state==PAUSED);go();finish();
  assert(session.state==DONE && sessionArchive.latestId()==2);
  Serial.space=64;Serial.output.clear();commandA("version\n");
  assert(Serial.output=="# DEVICE,part_a,A2.0\n");
  Serial.output.clear();commandA("history\n");
  assert(Serial.output.find("# ASAVED,")==0 && Serial.output.find("# END_A_HISTORY\n")!=std::string::npos);
  assert(Serial.output.find("ASAVED,2,lateral_raises,")!=std::string::npos);
  for(int cycle=0;cycle<5;++cycle){
    Serial.connected=false;Serial.input="\r\n";Serial.output.clear();
    beginAttempt();raise();finish();assert(session.state==DONE && saved);
    assert(Serial.output.empty()); // No event/telemetry printing on button actions.
    Serial.connected=true;Serial.space=64;Serial.output.clear();commandA("history\n");
    assert(Serial.output.find("# END_A_HISTORY\n")!=std::string::npos);
  }
  std::string exportFixture=Serial.output;
  Serial.output.clear();beginAttempt();commandA("history\n");
  assert(Serial.output.find("pause_before_history")!=std::string::npos);
  go();finish();
  // Serial erase also retains the current DONE result and returns to normal UI.
  commandA("erase CONFIRM\n");assert(menu==NOTICE && sessionArchive.latestId()==0);
  advanceA(2200);assert(menu==NORMAL && session.state==DONE);
  // Pulse fault is isolated: UI/session work even with a missing sensor.
  fakeWireError=2;advanceA(1200);assert(!pulseSensorReady);
  beginAttempt();finish();assert(session.state==DONE && saved);
#if defined(ARDUINO_ARCH_RENESAS_UNO)
  fakeUsbCloseOnSubmit=true;Serial.connected=true;
  assert(serialLink.writeSome("x",1)==0 && fakeUsbInterruptsEnabled);
  Serial.connected=true;fakeUsbFullOnSubmit=true;
  assert(serialLink.writeSome("x",1)==0 && fakeUsbInterruptsEnabled);
  Serial.space=64;assert(serialLink.writeSome("x",1)==1 && fakeUsbInterruptsEnabled);
#endif
  // Countdown menu must preserve remaining time; a tap cancels without saving.
  resetAttempt(fakeNow);go();advanceA(1000);uint32_t remainingAt=fakeNow-session.countdownAt;
  openErase();next();advanceA(2050);
  assert(session.state==COUNTDOWN && fakeNow-session.countdownAt<remainingAt+700);
  go();assert(session.state==IDLE);
  before=EEPROM;openErase();next();advanceA(2200);
  assert(session.state==IDLE && menu==NORMAL && memcmp(before.bytes,EEPROM.bytes,1024)==0);
  // A partial movement must not complete across a pause/resume transition.
  beginAttempt();fakeAnalog[Y_PIN]=612;advanceA(250);assert(latRaiseDetected);
  go();fakeAnalog[Y_PIN]=512;advanceA(350);go();advanceA(350);
  assert(repCount==0 && !latRaiseDetected);finish();
  // Unsigned clock arithmetic across wrap, independently of the fake boot.
  parta::Session wrapped;wrapped.reset(UINT32_MAX-200);wrapped.state=RUNNING;
  wrapped.tick(100);assert(wrapped.active==301);
  wrapped.state=PAUSED;wrapped.tick(500);assert(wrapped.active==301);
  wrapped.state=RUNNING;wrapped.tick(600);assert(wrapped.active==401);
  testArchive();
  std::cout<<"PASS: A2 state/buttons, repeated saves/restarts, detector criteria, pulse polling, "
             "all-state erase/cancel, timing, stalled/closed COM exports, legacy persistence and power-loss checks\n";
  if(argc>1 && !strcmp(argv[1],"--emit-history"))std::cout<<exportFixture;
}
