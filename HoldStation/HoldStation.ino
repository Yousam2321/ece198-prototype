/* ECE198 Group 23 - wall-sit / plank station, UNO R3.
 * HC-SR04: 5V/GND, TRIG=D8, ECHO=D9.
 * Bare buttons: D2-GND (GO), D3-GND (MODE / CALIBRATE).
 * OLED I2C: SDA=A4 SCL=A5. No shield; follow printed power labels.
 */
#include <Arduino.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "PrototypeCore.h"
#include "Config.h"
#include "ExerciseProfiles.h"
#include <EEPROM.h>
#include "SavedSessions.h"
#include "HoldTiming.h"
#if ENABLE_OLED
#include <Wire.h>
#include <U8x8lib.h>
#if OLED_SH1106
U8X8_SH1106_128X64_NONAME_HW_I2C oled(U8X8_PIN_NONE);
#else
U8X8_SSD1306_128X64_NONAME_HW_I2C oled(U8X8_PIN_NONE);
#endif
#endif
using namespace ece198;
const uint8_t GO_PIN=2, MODE_PIN=3, TRIG_PIN=8, ECHO_PIN=9;
// Display address and driver settings are in Config.h.
Session session;
Button goButton, modeButton;
HoldTiming rangeTimer;
SessionArchive<EEPROMClass> archive(EEPROM);
uint8_t profile=0; // 0=wall sit, 1=plank
bool calibrated=false, calibrating=false, distanceValid=false, inside=false;
float targetCm=0, toleranceCm=HOLD_PROFILES[0].toleranceCm, distanceCm=0;
float history[5]={}, calSum=0, calSquareSum=0;
uint8_t historyCount=0, historyNext=0, calCount=0;
uint32_t lastRange=0, lastLog=0, lastDisplay=0, calAt=0;
char commandLine[48];
uint8_t commandLength=0;
const char* name() { return HOLD_PROFILES[profile].name; }
void emit(const char* type,const __FlashStringHelper* note) {
  Serial.print(type);Serial.print(F(",station,"));Serial.print(millis());
  Serial.print(',');Serial.print(name());Serial.print(',');Serial.print(stateName(session.state));
  Serial.print(',');Serial.print(session.active);Serial.print(',');
  if(distanceValid)Serial.print(distanceCm,1);
  Serial.print(',');if(calibrated)Serial.print(targetCm,1);
  Serial.print(',');Serial.print(toleranceCm,1);
  Serial.print(',');Serial.print(distanceValid?1:0);
  Serial.print(',');Serial.print(inside?1:0);
  Serial.print(',');Serial.print(rangeTimer.total);
  Serial.print(',');Serial.print(rangeTimer.gapMs);
  Serial.print(',');Serial.println(note);
}
void resetSession() {
  session.reset(millis());rangeTimer.clear();
  historyCount=0;distanceValid=false;inside=false;calibrating=false;
  emit("EVENT",F("reset_calibration_retained"));
}
// Changing exercise also clears calibration and loads that exercise's tolerance.
void chooseMode(uint8_t next) {
  if(session.state==DONE)resetSession();
  if(session.state!=IDLE || calibrating || next>1 || next==profile)return;
  profile=next;calibrated=false;toleranceCm=HOLD_PROFILES[profile].toleranceCm;rangeTimer.clear();
  historyCount=0;distanceValid=false;inside=false;
  emit("EVENT",F("mode_changed_recalibrate"));
}
// Average a stable reference distance for 2 seconds. Do this after mounting.
void startCalibration() {
  if(session.state==DONE)resetSession();
  if(session.state!=IDLE){emit("EVENT",F("calibrate_requires_idle"));return;}
  calibrating=true;calibrated=false;calSum=0;calSquareSum=0;calCount=0;calAt=millis();
  historyCount=0;distanceValid=false;inside=false;
  emit("EVENT",F("calibrate_2s_hold_reference_position"));
}
// Five-sample median rejects isolated spikes; it adds a short response delay.
float medianHistory() {
  float copy[5];
  for(uint8_t i=0;i<5;i++)copy[i]=history[i];
  for(uint8_t i=1;i<5;i++)for(uint8_t j=i;j>0 && copy[j]<copy[j-1];j--){
    float t=copy[j];copy[j]=copy[j-1];copy[j-1]=t;
  }
  return copy[2];
}
// Trigger an ultrasonic measurement every 75 ms (at least 60 ms between triggers).
// pulseIn has a timeout: missing echoes must not freeze the session indefinitely.
void measureDistance(uint32_t now) {
  if(uint32_t(now-lastRange)<75)return; // HC-SR04 trigger interval >60ms.
  lastRange=now;
  digitalWrite(TRIG_PIN,LOW);delayMicroseconds(3);
  digitalWrite(TRIG_PIN,HIGH);delayMicroseconds(10);digitalWrite(TRIG_PIN,LOW);
  uint32_t pulse=pulseIn(ECHO_PIN,HIGH,18000UL); // Timeout <20ms.
  uint32_t measuredAt=millis();
  float raw=pulse*0.0343f/2.0f;
  bool rawValid=pulse>0 && raw>=2.0f && raw<=200.0f;
  if(rawValid){
    history[historyNext]=raw;historyNext=(historyNext+1)%5;
    if(historyCount<5)++historyCount;
  }else historyCount=0;
  distanceValid=rawValid && historyCount==5;
  if(distanceValid)distanceCm=medianHistory();
  inside=distanceValid && calibrated && fabsf(distanceCm-targetCm)<=toleranceCm;
  if(calibrating && distanceValid){
    calSum+=distanceCm;calSquareSum+=distanceCm*distanceCm;
    if(calCount<255)++calCount;
  }
  if(calibrating && uint32_t(measuredAt-calAt)>=2000){
    calibrating=false;
    float mean=calCount?calSum/calCount:0;
    float variance=calCount?fmaxf(0,calSquareSum/calCount-mean*mean):1e6f;
    calibrated=calCount>=12 && mean>=12 && mean<=150 && variance<=4;
    if(calibrated)targetCm=mean;
    rangeTimer.clear();
    emit("EVENT",calibrated?F("reference_distance_saved"):F("calibration_failed_reposition"));
  }
  rangeTimer.update(measuredAt,session.state==RUNNING,distanceValid,inside);
}
void go() {
  if(calibrating){emit("EVENT",F("wait_for_calibration"));return;}
  // A completed session never toggles back to DONE on the next short press.
  if(session.state==DONE)resetSession();
  if(session.state==IDLE && !calibrated){emit("EVENT",F("calibrate_first"));return;}
  session.go(millis());rangeTimer.breakContinuity();
  emit("EVENT",F("button_or_go"));
}
// Finish only a session that actually started. Repeated presses cannot save twice.
void printSaved(const SavedSession& r) {
  Serial.print(F("SAVED,"));Serial.print(r.id);Serial.print(',');
  Serial.print(r.exercise?F("plank"):F("wall_sit"));Serial.print(',');
  Serial.print(r.activeMs);Serial.print(',');Serial.print(r.inBandMs);Serial.print(',');
  Serial.print(r.referenceMm/10.0f,1);Serial.print(',');
  Serial.println(r.toleranceMm/10.0f,1);
}
void showHistory() {
  if(session.state==RUNNING || session.state==COUNTDOWN){
    emit("EVENT",F("pause_before_history"));return;
  }
  Serial.println(F("# SAVED,id,exercise,active_ms,in_band_ms,reference_cm,band_cm"));
  uint32_t latest=archive.latestId();
  if(!latest){Serial.println(F("# No saved sessions"));return;}
  uint32_t first=latest>20?latest-19:1;
  SavedSession r;
  for(uint32_t id=first;;++id){
    if(archive.readId(id,r))printSaved(r);
    if(id==latest)break;
  }
}
void finishSession() {
  if(session.state!=RUNNING && session.state!=PAUSED)return;
  session.finish(millis());rangeTimer.breakContinuity();
  SavedSession r={0,session.active,rangeTimer.total,
    uint16_t(targetCm*10+0.5f),uint16_t(toleranceCm*10+0.5f),profile};
  bool saved=archive.append(r);
  emit("SUMMARY",saved?F("saved_to_eeprom"):F("save_failed"));
  if(saved)printSaved(r);
}
// Both wall sit and plank use this controller; only reference placement differs.
void handleCommand(char* line) {
  if(!strcmp(line,"go"))go();
  else if(!strcmp(line,"cal"))startCalibration();
  else if(!strcmp(line,"history"))showHistory();
  else if(!strcmp(line,"finish"))finishSession();
  else if(!strcmp(line,"reset") && session.state!=RUNNING)resetSession();
  else if(!strcmp(line,"wall"))chooseMode(0);
  else if(!strcmp(line,"plank"))chooseMode(1);
  else if(!strncmp(line,"band ",5) && (session.state==IDLE || session.state==DONE)){
    float n=atof(line+5);if(n>=1 && n<=20){toleranceCm=n;emit("EVENT",F("band_changed_cm"));}
  }
  else if(!strcmp(line,"version"))Serial.println(F("# ECE198_B_2.1"));
  else if(!strcmp(line,"help"))Serial.println(F("# go cal finish reset wall plank band CM history version"));
  else emit("EVENT",F("command_not_available"));
}
void readCommands() {
  while(Serial.available()){
    char ch=Serial.read();if(ch=='\r')continue;
    if(ch=='\n'){
      commandLine[commandLength]='\0';if(commandLength)handleCommand(commandLine);commandLength=0;
    }else if(commandLength<sizeof(commandLine)-1)commandLine[commandLength++]=ch;
    else commandLength=0;
  }
}
// Cache each padded row so unchanged text is not sent repeatedly.
// Drawing a complete padded row also erases leftover characters without blanking it first.
#if ENABLE_OLED
void writeDisplayLine(uint8_t row, const char* text) {
  static char previous[8][17] = {};
  char padded[17];
  uint8_t i=0;
  while(i<16 && text[i]){padded[i]=text[i];++i;}
  while(i<16)padded[i++]=' ';
  padded[16]='\0';
  if(strcmp(previous[row],padded)==0)return;
  oled.drawString(0,row,padded);
  memcpy(previous[row],padded,sizeof(padded));
}
#endif
// Text-only U8x8 avoids a large framebuffer on the memory-limited Uno R3.
void updateDisplay() {
#if ENABLE_OLED
  char text[17];
  writeDisplayLine(0,profile==0?"WALL SIT B2.1":"PLANK B2.1");
  writeDisplayLine(1,calibrating?"CALIBRATING":stateName(session.state));
  snprintf(text,sizeof(text),"Active %lus",(unsigned long)(session.active/1000));
  writeDisplayLine(2,text);
  snprintf(text,sizeof(text),"In band %lu.%lus",
    (unsigned long)(rangeTimer.total/1000),(unsigned long)((rangeTimer.total%1000)/100));
  writeDisplayLine(3,text);
  if(distanceValid){snprintf(text,sizeof(text),"Range %ucm",(unsigned int)(distanceCm+0.5f));writeDisplayLine(4,text);}
  else writeDisplayLine(4,"NO VALID ECHO");
  if(calibrated){snprintf(text,sizeof(text),"Ref %ucm +/-%u",(unsigned int)(targetCm+0.5f),(unsigned int)toleranceCm);writeDisplayLine(5,text);}
  else writeDisplayLine(5,"HOLD MODE TO CAL");
  writeDisplayLine(6,calibrated?(inside?"WITHIN BAND":"OUTSIDE BAND"):"NEEDS REFERENCE");
  writeDisplayLine(7,session.state==DONE?"GO=new attempt":
    (session.state==RUNNING?"GO=pause hold=end":"GO=start/resume"));
#endif
}
void setup() {
  Serial.begin(115200);
  pinMode(GO_PIN,INPUT_PULLUP);pinMode(MODE_PIN,INPUT_PULLUP);
  pinMode(TRIG_PIN,OUTPUT);pinMode(ECHO_PIN,INPUT);pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(TRIG_PIN,LOW);
#if ENABLE_OLED
  oled.setI2CAddress(OLED_ADDRESS<<1); // U8x8 takes the shifted 8-bit value.
  oled.begin();oled.setFont(u8x8_font_chroma48medium8_r);
#endif
  session.reset(millis());
  Serial.println(F("# ECE198_B_2.1"));
  Serial.println(F("type,device,uptime_ms,exercise,state,active_ms,distance_cm,reference_cm,band_cm,echo_valid,inside_band,range_ms,sample_gap_ms,note"));
  emit("EVENT",F("boot_type_help_for_commands"));
}
// Keep timing, buttons and sensor processing responsive; do not add long delays.
void loop() {
  uint32_t now=millis();
  RunState before=session.state;session.tick(now);
  if(before==COUNTDOWN && session.state==RUNNING){rangeTimer.breakContinuity();emit("EVENT",F("started"));}
  readCommands();
  now=millis(); // Commands/EEPROM reads may take time; sample buttons with fresh time.
  ButtonEvent a=goButton.update(digitalRead(GO_PIN)==LOW,now);
  ButtonEvent b=modeButton.update(digitalRead(MODE_PIN)==LOW,now);
  if(a==SHORT_PRESS){emit("EVENT",F("go_short"));go();}
  // Long GO is ignored in IDLE, COUNTDOWN and DONE: no empty sessions.
  if(a==LONG_PRESS){emit("EVENT",F("go_long"));finishSession();}
  if(b==SHORT_PRESS)chooseMode(1-profile);
  if(b==LONG_PRESS)startCalibration();
  measureDistance(now);
  if(uint32_t(millis()-lastRange)>150){distanceValid=false;inside=false;rangeTimer.breakContinuity();}
  digitalWrite(LED_BUILTIN,session.state==RUNNING && inside);
  if(uint32_t(now-lastDisplay)>=250){lastDisplay=now;updateDisplay();}
  if(uint32_t(now-lastLog)>=1000){lastLog=now;emit("DATA",F("status"));}
}
