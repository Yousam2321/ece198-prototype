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
#if defined(ARDUINO_ARCH_RENESAS_UNO)
bool fakeUsbInterruptsEnabled=true,fakeUsbCloseOnSubmit=false,fakeUsbFullOnSubmit=false;
#endif
#include "../PartA/PartA.ino"
void advanceA(uint32_t ms){for(uint32_t i=0;i<ms;++i){++fakeNow;loop();}}
void startPress(uint32_t ms){fakePins[START_BUTTON]=LOW;advanceA(ms);fakePins[START_BUTTON]=HIGH;advanceA(70);}
void exercisePress(){fakePins[EXERCISE_BUTTON]=LOW;advanceA(200);fakePins[EXERCISE_BUTTON]=HIGH;advanceA(70);}
void squat(){fakeAnalog[Z_PIN]=412;advanceA(250);fakeAnalog[Z_PIN]=512;advanceA(350);}
void latRaise(){fakeAnalog[Y_PIN]=612;advanceA(250);fakeAnalog[Y_PIN]=512;advanceA(350);}
void openHistoryMenu(){
  fakePins[START_BUTTON]=LOW;fakePins[EXERCISE_BUTTON]=LOW;advanceA(3200);
  assert(historyMenu==HISTORY_CONFIRM && !exerciseRunning);
  fakePins[START_BUTTON]=HIGH;fakePins[EXERCISE_BUTTON]=HIGH;advanceA(70);
  assert(historyArmed);
}
int main(){
#if defined(ARDUINO_ARCH_RENESAS_UNO)
  Serial.rejectCoreWrite=true;Serial.rejectCorePoll=true; // Exercise actual R4 transport selection.
#endif
  for(int& v:fakePins)v=HIGH;
  setup();assert(sessionArchive.latestId()==0 && !exerciseRunning);
  startPress(200);assert(exerciseRunning && sessionTouched[0]);
  squat();assert(exerciseReps[0]==1);advanceA(1300);squat();assert(exerciseReps[0]==2);
  startPress(1200);assert(!exerciseRunning && !sessionEnded && sessionArchive.latestId()==0);
  uint32_t stopped=activeMs[0];advanceA(1500);assert(activeMs[0]==stopped);
  startPress(200);assert(exerciseRunning && exerciseReps[0]==2);
  pulseSensorReady=true;fakeIr=60000;fakeBeat=true;updatePulseSensor();
  fakeNow+=1000;updateSessionClock();fakeBeat=true;updatePulseSensor();
  assert(sessionBpmSamples[0]==1);fakeIr=0;updatePulseSensor();assert(averageBPM==0);
  startPress(3200);assert(sessionEnded && !exerciseRunning && exerciseReps[0]==0);
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
  startPress(200);
  // A shallow Y excursion used to satisfy the old 20-unit threshold. With the
  // uploaded 25-unit threshold, baseline tracking leaves fewer than 3 qualifying
  // readings, so returning must not produce a rep. Exercise the real ADC loop.
  fakeAnalog[Y_PIN]=540;advanceA(250);fakeAnalog[Y_PIN]=512;advanceA(350);
  assert(exerciseReps[1]==0 && !latRaiseDetected);
  // A brief spike also must not become a rep; a full raise/return still counts.
  fakeAnalog[Y_PIN]=612;advanceA(50);fakeAnalog[Y_PIN]=512;advanceA(350);
  assert(exerciseReps[1]==0 && !latRaiseDetected);
  latRaise();assert(exerciseReps[1]==1);
  // A partial lateral raise must not bridge across the history-menu pause.
  advanceA(1300);fakeAnalog[Y_PIN]=612;advanceA(250);assert(latRaiseDetected);
  EEPROMClass beforeLatMenu=EEPROM;openHistoryMenu();
  assert(!latRaiseDetected && exerciseReps[1]==1);
  exercisePress();advanceA(2050);assert(exerciseRunning && historyMenu==HISTORY_NORMAL);
  fakeAnalog[Y_PIN]=512;advanceA(350);assert(exerciseReps[1]==1);
  assert(memcmp(beforeLatMenu.bytes,EEPROM.bytes,1024)==0);
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
  // Button-only history menu: consume both-button holds before normal handlers.
  PartASavedSession menuRecord={0,5000,0,5,0,0,0};assert(sessionArchive.append(menuRecord));
  EEPROMClass beforeMenu=EEPROM;int selected=currentExercise;
  fakePins[START_BUTTON]=LOW;fakePins[EXERCISE_BUTTON]=LOW;advanceA(2000);
  fakePins[START_BUTTON]=HIGH;fakePins[EXERCISE_BUTTON]=HIGH;advanceA(70);
  assert(historyMenu==HISTORY_NORMAL && !exerciseRunning && currentExercise==selected);
  assert(memcmp(beforeMenu.bytes,EEPROM.bytes,1024)==0); // Insufficient hold ignored.
  // EXERCISE first: do not switch before START joins the chord.
  fakePins[EXERCISE_BUTTON]=LOW;advanceA(1000);assert(currentExercise==selected);
  fakePins[START_BUTTON]=LOW;advanceA(3100);assert(historyMenu==HISTORY_CONFIRM);
  assert(lcd.rows[0].find("ERASE HISTORY?")==0 && lcd.rows[1].find("START=YES EX=NO")==0);
  advanceA(1500);assert(memcmp(beforeMenu.bytes,EEPROM.bytes,1024)==0);
  fakePins[START_BUTTON]=HIGH;fakePins[EXERCISE_BUTTON]=HIGH;advanceA(70);
  assert(historyArmed && !exerciseRunning && sessionArchive.latestId()==1);
  exercisePress();assert(historyMenu==HISTORY_RESULT && lcd.rows[0].find("ERASE CANCELLED")==0);
  assert(memcmp(beforeMenu.bytes,EEPROM.bytes,1024)==0 && currentExercise==selected);
  advanceA(2050);assert(historyMenu==HISTORY_NORMAL);
  // START first: its old one-second hold cannot save or start the session.
  fakePins[START_BUTTON]=LOW;advanceA(1200);fakePins[EXERCISE_BUTTON]=LOW;advanceA(3100);
  assert(historyMenu==HISTORY_CONFIRM && sessionArchive.latestId()==1);
  fakePins[START_BUTTON]=HIGH;advanceA(100);assert(!historyArmed);
  fakePins[EXERCISE_BUTTON]=HIGH;advanceA(70);assert(historyArmed);
  // Both again while confirming must not erase on either release.
  fakePins[START_BUTTON]=LOW;fakePins[EXERCISE_BUTTON]=LOW;advanceA(300);
  fakePins[START_BUTTON]=HIGH;fakePins[EXERCISE_BUTTON]=HIGH;advanceA(70);
  assert(sessionArchive.latestId()==1 && historyArmed);
  startPress(200);assert(historyMenu==HISTORY_RESULT && sessionArchive.latestId()==0);
  assert(lcd.rows[0].find("HISTORY CLEARED")==0 && !exerciseRunning && currentExercise==selected);
  assert(sessionArchive.historyId()==3);
  advanceA(2050);assert(historyMenu==HISTORY_NORMAL);
  PartAArchive<EEPROMClass> clearedReboot(EEPROM);assert(clearedReboot.latestId()==0);
  // Menu works while running: cancel preserves history/current count and returns
  // to RUNNING, excluding menu time and never saving the attempt accidentally.
  assert(sessionArchive.append(menuRecord)); // A saved result to preserve/erase.
  startPress(200);squat();assert(exerciseRunning && exerciseReps[0]==1);
  beforeMenu=EEPROM;
  fakePins[START_BUTTON]=LOW;fakePins[EXERCISE_BUTTON]=LOW;advanceA(2000);
  uint32_t duringMenu=activeMs[0];advanceA(200);assert(activeMs[0]==duringMenu);
  fakePins[START_BUTTON]=HIGH;fakePins[EXERCISE_BUTTON]=HIGH;advanceA(70);
  assert(exerciseRunning && historyMenu==HISTORY_NORMAL && exerciseReps[0]==1);
  openHistoryMenu();duringMenu=activeMs[0];advanceA(1000);assert(activeMs[0]==duringMenu);
  exercisePress();assert(historyMenu==HISTORY_RESULT && !exerciseRunning);
  advanceA(2050);assert(exerciseRunning && historyMenu==HISTORY_NORMAL && exerciseReps[0]==1);
  assert(uint32_t(activeMs[0]-duringMenu)<200 && memcmp(beforeMenu.bytes,EEPROM.bytes,1024)==0);
  // Confirm while running: erase only saved records, then continue this attempt.
  openHistoryMenu();startPress(200);assert(sessionArchive.latestId()==0 && sessionArchive.historyId()==4);
  assert(exerciseReps[0]==1 && sessionTouched[0] && !sessionEnded);
  advanceA(2050);assert(exerciseRunning && historyMenu==HISTORY_NORMAL);
  // Paused attempts stay paused after both cancel and confirmed clear.
  startPress(200);uint32_t pausedAt=activeMs[0];assert(!exerciseRunning);
  assert(sessionArchive.append(menuRecord));beforeMenu=EEPROM;
  openHistoryMenu();exercisePress();advanceA(2050);
  assert(!exerciseRunning && !sessionEnded && activeMs[0]==pausedAt && exerciseReps[0]==1);
  assert(memcmp(beforeMenu.bytes,EEPROM.bytes,1024)==0);
  openHistoryMenu();startPress(200);advanceA(2050);
  assert(!exerciseRunning && !sessionEnded && activeMs[0]==pausedAt && exerciseReps[0]==1);
  assert(sessionArchive.latestId()==0 && sessionArchive.historyId()==5 && sessionTouched[0]);
  startPress(200);assert(exerciseRunning);startPress(3200);assert(sessionEnded && sessionArchive.latestId()==1);
  assert(sessionArchive.readId(1,saved) && saved.reps==1 && saved.historyId==5);
  // Export through the actual command parser, close the host port, and use the
  // same running sketch again. No reboot/reset is supplied by this test.
  Serial.output.clear();Serial.input="history\n";advanceA(2);
  assert(Serial.output.find("# END_A_HISTORY")!=std::string::npos);
  Serial.connected=false;Serial.space=0;Serial.output.clear();
  // Reproduce the actual R4 core: available()>0 while a closed port's read()
  // returns -1 without consuming bytes. The sketch must still service buttons.
  Serial.input="\r\n";serialCommandLength=3;Serial.failedReads=0;
  advanceA(1);assert(Serial.failedReads==1 && serialCommandLength==0);
  startPress(200);squat();assert(exerciseRunning && exerciseReps[0]==1);
  startPress(200);assert(!exerciseRunning);startPress(200);assert(exerciseRunning);
  startPress(3200);assert(sessionEnded && sessionArchive.latestId()==2 && Serial.output.empty());
  assert(sessionArchive.readId(2,saved) && saved.reps==1);
  // An open host that stops reading also cannot trap a byte write forever.
  Serial.connected=true;safeSerial.resetError();uint32_t blockedAt=fakeNow;
  safeSerial.print("x");assert(safeSerial.failed() && uint32_t(fakeNow-blockedAt)<=20);
  Serial.output.clear();Serial.input="history\n";advanceA(1);
  assert(Serial.output.find("# END_A_HISTORY")==std::string::npos); // No success on partial transfer.
  Serial.space=64;Serial.input="history\n";Serial.output.clear();advanceA(2);
  assert(Serial.output.find("ASAVED,2,squats,")!=std::string::npos);
  assert(Serial.output.find("# END_A_HISTORY")!=std::string::npos);
  // A long input burst is processed over several loop passes, leaving time for
  // LCD/keys between chunks rather than draining an unbounded stream at once.
  Serial.input=std::string(100,'x');serialCommandLength=0;
  readHistoryCommands();assert(Serial.input.size()==68);
  Serial.input.clear();serialCommandLength=0;
#if defined(ARDUINO_ARCH_RENESAS_UNO)
  // Host close or full FIFO between the preliminary check and submission must
  // return immediately, flag transfer failure, and always restore USB IRQs.
  safeSerial.resetError();fakeUsbCloseOnSubmit=true;uint32_t racedAt=fakeNow;
  assert(safeSerial.write('x')==0 && safeSerial.failed());
  assert(fakeUsbInterruptsEnabled && fakeNow==racedAt);
  Serial.connected=true;safeSerial.resetError();fakeUsbFullOnSubmit=true;
  assert(safeSerial.write('x')==0 && safeSerial.failed());
  assert(fakeUsbInterruptsEnabled && fakeNow==racedAt);
  Serial.space=64;safeSerial.resetError();assert(safeSerial.write('x')==1);
  assert(fakeUsbInterruptsEnabled && !safeSerial.failed());
  for(int cycle=0;cycle<3;++cycle){
    Serial.connected=false;Serial.space=0;startPress(200);assert(exerciseRunning);
    startPress(200);assert(!exerciseRunning);startPress(200);assert(exerciseRunning);
    startPress(3200);assert(sessionEnded);
    Serial.connected=true;Serial.space=64;Serial.input="history\n";Serial.output.clear();advanceA(2);
    assert(Serial.output.find("# END_A_HISTORY")!=std::string::npos);
    assert(fakeUsbInterruptsEnabled);
  }
#endif
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
  std::cout<<"PASS: A detectors, 3s finish, timing, saves, BPM, erase in every session state / cancel / retained attempt, export close/stall recovery, persistence and interrupted writes\n";
}
