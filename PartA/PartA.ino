/*
 * ECE198 Part A A2.0 -- explicit session controller, as used by Part B.
 * Arduino Uno R4 Minima. Existing wiring and exercise criteria are retained.
 * START D2 -> GND; EXERCISE D3 -> GND (INPUT_PULLUP).
 * MMA7361: X=A0, Y=A1, Z=A2. LCD: RS=7, E=8, data=9,10,11,12.
 * MAX30102: SDA/SCL, with the same verified breakout power wiring as before.
 *
 * START tap: start 3s countdown / pause / resume / next attempt after DONE.
 * START hold 3s: finish/save once. EXERCISE tap at READY/DONE: next exercise.
 * EXERCISE hold 2s while paused: cancel confirmation (START yes, EXERCISE no).
 * Both hold 3s: erase-history confirmation in any state; release, then choose.
 * Normal use is silent on serial. Commands: version, history, status,
 * erase CONFIRM (while not running). CSV/EEPROM format matches A1.x.
 */
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "PartACore.h"
#include "PartASessions.h"
#include "PartASerial.h"
#include <stdio.h>
#include <string.h>
#if defined(ARDUINO_ARCH_AVR)
#error "Part A A2.0 targets the Uno R4 Minima. Select that board; Part B uses the Uno R3."
#endif

using namespace parta;
const uint8_t START_BUTTON=2, EXERCISE_BUTTON=3;
const uint8_t X_PIN=A0,Y_PIN=A1,Z_PIN=A2;
LiquidCrystal lcd(7,8,9,10,11,12);
MAX30105 pulseSensor;
PartAArchive<EEPROMClass> sessionArchive(EEPROM);
PartASerialLink<decltype(Serial)> serialLink(Serial);
Session session;
Key startKey,exerciseKey;
uint8_t currentExercise=0; // 0=squats, 1=lateral raises
uint16_t repCount=0;
uint32_t bpmSumTenths=0,lastBeatAt=0;
uint16_t bpmSamples=0,displayBpm=0;
bool beatSeen=false,pulseSensorReady=false,hasAttempt=false,saveFailed=false,saved=false;
uint32_t savedId=0;

// Keep the team's original detector thresholds, confirmation samples, EMA
// baseline tracking (alpha .05) and 1200 ms movement-entry cooldown.
const float SQUAT_DOWN_THRESHOLD=-40.0f,SQUAT_RETURN_THRESHOLD=-5.0f;
const float LAT_RAISE_UP_THRESHOLD=25.0f,LAT_RAISE_RETURN_THRESHOLD=5.0f;
bool squatDownDetected=false,latRaiseDetected=false;
uint8_t squatDownSamples=0,squatReturnSamples=0,latRaiseUpSamples=0,latRaiseReturnSamples=0;
uint32_t lastSquatRepTime=0,lastLatRaiseRepTime=0;
float gravityX=0,gravityY=0,gravityZ=0,x=0,y=0,z=0;
bool calibrating=true;
uint16_t calibrationSamples=0;
uint32_t calibrationAt=0,lastAccelAt=0,lastPulseAt=0,lastPulseHealthAt=0,lastPulseSampleAt=0;
const uint32_t FINGER_THRESHOLD=50000UL;
uint8_t rates[5]={},rateSpot=0,validRates=0;

// Menus pause the session without changing the attempt or saved archive.
// The original state is restored only after the menu/result has finished.
enum Menu : uint8_t { NORMAL, ERASE_HOLD, ERASE_CONFIRM, CANCEL_CONFIRM, NOTICE };
Menu menu=NORMAL;
State menuReturnState=IDLE;
Key menuStart,menuExercise;
bool menuArmed=false,chordTiming=false;
uint32_t menuAt=0,chordAt=0,noticeAt=0;
const char* noticeTop="";
const char* noticeBottom="";
char lcdCache[2][17]={{0},{0}};
uint32_t lastDisplayAt=0;

// At most one CSV row is queued. Exports move one row/chunk each main-loop
// pass, so host close/stall never becomes a wait loop that blocks the buttons.
enum Reply : uint8_t { NO_REPLY, HISTORY_REPLY, VERSION_REPLY, STATUS_REPLY, ERASE_REPLY };
Reply reply=NO_REPLY;
uint8_t replyPhase=0,txLength=0,txAt=0;
char tx[144],command[32];
uint8_t commandLength=0;
uint32_t exportFirst=0,exportLast=0,exportNext=0,exportGeneration=0,serialProgressAt=0;
bool hostWasConnected=false,serialEraseSucceeded=false;

const char* exerciseName(){return currentExercise==0?"squats":"lateral_raises";}
const char* screenName(){return currentExercise==0?"SQ":"LAT";}
void renderDisplay(bool force=false);

void resetDetectors(){
  squatDownDetected=latRaiseDetected=false;
  squatDownSamples=squatReturnSamples=latRaiseUpSamples=latRaiseReturnSamples=0;
}
void resetAttempt(uint32_t now){
  session.reset(now);repCount=0;bpmSumTenths=0;bpmSamples=0;
  hasAttempt=false;saveFailed=false;saved=false;savedId=0;
  lastSquatRepTime=lastLatRaiseRepTime=0;resetDetectors();
}
void abortReply(){reply=NO_REPLY;txLength=txAt=0;replyPhase=0;}
void finishAttempt(uint32_t now){
  if(!hasAttempt || (session.state!=RUNNING && session.state!=PAUSED))return;
  session.tick(now);session.state=SAVING;resetDetectors();
  renderDisplay(true); // Show saving before touching EEPROM.
  abortReply(); // A changing archive must never finish an old partial export.
  PartASavedSession record={0,session.active,0,repCount,
    bpmSamples?uint16_t((bpmSumTenths+bpmSamples/2)/bpmSamples):uint16_t(0),
    bpmSamples,currentExercise};
  bool ok=sessionArchive.append(record);
  session.last=millis(); // Never credit save time.
  if(ok){saved=true;savedId=record.id;saveFailed=false;session.state=DONE;}
  else {saveFailed=true;session.state=PAUSED;} // Keep the attempt for retry.
  renderDisplay(true);
}
void startOrToggle(uint32_t now){
  resetDetectors();
  if(session.state==IDLE || session.state==DONE){
    resetAttempt(now);session.start(now);hasAttempt=true;
  }else if(session.state==COUNTDOWN){resetAttempt(now);}
  else if(session.state==RUNNING){session.tick(now);session.state=PAUSED;}
  else if(session.state==PAUSED){session.last=now;session.state=RUNNING;saveFailed=false;}
}
void enterMenu(Menu next,uint32_t now){
  session.tick(now);menuReturnState=session.state;menuAt=now;
  session.state=PAUSED;menu=next;menuArmed=false;chordTiming=false;
  menuStart.suppress(true,now);menuExercise.suppress(true,now);
  resetDetectors();
}
void leaveMenu(uint32_t now){
  if(menuReturnState==COUNTDOWN)session.countdownAt+=uint32_t(now-menuAt);
  session.state=menuReturnState;session.last=now;menu=NORMAL;
  startKey.suppress(digitalRead(START_BUTTON)==LOW,now);
  exerciseKey.suppress(digitalRead(EXERCISE_BUTTON)==LOW,now);
  resetDetectors();
}
void showNotice(const char* top,const char* bottom,uint32_t now){
  noticeTop=top;noticeBottom=bottom;noticeAt=now;menu=NOTICE;
}
bool eraseSavedHistory(){
  abortReply();
  bool ok=sessionArchive.clear();
  if(ok){saved=false;savedId=0;} // A DONE result is retained on screen, not re-saved.
  return ok;
}
void serviceButtons(uint32_t now){
  bool go=digitalRead(START_BUTTON)==LOW,ex=digitalRead(EXERCISE_BUTTON)==LOW;
  if(calibrating){startKey.suppress(go,now);exerciseKey.suppress(ex,now);return;}
  if(menu==NORMAL && go && ex)enterMenu(ERASE_HOLD,now);
  if(menu!=NORMAL){
    startKey.suppress(go,now);exerciseKey.suppress(ex,now);
    if(menu==ERASE_HOLD){
      if(go && ex){
        if(!chordTiming){chordAt=now;chordTiming=true;}
        if(uint32_t(now-chordAt)>=3000){
          menu=ERASE_CONFIRM;menuArmed=false;
          menuStart.suppress(go,now);menuExercise.suppress(ex,now);
        }
      }else {chordTiming=false;if(!go && !ex)leaveMenu(now);}
      return;
    }
    if(menu==NOTICE){
      if(!go && !ex && uint32_t(now-noticeAt)>=2000)leaveMenu(now);
      return;
    }
    if(!menuArmed){
      if(!go && !ex){
        menuArmed=true;menuStart.suppress(false,now);menuExercise.suppress(false,now);
      }
      return;
    }
    if(go && ex){menuArmed=false;return;}
    KeyEvent yes=menuStart.update(go,now,3000);
    KeyEvent no=menuExercise.update(ex,now,2000);
    if(yes!=NO_KEY){
      if(menu==ERASE_CONFIRM){
        renderDisplay(true);
        bool ok=eraseSavedHistory();
        showNotice(ok?"HISTORY CLEARED":"ERASE FAILED","Attempt kept",millis());
      }else{
        resetAttempt(now);menuReturnState=IDLE;
        showNotice("NOT SAVED","Attempt cancelled",now);
      }
    }else if(no!=NO_KEY)showNotice("CANCELLED",menu==ERASE_CONFIRM?"History kept":"Attempt kept",now);
    return;
  }
  KeyEvent goEvent=startKey.update(go,now,3000);
  KeyEvent exEvent=exerciseKey.update(ex,now,2000);
  if(goEvent==TAP)startOrToggle(now);
  else if(goEvent==HOLD){
    if(session.state==COUNTDOWN)resetAttempt(now);
    else finishAttempt(now);
  }
  if(exEvent==TAP && (session.state==IDLE || session.state==DONE)){
    currentExercise=uint8_t((currentExercise+1)%2);resetAttempt(now);
  }else if(exEvent==HOLD && session.state==PAUSED){
    enterMenu(CANCEL_CONFIRM,now);
  }
}
void updateSquatCounter(uint32_t now){
  if(currentExercise!=0 || session.state!=RUNNING){squatDownDetected=false;squatDownSamples=squatReturnSamples=0;return;}
  if(!squatDownDetected){
    if(z<=SQUAT_DOWN_THRESHOLD){if(squatDownSamples<3)++squatDownSamples;}else squatDownSamples=0;
    if(squatDownSamples>=3 && (!lastSquatRepTime || uint32_t(now-lastSquatRepTime)>=1200)){
      squatDownDetected=true;squatDownSamples=squatReturnSamples=0;
    }
  }else{
    if(z>=SQUAT_RETURN_THRESHOLD){if(squatReturnSamples<8)++squatReturnSamples;}else squatReturnSamples=0;
    if(squatReturnSamples>=8){
      if(repCount<32767)++repCount;
      lastSquatRepTime=now;
      squatDownDetected=false;squatDownSamples=squatReturnSamples=0;
    }
  }
}
void updateLatRaiseCounter(uint32_t now){
  if(currentExercise!=1 || session.state!=RUNNING){latRaiseDetected=false;latRaiseUpSamples=latRaiseReturnSamples=0;return;}
  if(!latRaiseDetected){
    if(y>=LAT_RAISE_UP_THRESHOLD){if(latRaiseUpSamples<3)++latRaiseUpSamples;}else latRaiseUpSamples=0;
    if(latRaiseUpSamples>=3 && (!lastLatRaiseRepTime || uint32_t(now-lastLatRaiseRepTime)>=1200)){
      latRaiseDetected=true;latRaiseUpSamples=latRaiseReturnSamples=0;
    }
  }else{
    if(y<=LAT_RAISE_RETURN_THRESHOLD){if(latRaiseReturnSamples<8)++latRaiseReturnSamples;}else latRaiseReturnSamples=0;
    if(latRaiseReturnSamples>=8){
      if(repCount<32767)++repCount;
      lastLatRaiseRepTime=now;
      latRaiseDetected=false;latRaiseUpSamples=latRaiseReturnSamples=0;
    }
  }
}
void serviceAccelerometer(uint32_t now){
  if(calibrating){
    if(uint32_t(now-calibrationAt)<5)return;
    calibrationAt=now;
    gravityX+=analogRead(X_PIN);gravityY+=analogRead(Y_PIN);gravityZ+=analogRead(Z_PIN);
    if(++calibrationSamples==200){
      gravityX/=200;gravityY/=200;gravityZ/=200;calibrating=false;
      lastAccelAt=now;session.last=now;
    }
    return;
  }
  uint32_t gap=uint32_t(now-lastAccelAt);
  if(gap<25)return;
  if(gap>150)resetDetectors(); // Never bridge a stalled/missing sample sequence.
  lastAccelAt=now;
  float rawX=analogRead(X_PIN),rawY=analogRead(Y_PIN),rawZ=analogRead(Z_PIN);
  gravityX+=.05f*(rawX-gravityX);gravityY+=.05f*(rawY-gravityY);gravityZ+=.05f*(rawZ-gravityZ);
  x=rawX-gravityX;y=rawY-gravityY;z=rawZ-gravityZ;
  updateSquatCounter(now);updateLatRaiseCounter(now);
}
void clearPulseReading(){
  displayBpm=0;beatSeen=false;validRates=rateSpot=0;
}
void acceptPulseSample(uint32_t ir,uint32_t now){
  if(ir<FINGER_THRESHOLD){clearPulseReading();return;}
  if(!checkForBeat(ir))return;
  if(beatSeen){
    uint32_t dt=uint32_t(now-lastBeatAt);
    if(dt){
      float bpm=60000.0f/dt;
      if(bpm>=40 && bpm<=180){
        rates[rateSpot]=uint8_t(bpm);rateSpot=uint8_t((rateSpot+1)%5);
        if(validRates<5)++validRates;
        uint16_t sum=0;for(uint8_t i=0;i<validRates;++i)sum+=rates[i];
        displayBpm=sum/validRates;
        if(session.state==RUNNING && bpmSamples<65535){
          bpmSumTenths+=uint16_t(bpm*10+.5f);++bpmSamples;
        }
      }
    }
  }
  lastBeatAt=now;beatSeen=true;
}
void servicePulse(uint32_t now){
  if(!pulseSensorReady || uint32_t(now-lastPulseAt)<10)return;
  lastPulseAt=now;
  if(uint32_t(now-lastPulseHealthAt)>=1000){
    lastPulseHealthAt=now;
    Wire.beginTransmission(uint8_t(0x57));
    if(Wire.endTransmission()!=0){pulseSensorReady=false;clearPulseReading();return;}
  }
  // No getIR()/safeCheck(250) wait. Poll once and consume buffered samples.
  pulseSensor.check();
  bool haveSample=false;uint32_t ir=0;
  for(uint8_t i=0;i<4 && pulseSensor.available();++i){
    ir=pulseSensor.getFIFOIR();pulseSensor.nextSample();haveSample=true;
  }
  if(haveSample){lastPulseSampleAt=now;acceptPulseSample(ir,now);}
  else if(uint32_t(now-lastPulseSampleAt)>1500)clearPulseReading();
}
void setLine(uint8_t row,const char* value){
  char padded[17];memset(padded,' ',16);padded[16]=0;
  size_t n=strlen(value);if(n>16)n=16;memcpy(padded,value,n);
  if(strcmp(padded,lcdCache[row])){
    lcd.setCursor(0,row);lcd.print(padded);memcpy(lcdCache[row],padded,17);
  }
}
void renderDisplay(bool force){
  uint32_t now=millis();
  if(!force && uint32_t(now-lastDisplayAt)<100)return;
  lastDisplayAt=now;
  char top[24],bottom[24];
  if(calibrating){strcpy(top,"Calibrating...");strcpy(bottom,"Keep sensor still");}
  else if(menu==ERASE_CONFIRM){strcpy(top,"ERASE HISTORY?");strcpy(bottom,"GO:yes EX:no");}
  else if(menu==CANCEL_CONFIRM){strcpy(top,"CANCEL ATTEMPT?");strcpy(bottom,"GO:yes EX:no");}
  else if(menu==NOTICE){strcpy(top,noticeTop);strcpy(bottom,noticeBottom);}
  else if(menu==ERASE_HOLD){strcpy(top,"Hold both 3 sec");strcpy(bottom,"History menu...");}
  else if(session.state==IDLE){
    snprintf(top,sizeof(top),"%s A2.0 READY",screenName());strcpy(bottom,"GO:start EX:next");
  }else if(session.state==COUNTDOWN){
    uint32_t elapsed=uint32_t(now-session.countdownAt);
    uint8_t seconds=elapsed>=3000?0:uint8_t(3-elapsed/1000);
    snprintf(top,sizeof(top),"%s START IN %u",screenName(),seconds);strcpy(bottom,"GO:cancel");
  }else if(session.state==SAVING){strcpy(top,"SAVING...");strcpy(bottom,"Please wait");}
  else if(session.state==DONE){
    snprintf(top,sizeof(top),"%s %s R%u",screenName(),saved?"SAVED":"DONE",repCount);
    snprintf(bottom,sizeof(bottom),"%lus GO:new EX:>",(unsigned long)(session.active/1000));
  }else if(saveFailed){
    strcpy(top,"SAVE FAILED");strcpy(bottom,"Hold GO 3s retry");
  }else{
    snprintf(top,sizeof(top),"%s %s",screenName(),stateName(session.state));
    char pulse[6]="---";if(displayBpm)snprintf(pulse,sizeof(pulse),"%u",displayBpm);
    snprintf(bottom,sizeof(bottom),"R%u T%lus B%s",repCount,(unsigned long)(session.active/1000),pulse);
  }
  setLine(0,top);setLine(1,bottom);
}
void queueLine(const char* line){
  size_t size=strlen(line);if(size>=sizeof(tx))size=sizeof(tx)-1;
  memcpy(tx,line,size);tx[size]=0;txLength=uint8_t(size);txAt=0;
}
void queueSaved(const PartASavedSession& r){
  char pulse[8]="";
  if(r.bpmSamples)snprintf(pulse,sizeof(pulse),"%u.%u",r.bpmTenths/10,r.bpmTenths%10);
  snprintf(tx,sizeof(tx),"ASAVED,%lu,%s,%lu,%u,%s,%u,%lu\n",
    (unsigned long)r.id,r.exercise==0?"squats":"lateral_raises",
    (unsigned long)r.activeMs,r.reps,pulse,r.bpmSamples,(unsigned long)r.historyId);
  txLength=uint8_t(strlen(tx));txAt=0;
}
void beginReply(Reply kind,uint32_t now){
  abortReply();reply=kind;serialProgressAt=now;
}
void handleCommand(const char* value,uint32_t now){
  if(!strcmp(value,"version")){beginReply(VERSION_REPLY,now);return;}
  if(!strcmp(value,"status")){beginReply(STATUS_REPLY,now);return;}
  if(!strcmp(value,"history")){
    beginReply(HISTORY_REPLY,now);
    if(calibrating || menu!=NORMAL || session.state==RUNNING || session.state==COUNTDOWN){
      queueLine("A_EVENT,part_a,pause_before_history\n");reply=NO_REPLY;return;
    }
    exportLast=sessionArchive.latestId();exportFirst=exportLast>20?exportLast-19:1;
    exportNext=exportFirst;exportGeneration=sessionArchive.historyId();
    return;
  }
  if(!strcmp(value,"erase CONFIRM") && !calibrating && menu==NORMAL &&
      session.state!=RUNNING && session.state!=COUNTDOWN && session.state!=SAVING){
    enterMenu(NOTICE,now);
    serialEraseSucceeded=eraseSavedHistory();
    showNotice(serialEraseSucceeded?"HISTORY CLEARED":"ERASE FAILED","Attempt kept",millis());
    beginReply(ERASE_REPLY,millis());return;
  }
  beginReply(NO_REPLY,now);queueLine("A_EVENT,part_a,command_not_available\n");
}
void prepareReply(){
  if(reply==VERSION_REPLY){queueLine("# DEVICE,part_a,A2.0\n");reply=NO_REPLY;}
  else if(reply==STATUS_REPLY){
    snprintf(tx,sizeof(tx),"A_STATUS,%s,%s,%lu,%u,%u,%u,%u\n",exerciseName(),stateName(session.state),
      (unsigned long)session.active,repCount,pulseSensorReady?1:0,
      digitalRead(START_BUTTON)==LOW?1:0,digitalRead(EXERCISE_BUTTON)==LOW?1:0);
    txLength=uint8_t(strlen(tx));txAt=0;reply=NO_REPLY;
  }else if(reply==ERASE_REPLY){
    queueLine(serialEraseSucceeded?"A_EVENT,part_a,history_cleared\n":"A_EVENT,part_a,erase_failed\n");reply=NO_REPLY;
  }else if(reply==HISTORY_REPLY){
    if(sessionArchive.historyId()!=exportGeneration){
      queueLine("A_EVENT,part_a,pause_before_history\n");reply=NO_REPLY;return;
    }
    if(replyPhase==0){
      queueLine("# ASAVED,id,exercise,active_ms,reps,bpm_avg,bpm_samples,history_id\n");replyPhase=1;
    }else if(!exportLast){queueLine("# No saved A sessions\n");reply=NO_REPLY;}
    else if(replyPhase==1){
      PartASavedSession r;
      uint32_t id=exportNext; // At most one record search per main-loop pass.
      if(id==exportLast)replyPhase=2;else ++exportNext;
      if(sessionArchive.readId(id,r))queueSaved(r);
    }else{queueLine("# END_A_HISTORY\n");reply=NO_REPLY;}
  }
}
void serviceSerial(uint32_t now){
  bool connected=serialLink.connected();
  if(!connected){abortReply();commandLength=0;hostWasConnected=false;return;}
  if(!hostWasConnected){commandLength=0;hostWasConnected=true;}
  for(uint8_t budget=32;budget && serialLink.available();--budget){
    int received=serialLink.read();
    if(received<0){commandLength=0;break;}
    char ch=char(received);
    if(ch=='\r')continue;
    if(ch=='\n'){
      command[commandLength]=0;
      if(commandLength)handleCommand(command,now);
      commandLength=0;
    }else if(commandLength<sizeof(command)-1)command[commandLength++]=ch;
    else commandLength=0;
  }
  if(txAt==txLength){txAt=txLength=0;prepareReply();}
  if(txAt<txLength){
    uint8_t count=uint8_t(txLength-txAt);if(count>32)count=32;
    uint8_t sent=serialLink.writeSome(tx+txAt,count);
    txAt=uint8_t(txAt+sent);
    if(sent)serialProgressAt=now;
    else if(uint32_t(now-serialProgressAt)>=1000)abortReply(); // No partial END marker.
  }
}
void setup(){
  Serial.begin(115200);
  pinMode(START_BUTTON,INPUT_PULLUP);pinMode(EXERCISE_BUTTON,INPUT_PULLUP);
  lcd.begin(16,2);lcd.clear();setLine(0,"PART A A2.0");setLine(1,"Starting...");
  Wire.begin();
#if defined(ARDUINO_ARCH_RENESAS_UNO) || defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(5000,true); // A sensor fault must not wait indefinitely.
#endif
  pulseSensorReady=pulseSensor.begin(Wire,I2C_SPEED_STANDARD);
  if(pulseSensorReady){
    pulseSensor.setup();pulseSensor.setPulseAmplitudeRed(0x1F);
    pulseSensor.setPulseAmplitudeIR(0x1F);pulseSensor.setPulseAmplitudeGreen(0);
  }
  uint32_t now=millis();resetAttempt(now);calibrationAt=now;
  startKey.suppress(digitalRead(START_BUTTON)==LOW,now);
  exerciseKey.suppress(digitalRead(EXERCISE_BUTTON)==LOW,now);
  renderDisplay(true);
}
void loop(){
  uint32_t now=millis();State previous=session.state;
  session.tick(now);
  if(previous!=session.state)resetDetectors();
  serviceButtons(now);
  serviceAccelerometer(millis());
  if(!calibrating)servicePulse(millis());
  renderDisplay();
  serviceSerial(millis());
}
