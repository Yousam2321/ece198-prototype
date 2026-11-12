#include <cassert>
#include <iostream>
#include "parta_stubs/Arduino.h"
#include "parta_stubs/EEPROM.h"
#include "parta_stubs/Wire.h"
uint32_t fakeNow=0,fakePulse=0;
int fakePins[20]={},fakeAnalog[3]={512,512,512};
FakeSerial Serial;EEPROMClass EEPROM;PartAWire Wire;
bool fakeSensorPresent=false,fakeBeat=false;
long fakeIr=0;
#include "../PartA/PartA.ino"
void advanceA(uint32_t ms){for(uint32_t i=0;i<ms;++i){++fakeNow;loop();}}
void startPress(uint32_t ms){fakePins[START_BUTTON]=LOW;advanceA(ms);fakePins[START_BUTTON]=HIGH;advanceA(70);}
void exercisePress(){fakePins[EXERCISE_BUTTON]=LOW;advanceA(200);fakePins[EXERCISE_BUTTON]=HIGH;advanceA(70);}
void squat(){fakeAnalog[Z_PIN]=412;advanceA(250);fakeAnalog[Z_PIN]=512;advanceA(350);}
void latRaise(){fakeAnalog[Y_PIN]=612;advanceA(250);fakeAnalog[Y_PIN]=512;advanceA(350);}
int main(){
  for(int& v:fakePins)v=HIGH;
  setup();assert(sessionArchive.latestId()==0 && !exerciseRunning);
  startPress(200);assert(exerciseRunning && sessionTouched[0]);
  squat();assert(exerciseReps[0]==1);advanceA(1300);squat();assert(exerciseReps[0]==2);
  startPress(200);assert(!exerciseRunning);uint32_t stopped=activeMs[0];advanceA(1500);assert(activeMs[0]==stopped);
  startPress(200);assert(exerciseRunning && exerciseReps[0]==2);
  pulseSensorReady=true;fakeIr=60000;fakeBeat=true;updatePulseSensor();
  fakeNow+=1000;updateSessionClock();fakeBeat=true;updatePulseSensor();
  assert(sessionBpmSamples[0]==1);fakeIr=0;updatePulseSensor();assert(averageBPM==0);
  startPress(1200);assert(sessionEnded && !exerciseRunning && exerciseReps[0]==0);
  PartASavedSession saved;assert(sessionArchive.readId(1,saved));
  assert(saved.reps==2 && saved.exercise==0 && saved.activeMs>stopped && saved.bpmSamples==1 && saved.bpmTenths==600);
  endSession();assert(sessionArchive.latestId()==1); // Ended/unused modes do not save twice.
  PartAArchive<EEPROMClass> reboot(EEPROM);assert(reboot.readId(1,saved) && saved.reps==2);
  Serial.output.clear();handleHistoryCommand("history");
  assert(Serial.output.find("ASAVED,1,squats,")!=std::string::npos && Serial.output.find("# END_A_HISTORY")!=std::string::npos);
  startPress(200);assert(exerciseRunning);squat();assert(exerciseReps[0]==1);
  exercisePress();assert(currentExercise==0); // Original running-switch restriction stays.
  startPress(200);assert(!exerciseRunning);exercisePress();
  assert(currentExercise==1 && exerciseReps[0]==0 && sessionArchive.latestId()==2);
  assert(reboot.readId(2,saved) && saved.exercise==0 && saved.reps==1 && saved.bpmSamples==0);
  startPress(200);latRaise();assert(exerciseReps[1]==1);
  startPress(200);exercisePress();assert(currentExercise==0 && sessionArchive.latestId()==3);
  assert(reboot.readId(3,saved) && saved.exercise==1 && saved.reps==1);
  // Save failure pauses and keeps the counts; a retry saves once.
  startPress(200);squat();assert(exerciseReps[0]==1);
  EEPROM.writes=0;EEPROM.failAfter=0;bool failed=false;
  try{endSession();}catch(int){failed=true;}assert(failed && exerciseReps[0]==1);
  EEPROM.failAfter=-1;endSession();assert(sessionArchive.latestId()==4 && exerciseReps[0]==0);
  // No BPM data is represented as absent, never a zero BPM measurement.
  assert(reboot.readId(4,saved) && saved.bpmSamples==0 && saved.bpmTenths==0);
  handleHistoryCommand("erase CONFIRM");assert(sessionArchive.latestId()==0 && sessionArchive.historyId()==2);
  EEPROMClass memory;PartAArchive<EEPROMClass> log(memory);
  for(int i=1;i<=25;++i){PartASavedSession r={0,10000,0,5,0,0,uint8_t(i%2)};assert(log.append(r));}
  assert(!log.readId(5,saved) && log.readId(6,saved) && log.readId(25,saved));
  for(int cut=0;cut<24;++cut){
    EEPROMClass broken=memory;broken.writes=0;broken.failAfter=cut;PartAArchive<EEPROMClass> partial(broken);
    PartASavedSession r={0,10000,0,3,750,3,0};try{partial.append(r);}catch(int){}
    assert(!partial.readId(26,saved) && partial.readId(25,saved));
  }
  memory.bytes[((25-1)%20)*23+11]^=1;assert(!log.readId(25,saved));
  for(int cut=0;cut<477;++cut){
    EEPROMClass interrupted=memory;interrupted.writes=0;interrupted.failAfter=cut;
    PartAArchive<EEPROMClass> erasing(interrupted);try{erasing.clear();}catch(int){}
    if(erasing.historyId()==2)assert(!erasing.readId(24,saved) && erasing.latestId()==0);
    else assert(erasing.readId(24,saved));
  }
  assert(log.clear());PartASavedSession fresh={0,5000,0,2,800,2,1};assert(log.append(fresh) && fresh.id==1 && fresh.historyId==2);
  std::cout<<"PASS: original A detectors/buttons, pause timing, save-before-reset/switch, BPM, persistence, ring and interrupted writes\n";
}
