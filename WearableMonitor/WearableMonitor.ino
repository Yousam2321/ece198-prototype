/* ECE198 Group 23 - wearable prototype, UNO R4 Minima.
 * Direct wiring: MMA7361 X/Y/Z=A0/A1/A2, optional selector wiper=A3,
 * MMA7361 3V3 + Sleep -> 3.3V; GND + GS + ST -> GND; 5V/0G open.
 * bare button D2-to-GND (INPUT_PULLUP). USB power/data. No shield.
 * Read BUILD_GUIDE before connecting the optional MAX30102 breakout.
 */
#include <Arduino.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "PrototypeCore.h"
#include "Config.h"
#include "ExerciseProfiles.h"
#if ENABLE_MAX30102
#include <Wire.h>
#include <MAX30105.h>
#include <heartRate.h>
MAX30105 optical;
#endif
using namespace ece198;

const uint8_t BUTTON_PIN = 2;
const uint8_t AXIS_PINS[3] = {A0,A1,A2};
const uint8_t SELECT_PIN = A3;
Session session;
Button button;
CycleCounter cycle;
uint8_t profile = 0, axis = 0;
uint32_t reps = 0, lastSample = 0, lastLog = 0, teachAt = 0;
uint32_t warmupMs = REAL_WARMUP_MS, cardioMs = REAL_CARDIO_MS;
uint32_t baselineAt = 0, lastBaselineSample = 0;
float filtered[3] = {}, prevFiltered[3] = {}, teachMin[3], teachMax[3];
float lowEnd = 0, highEnd = 1, signalValue = 0, motionDelta = 0;
float hr = 0, hrBaseline = 0, baselineSum = 0, manualDistance = 0;
uint16_t baselineCount = 0;
uint16_t lastSelector = 0;
int selectorCandidate = -1;
uint32_t selectorAt = 0;
bool sampleReady = false, calibrated = false, teaching = false;
bool demoMode = false, cardioAnnounced = false, baselineRecording = false;
bool hrReady = false, opticalFound = false, fingerPresent = false;
uint16_t hrGoal = 0; // User-supplied demonstration goal; no default prescription.
char commandLine[64];
uint8_t commandLength = 0;
#if ENABLE_MAX30102
uint32_t lastBeat = 0, lastGoodBeat = 0, lastContact = 0;
float bpmWindow[4] = {};
uint8_t bpmCount = 0, bpmIndex = 0;
const uint32_t CONTACT_IR_MIN = 50000;
#endif

// Label how repetitions were obtained so manual confirmations never look automatic.
const char* sourceName() {
  if (!EXERCISES[profile].repCandidate) return "timer_manual";
  return calibrated ? "accelerometer_candidate" : "manual_no_calibration";
}
// Conservative display status; these heuristics do not validate physiological accuracy.
const char* hrStatus() {
#if !ENABLE_MAX30102
  return "DISABLED";
#else
  if (!opticalFound) return "NOT_FOUND";
  if (!fingerPresent) return "NO_CONTACT";
  if (motionDelta > 6.0f) return "MOTION";
  return hrReady ? "CANDIDATE" : "ACQUIRING";
#endif
}
// Print one CSV row. A blank HR field means unavailable, not a measured zero BPM.
void emit(const char* type, const char* note) {
  Serial.print(type); Serial.print(F(",wearable,")); Serial.print(millis());
  Serial.print(','); Serial.print(EXERCISES[profile].name);
  Serial.print(','); Serial.print(stateName(session.state));
  Serial.print(','); Serial.print(session.active);
  Serial.print(','); Serial.print(reps);
  Serial.print(','); Serial.print(sourceName());
  Serial.print(','); Serial.print(signalValue,3);
  Serial.print(',');
  if (hrReady && motionDelta <= 6.0f) Serial.print(hr,1);
  Serial.print(','); Serial.print(hrStatus());
  Serial.print(','); if (hrBaseline > 0) Serial.print(hrBaseline,1);
  Serial.print(','); Serial.print(manualDistance,1);
  Serial.print(','); Serial.print(demoMode ? "DEMO" : "REAL_TIME");
  Serial.print(','); Serial.println(note);
}
void resetSession() {
  session.reset(millis()); cycle.reset(); reps = 0; manualDistance = 0;
  cardioAnnounced = false; teaching = false; baselineRecording = false;
  emit("EVENT","reset");
}
// Selection is allowed only while IDLE. A new exercise invalidates sensor calibration.
void chooseProfile(uint8_t next) {
  if (session.state != IDLE || next >= PROFILE_COUNT || next == profile) return;
  profile = next; calibrated = false; teaching = false;
  reps = 0; signalValue = 0; manualDistance = 0; cycle.reset();
  emit("EVENT","profile_selected");
}
// Learn only the signal range for the selected placement, not a medical form model.
void startTeaching() {
  if (session.state != IDLE || !EXERCISES[profile].repCandidate) {
    emit("EVENT","teach_requires_idle_rep_profile"); return;
  }
  teaching = true; calibrated = false; teachAt = millis();
  for (uint8_t i=0;i<3;i++) { teachMin[i] = 1023; teachMax[i] = 0; }
  emit("EVENT","teach_8s_repeat_3_cycles_now");
}
// Start/pause/resume; discard partial cycles so a pause cannot complete a rep.
void go() {
  if (teaching) { emit("EVENT","wait_for_teach"); return; }
  RunState before = session.state;
  session.go(millis()); cycle.reset();
  if (before == DONE) { resetSession(); return; }
  emit("EVENT","button_or_go");
}
void finishSession() {
  session.finish(millis()); cycle.reset(); teaching = false;
  emit("SUMMARY","finished");
}
// Sample analog MMA7361 outputs at about 50 Hz; low-pass filter to reduce noise.
// ADC values are normalized against a taught range, not reported as degrees or force.
void readAcceleration(uint32_t now) {
  if (uint32_t(now-lastSample)<20) return;
  lastSample = now; motionDelta = 0;
  for (uint8_t i=0;i<3;i++) {
    analogRead(AXIS_PINS[i]); // Discard after channel change; analog outputs have finite impedance.
    delayMicroseconds(50);
    float raw = analogRead(AXIS_PINS[i]);
    prevFiltered[i] = filtered[i];
    filtered[i] = sampleReady ? filtered[i]+0.20f*(raw-filtered[i]) : raw;
    motionDelta += fabsf(filtered[i]-prevFiltered[i]);
    if (teaching) {
      if (filtered[i]<teachMin[i]) teachMin[i]=filtered[i];
      if (filtered[i]>teachMax[i]) teachMax[i]=filtered[i];
    }
  }
  sampleReady = true;
  if (teaching && uint32_t(now-teachAt)>=8000) {
    teaching = false; axis = 0;
    for (uint8_t i=1;i<3;i++)
      if (teachMax[i]-teachMin[i]>teachMax[axis]-teachMin[axis]) axis=i;
    lowEnd=teachMin[axis]; highEnd=teachMax[axis];
    calibrated = highEnd-lowEnd>=18 && highEnd-lowEnd<=620 && lowEnd>5 && highEnd<1018;
    cycle.reset();
    emit("EVENT",calibrated ? "teach_pass_candidate_only" : "teach_failed_use_manual");
    Serial.print(F("# axis=")); Serial.print(axis);
    Serial.print(F(" min=")); Serial.print(lowEnd);
    Serial.print(F(" max=")); Serial.println(highEnd);
  }
  signalValue = calibrated ? (filtered[axis]-lowEnd)/(highEnd-lowEnd) : 0;
  if (session.state==RUNNING && calibrated && EXERCISES[profile].repCandidate) {
    if (cycle.update(signalValue,now)) { ++reps; emit("EVENT","candidate_rep"); }
  }
}
// Poll the MAX30102 FIFO and estimate BPM from beat-to-beat timing.
// The library calls this device MAX30105, but supports MAX30102 too. No SpO2 here.
void readOptical(uint32_t now) {
#if ENABLE_MAX30102
  if (!opticalFound) return;
  optical.check();
  // Average 4 samples at 400Hz -> 100 samples/s, matching detector use.
  while (optical.available()) {
    uint8_t queued = optical.available();
    uint32_t sampleAt = now - uint32_t(queued-1)*10UL;
    uint32_t ir = optical.getFIFOIR(); optical.nextSample();
    bool detected = checkForBeat(int32_t(ir));
    fingerPresent = ir>=CONTACT_IR_MIN;
    if (!fingerPresent) {
      hrReady=false; bpmCount=0; bpmIndex=0; lastBeat=0;
      continue;
    }
    lastContact=now;
    if (detected) {
      if (lastBeat) {
        uint32_t interval=uint32_t(sampleAt-lastBeat);
        if (interval>=300 && interval<=1500) {
          bpmWindow[bpmIndex]=60000.0f/interval;
          bpmIndex=(bpmIndex+1)%4; if (bpmCount<4) ++bpmCount;
          lastGoodBeat=now;
          float sum=0, lo=1000, hi=0;
          for (uint8_t i=0;i<bpmCount;i++) {
            sum+=bpmWindow[i]; lo=fminf(lo,bpmWindow[i]); hi=fmaxf(hi,bpmWindow[i]);
          }
          hr=sum/bpmCount;
          hrReady=bpmCount==4 && hi-lo<=0.25f*hr;
        } else { bpmCount=0; bpmIndex=0; hrReady=false; }
      }
      lastBeat=sampleAt;
    }
  }
  if (uint32_t(now-lastContact)>500) { fingerPresent=false; hrReady=false; }
  if (uint32_t(now-lastGoodBeat)>2500) { hrReady=false; bpmCount=0; }
#else
  (void)now;
#endif
}
// Commands are newline-terminated text from Serial Monitor or capture_serial.py.
// Serial Monitor: 115200 baud, Newline. Commands are documented in docs/EXERCISES.md.
void handleCommand(char* line) {
  if (!strcmp(line,"go")) go();
  else if (!strcmp(line,"finish")) finishSession();
  else if (!strcmp(line,"reset") && session.state!=RUNNING) resetSession();
  else if (!strcmp(line,"teach")) startTeaching();
  else if (!strcmp(line,"rep") && session.state==RUNNING) {
    if (calibrated && EXERCISES[profile].repCandidate) emit("EVENT","manual_rep_requires_manual_mode");
    else { ++reps; emit("EVENT","manual_rep_confirmed"); }
  }
  else if (!strcmp(line,"manual") && session.state==IDLE) {
    calibrated=false; cycle.reset(); emit("EVENT","manual_mode");
  }
  else if (!strncmp(line,"select ",7)) {
    int n=atoi(line+7); if (n>=0 && n<PROFILE_COUNT) chooseProfile(uint8_t(n));
  }
  else if (!strncmp(line,"distance ",9)) {
    float d=atof(line+9);
    if (profile==6 && d>=0 && d<=10000) { manualDistance=d; emit("EVENT","distance_manual_metres"); }
  }
  else if (!strncmp(line,"goal ",5)) {
    int n=atoi(line+5); if (n==0 || (n>=40 && n<=200)) { hrGoal=n; emit("EVENT","hr_goal_user_supplied"); }
  }
  else if (!strcmp(line,"baseline") && session.state==IDLE) {
    baselineRecording=true; baselineAt=millis(); baselineSum=0; baselineCount=0;
    emit("EVENT","baseline_20s_keep_still");
  }
  else if ((!strcmp(line,"demo") || !strcmp(line,"real")) && session.state==IDLE) {
    demoMode=!strcmp(line,"demo");
    warmupMs=demoMode?DEMO_WARMUP_MS:REAL_WARMUP_MS; cardioMs=demoMode?DEMO_CARDIO_MS:REAL_CARDIO_MS;
    emit("EVENT",demoMode?"demo_timer_settings_selected":"real_timer_settings_selected");
  }
  else if (!strcmp(line,"help")) {
    Serial.println(F("# go finish reset teach manual rep select 0..8 distance METRES goal BPM baseline demo real"));
    for(uint8_t i=0;i<PROFILE_COUNT;i++){ Serial.print(F("# "));Serial.print(i);Serial.print(' ');Serial.println(EXERCISES[i].name); }
  }
  else emit("EVENT","command_not_available");
}
void readCommands() {
  while (Serial.available()) {
    char ch=Serial.read();
    if (ch=='\r') continue;
    if (ch=='\n') {
      commandLine[commandLength]='\0';
      if(commandLength) handleCommand(commandLine);
      commandLength=0;
    } else if (commandLength<sizeof(commandLine)-1) commandLine[commandLength++]=ch;
    else commandLength=0;
  }
}
// Runs once at power-up: configure inputs, optionally start HR, print CSV header.
void setup() {
  Serial.begin(115200);
  // Native USB serial must not prevent operation on a power bank.
  uint32_t waitAt=millis(); while(!Serial && uint32_t(millis()-waitAt)<1500) {}
  pinMode(BUTTON_PIN,INPUT_PULLUP); pinMode(LED_BUILTIN,OUTPUT);
#if defined(ARDUINO_ARCH_RENESAS_UNO)
  analogReadResolution(10); // Same ADC scale as code and calibration thresholds.
#endif
#if USE_SELECTOR_POT
  analogRead(SELECT_PIN); delayMicroseconds(100);
  lastSelector=analogRead(SELECT_PIN);
  profile=uint8_t((uint32_t(lastSelector)*PROFILE_COUNT)/1024);
#else
  profile=0; // Treadmill at boot; select another profile through Serial.
#endif
#if ENABLE_MAX30102
  Wire.begin();
  opticalFound=optical.begin(Wire,I2C_SPEED_STANDARD);
  if(opticalFound) {
    optical.setup(0x1F,4,2,400,411,4096);
    optical.setPulseAmplitudeRed(0); optical.setPulseAmplitudeIR(0x1F);
  }
#endif
  session.reset(millis());
  Serial.println(F("type,device,uptime_ms,exercise,state,active_ms,reps,source,normalized_signal,hr_bpm,hr_status,baseline_bpm,manual_distance_m,time_mode,note"));
  emit("EVENT","boot_type_help_for_commands");
}
// Runs repeatedly. Tick timers and poll inputs frequently; avoid long delay() calls.
// The three-second countdown and active-time clock live in PrototypeCore.h.
void loop() {
  uint32_t now=millis();
  RunState before=session.state; session.tick(now);
  if(before==COUNTDOWN && session.state==RUNNING) { cycle.reset(); emit("EVENT","started"); }
  readCommands();
  ButtonEvent event=button.update(digitalRead(BUTTON_PIN)==LOW,now);
  if(event==SHORT_PRESS) go();
  if(event==LONG_PRESS) {
    if(session.state==IDLE) startTeaching();
    else if(session.state==DONE) resetSession();
    else finishSession();
  }
#if USE_SELECTOR_POT
  if(session.state==IDLE && !teaching) {
    analogRead(SELECT_PIN); delayMicroseconds(100);
    uint32_t sum=0;
    for(uint8_t i=0;i<8;i++)sum+=analogRead(SELECT_PIN);
    uint16_t value=sum/8;
    if(abs(int(value)-int(lastSelector))>15) {
      int next=(uint32_t(value)*PROFILE_COUNT)/1024;
      if(next!=selectorCandidate) { selectorCandidate=next; selectorAt=now; }
      if(uint32_t(now-selectorAt)>350) { chooseProfile(uint8_t(next));lastSelector=value; }
    }
  }
#endif
  readAcceleration(now); readOptical(now);
  if(baselineRecording && uint32_t(now-lastBaselineSample)>=1000) {
    lastBaselineSample=now;
    if(hrReady && motionDelta<=6.0f) { baselineSum+=hr; ++baselineCount; }
    if(uint32_t(now-baselineAt)>=20000) {
      baselineRecording=false;
      if(baselineCount>=10) { hrBaseline=baselineSum/baselineCount; emit("EVENT","baseline_recorded_candidate"); }
      else emit("EVENT","baseline_failed_insufficient_signal");
    }
  }
  if(profile==0 && session.state==RUNNING) {
    if(!cardioAnnounced && session.active>=warmupMs) { cardioAnnounced=true;emit("EVENT","cardio_started"); }
    if(session.active>=warmupMs+cardioMs) finishSession();
  }
  bool flash=(now/150)%2;
  digitalWrite(LED_BUILTIN,(teaching || session.state==COUNTDOWN) ? flash : session.state==RUNNING);
  if(uint32_t(now-lastLog)>=1000) {
    lastLog=now;
    const char* note="status";
    if(profile==0 && session.state==RUNNING) note=cardioAnnounced?"cardio":"warmup";
    if(hrGoal && hrReady && motionDelta<=6.0f) note=hr>=hrGoal?"candidate_at_or_above_goal":"candidate_below_goal";
    emit("DATA",note);
  }
}
