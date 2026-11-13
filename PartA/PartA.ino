
#include <Wire.h>
#include <LiquidCrystal.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <EEPROM.h>
#include "PartASessions.h"
#include "PartASerial.h"
PartASerialOutput<decltype(Serial)> safeSerial(Serial);

// ================= LCD =================

LiquidCrystal lcd(7, 8, 9, 10, 11, 12);


// ================= BUTTONS =================

const int START_BUTTON = 2;
const int EXERCISE_BUTTON = 3;

const unsigned long DEBOUNCE_TIME = 50;
const unsigned long LONG_PRESS_TIME = 3000; // Finish/save: hold START for 3 seconds.

// Button 1
bool startRawState = HIGH;
bool startStableState = HIGH;
unsigned long startChangeTime = 0;
unsigned long startPressTime = 0;
bool longPressHandled = false;

// Button 2
bool exerciseRawState = HIGH;
bool exerciseStableState = HIGH;
unsigned long exerciseChangeTime = 0;
bool exerciseSwitchArmed = false;


// ================= EXERCISES =================

const char* exercises[] = {
  "Squats",
  "Lat Raises"
};

const int NUM_EXERCISES = 2;
int currentExercise = 0;

// Independent rep counters
int exerciseReps[NUM_EXERCISES] = {0, 0};


// ================= SESSION =================

bool exerciseRunning = false;
bool sessionEnded = false;


// ================= SQUAT DETECTION =================

// Existing squat thresholds - unchanged

const float SQUAT_DOWN_THRESHOLD = -40.0;
const float SQUAT_RETURN_THRESHOLD = -5.0;

const byte SQUAT_DOWN_CONFIRM_SAMPLES = 3;
const byte SQUAT_RETURN_CONFIRM_SAMPLES = 8;

const unsigned long SQUAT_MIN_INTERVAL = 1200;

bool squatDownDetected = false;

byte squatDownSamples = 0;
byte squatReturnSamples = 0;

unsigned long lastSquatRepTime = 0;


// ================= LAT RAISE DETECTION =================

// Y-axis thresholds based on your wrist data

const float LAT_RAISE_UP_THRESHOLD = 20.0;
const float LAT_RAISE_RETURN_THRESHOLD = 5.0;

const byte LAT_RAISE_UP_CONFIRM_SAMPLES = 3;
const byte LAT_RAISE_RETURN_CONFIRM_SAMPLES = 8;

const unsigned long LAT_RAISE_MIN_INTERVAL = 1200;

bool latRaiseDetected = false;

byte latRaiseUpSamples = 0;
byte latRaiseReturnSamples = 0;

unsigned long lastLatRaiseRepTime = 0;


// ================= ACCELEROMETER =================

const int X_PIN = A0;
const int Y_PIN = A1;
const int Z_PIN = A2;

const float GRAVITY_ALPHA = 0.05;

float gravityX = 0;
float gravityY = 0;
float gravityZ = 0;

float x = 0;
float y = 0;
float z = 0;

unsigned long lastAccelUpdate = 0;
const unsigned long ACCEL_DELAY = 25;


// ================= PULSE SENSOR =================

MAX30105 pulseSensor;
bool pulseSensorReady = false;

const long FINGER_THRESHOLD = 50000;

long irValue = 0;
long lastBeat = 0;

float bpm = 0;
float averageBPM = 0;

const byte RATE_SIZE = 5;
byte rates[RATE_SIZE];

byte rateSpot = 0;
byte validRates = 0;


// ================= LCD UPDATE =================

unsigned long lastLCDUpdate = 0;
const unsigned long LCD_DELAY = 250;


// ================= ADDED: HISTORY AND EXPORT =================
// Detection thresholds, confirmation samples and pin mapping are unchanged.
PartAArchive<EEPROMClass> sessionArchive(EEPROM);
uint32_t activeMs[NUM_EXERCISES]={0,0};
bool sessionTouched[NUM_EXERCISES]={false,false};
float sessionBpmSum[NUM_EXERCISES]={0,0};
uint16_t sessionBpmSamples[NUM_EXERCISES]={0,0};
uint32_t sessionClockAt=0;
char serialCommand[32];
uint8_t serialCommandLength=0;

// The existing buttons also open a protected history menu. Debounce this
// separately so a two-button hold cannot leak into the normal button handlers.
struct HistoryKey {
  bool raw=HIGH,stable=HIGH;
  uint32_t changedAt=0;
  bool update(bool reading,uint32_t now){
    if(reading!=raw){raw=reading;changedAt=now;}
    if(uint32_t(now-changedAt)>=DEBOUNCE_TIME && stable!=raw){
      stable=raw;return stable==HIGH; // An action requires a fresh release.
    }
    return false;
  }
};
enum HistoryMenu : uint8_t { HISTORY_NORMAL,HISTORY_CHORD,HISTORY_CONFIRM,HISTORY_RESULT };
HistoryMenu historyMenu=HISTORY_NORMAL;
HistoryKey historyStart,historyExercise;
bool historyEligible=false,historyHolding=false,historyArmed=false;
uint32_t historyHoldAt=0,historyResultAt=0;

void consumeHistoryButtons(bool start,bool exercise,uint32_t now){
  startRawState=start;startStableState=start;startChangeTime=now;
  exerciseRawState=exercise;exerciseStableState=exercise;exerciseChangeTime=now;
  longPressHandled=true;exerciseSwitchArmed=false;
}

bool handleHistoryButtons(){
  uint32_t now=millis();
  bool start=digitalRead(START_BUTTON),exercise=digitalRead(EXERCISE_BUTTON);
  bool startReleased=historyStart.update(start,now);
  bool exerciseReleased=historyExercise.update(exercise,now);
  bool both=start==LOW && exercise==LOW;
  bool released=start==HIGH && exercise==HIGH && historyStart.stable==HIGH && historyExercise.stable==HIGH;
  if(historyMenu==HISTORY_NORMAL){
    if(!both)return false;
    historyMenu=HISTORY_CHORD;historyHolding=false;
    historyEligible=!exerciseRunning;
    for(int i=0;i<NUM_EXERCISES;++i)if(sessionTouched[i])historyEligible=false;
  }
  // Consume all chord/menu transitions, including the final release. Neither
  // the opening hold nor simultaneous confirmation presses can erase history.
  consumeHistoryButtons(start,exercise,now);
  if(historyMenu==HISTORY_CHORD){
    if(both && historyStart.stable==LOW && historyExercise.stable==LOW){
      if(!historyHolding){historyHolding=true;historyHoldAt=now;}
      if(historyEligible && uint32_t(now-historyHoldAt)>=3000){
        historyMenu=HISTORY_CONFIRM;historyArmed=false;
        lcd.clear();lcd.setCursor(0,0);lcd.print(F("ERASE HISTORY?"));
        lcd.setCursor(0,1);lcd.print(F("START=YES EX=NO"));
        safeSerial.println(F("A_EVENT,part_a,erase_history_prompt"));
      }
    }else historyHolding=false;
    if(released)historyMenu=HISTORY_NORMAL;
    return true;
  }
  if(historyMenu==HISTORY_CONFIRM){
    if(!historyArmed){if(released)historyArmed=true;return true;}
    if(both){historyArmed=false;return true;}
    if((startReleased && exercise==HIGH && historyExercise.stable==HIGH) ||
       (exerciseReleased && start==HIGH && historyStart.stable==HIGH)){
      lcd.clear();lcd.setCursor(0,0);
      if(startReleased){
        bool erased=sessionArchive.clear();
        lcd.print(erased?F("HISTORY CLEARED"):F("ERASE FAILED"));
        safeSerial.println(erased?F("A_EVENT,part_a,history_cleared"):F("A_EVENT,part_a,erase_failed"));
      }else{
        lcd.print(F("ERASE CANCELLED"));safeSerial.println(F("A_EVENT,part_a,erase_cancelled"));
      }
      historyMenu=HISTORY_RESULT;historyResultAt=now;
    }
    return true;
  }
  // Keep confirmation visible, then return to the original LCD without turning
  // a held confirmation button into a new exercise/start action.
  if(released && uint32_t(now-historyResultAt)>=2000){
    historyMenu=HISTORY_NORMAL;lcd.clear();lastLCDUpdate=0;
  }
  return true;
}

void updateSessionClock(){
  uint32_t now=millis();
  if(exerciseRunning && !sessionEnded)activeMs[currentExercise]+=uint32_t(now-sessionClockAt);
  sessionClockAt=now;
}

void resetSessionStats(int exercise){
  activeMs[exercise]=0;sessionTouched[exercise]=false;
  sessionBpmSum[exercise]=0;sessionBpmSamples[exercise]=0;
}

void recordSessionBeat(float value){
  if(exerciseRunning && !sessionEnded && sessionBpmSamples[currentExercise]<65535){
    sessionBpmSum[currentExercise]+=value;++sessionBpmSamples[currentExercise];
  }
}

void printPartASaved(const PartASavedSession& r){
  safeSerial.print(F("ASAVED,"));safeSerial.print(r.id);safeSerial.print(',');
  safeSerial.print(r.exercise==0?F("squats"):F("lateral_raises"));safeSerial.print(',');
  safeSerial.print(r.activeMs);safeSerial.print(',');safeSerial.print(r.reps);safeSerial.print(',');
  if(r.bpmSamples)safeSerial.print(r.bpmTenths/10.0f,1);
  safeSerial.print(',');safeSerial.print(r.bpmSamples);safeSerial.print(',');safeSerial.println(r.historyId);
}

bool saveCurrentSession(){
  if(!sessionTouched[currentExercise])return true; // Never save an unused mode.
  PartASavedSession r={0,activeMs[currentExercise],0,uint16_t(exerciseReps[currentExercise]),
    sessionBpmSamples[currentExercise]?uint16_t(sessionBpmSum[currentExercise]/sessionBpmSamples[currentExercise]*10+.5f):uint16_t(0),
    sessionBpmSamples[currentExercise],uint8_t(currentExercise)};
  if(!sessionArchive.append(r)){safeSerial.println(F("A_EVENT,part_a,save_failed_counts_kept"));return false;}
  sessionTouched[currentExercise]=false;printPartASaved(r);
  return true;
}

void handleHistoryCommand(const char* command){
  if(!strcmp(command,"version")){safeSerial.println(F("# DEVICE,part_a,A1.2"));return;}
  if(!strcmp(command,"history")){
    if(exerciseRunning){safeSerial.println(F("A_EVENT,part_a,pause_before_history"));return;}
    safeSerial.resetError();
    safeSerial.println(F("# ASAVED,id,exercise,active_ms,reps,bpm_avg,bpm_samples,history_id"));
    uint32_t latest=sessionArchive.latestId();
    if(!latest){safeSerial.println(F("# No saved A sessions"));return;}
    uint32_t first=latest>20?latest-19:1;PartASavedSession r;
    for(uint32_t id=first;;++id){
      if(sessionArchive.readId(id,r))printPartASaved(r);
      if(safeSerial.failed())return; // Never mark a partial transfer complete.
      if(id==latest)break;
    }
    safeSerial.println(F("# END_A_HISTORY"));return;
  }
  if(!strcmp(command,"erase CONFIRM") && !exerciseRunning){
    safeSerial.println(sessionArchive.clear()?F("A_EVENT,part_a,history_cleared"):F("A_EVENT,part_a,erase_failed"));return;
  }
  safeSerial.println(F("A_EVENT,part_a,command_not_available"));
}

void readHistoryCommands(){
  while(Serial.available()){
    char ch=Serial.read();if(ch=='\r')continue;
    if(ch=='\n'){
      serialCommand[serialCommandLength]='\0';
      if(serialCommandLength)handleHistoryCommand(serialCommand);
      serialCommandLength=0;
    }else if(serialCommandLength<sizeof(serialCommand)-1)serialCommand[serialCommandLength++]=ch;
    else serialCommandLength=0;
  }
}

// ================= RESET SQUAT DETECTION =================

void resetSquatDetection() {

  squatDownDetected = false;

  squatDownSamples = 0;
  squatReturnSamples = 0;
}


// ================= RESET LAT RAISE DETECTION =================

void resetLatRaiseDetection() {

  latRaiseDetected = false;

  latRaiseUpSamples = 0;
  latRaiseReturnSamples = 0;
}


// ================= END SESSION =================

void endSession() {

  updateSessionClock();
  exerciseRunning = false;
  if(!saveCurrentSession())return; // Keep counters available if saving fails.
  sessionEnded = true;

  // Reset ALL exercise rep counts

  for (int i = 0; i < NUM_EXERCISES; i++) {

    exerciseReps[i] = 0;
    resetSessionStats(i);
  }

  lastSquatRepTime = 0;
  lastLatRaiseRepTime = 0;

  resetSquatDetection();
  resetLatRaiseDetection();

  safeSerial.println("SESSION ENDED");
  safeSerial.println("ALL REP COUNTERS RESET");
}


// ================= SQUAT DETECTION =================

void updateSquatCounter() {

  // Only count squats when Squats is selected
  // and the exercise is running

  if (currentExercise != 0 ||
      !exerciseRunning ||
      sessionEnded) {

    resetSquatDetection();
    return;
  }


  // Detect downward movement

  if (!squatDownDetected) {

    if (z <= SQUAT_DOWN_THRESHOLD) {

      if (squatDownSamples < SQUAT_DOWN_CONFIRM_SAMPLES) {
        squatDownSamples++;
      }

    } else {

      squatDownSamples = 0;
    }


    bool cooldownComplete =
      (lastSquatRepTime == 0) ||
      (millis() - lastSquatRepTime >= SQUAT_MIN_INTERVAL);


    // Confirm squat movement

    if (squatDownSamples >= SQUAT_DOWN_CONFIRM_SAMPLES &&
        cooldownComplete) {

      squatDownDetected = true;

      squatReturnSamples = 0;
      squatDownSamples = 0;
    }
  }


  // Detect return to standing

  else {

    if (z >= SQUAT_RETURN_THRESHOLD) {

      if (squatReturnSamples < SQUAT_RETURN_CONFIRM_SAMPLES) {
        squatReturnSamples++;
      }

    } else {

      squatReturnSamples = 0;
    }


    // Count successful squat

    if (squatReturnSamples >= SQUAT_RETURN_CONFIRM_SAMPLES) {

      exerciseReps[0]++;

      lastSquatRepTime = millis();

      squatDownDetected = false;

      squatReturnSamples = 0;
      squatDownSamples = 0;

      safeSerial.print("Successful squat reps: ");
      safeSerial.println(exerciseReps[0]);
    }
  }
}


// ================= LAT RAISE DETECTION =================

void updateLatRaiseCounter() {

  // Only count lat raises when Lat Raises is selected
  // and the exercise is running

  if (currentExercise != 1 ||
      !exerciseRunning ||
      sessionEnded) {

    resetLatRaiseDetection();
    return;
  }


  // Detect upward arm movement

  if (!latRaiseDetected) {

    if (y >= LAT_RAISE_UP_THRESHOLD) {

      if (latRaiseUpSamples < LAT_RAISE_UP_CONFIRM_SAMPLES) {
        latRaiseUpSamples++;
      }

    } else {

      latRaiseUpSamples = 0;
    }


    // Check cooldown

    bool cooldownComplete =
      (lastLatRaiseRepTime == 0) ||
      (millis() - lastLatRaiseRepTime >= LAT_RAISE_MIN_INTERVAL);


    // Confirm raised movement

    if (latRaiseUpSamples >= LAT_RAISE_UP_CONFIRM_SAMPLES &&
        cooldownComplete) {

      latRaiseDetected = true;

      latRaiseReturnSamples = 0;
      latRaiseUpSamples = 0;

      safeSerial.println("Lat raise movement detected");
    }
  }


  // Detect return to starting position

  else {

    if (y <= LAT_RAISE_RETURN_THRESHOLD) {

      if (latRaiseReturnSamples < LAT_RAISE_RETURN_CONFIRM_SAMPLES) {
        latRaiseReturnSamples++;
      }

    } else {

      latRaiseReturnSamples = 0;
    }


    // Count successful lat raise

    if (latRaiseReturnSamples >= LAT_RAISE_RETURN_CONFIRM_SAMPLES) {

      exerciseReps[1]++;

      lastLatRaiseRepTime = millis();

      latRaiseDetected = false;

      latRaiseReturnSamples = 0;
      latRaiseUpSamples = 0;

      safeSerial.print("Successful lat raise reps: ");
      safeSerial.println(exerciseReps[1]);
    }
  }
}


// ================= SETUP =================

void setup() {

  Serial.begin(115200);

  pinMode(START_BUTTON, INPUT_PULLUP);
  pinMode(EXERCISE_BUTTON, INPUT_PULLUP);

  lcd.begin(16, 2);
  lcd.clear();

  lcd.print("Starting...");

  Wire.begin();

  if (pulseSensor.begin(Wire, I2C_SPEED_STANDARD)) {

    pulseSensorReady = true;

    pulseSensor.setup();

    pulseSensor.setPulseAmplitudeRed(0x1F);
    pulseSensor.setPulseAmplitudeIR(0x1F);
    pulseSensor.setPulseAmplitudeGreen(0);

    safeSerial.println("MAX30102 connected!");

  } else {

    safeSerial.println("MAX30102 not detected!");
  }


  // Calibrate accelerometer

  lcd.clear();
  lcd.print("Calibrating...");

  const int samples = 200;

  for (int i = 0; i < samples; i++) {

    gravityX += analogRead(X_PIN);
    gravityY += analogRead(Y_PIN);
    gravityZ += analogRead(Z_PIN);

    delay(5);
  }

  gravityX /= samples;
  gravityY /= samples;
  gravityZ /= samples;


  // Initialize button states

  startRawState = digitalRead(START_BUTTON);
  startStableState = startRawState;

  exerciseRawState = digitalRead(EXERCISE_BUTTON);
  exerciseStableState = exerciseRawState;

  startChangeTime = millis();
  exerciseChangeTime = millis();

  lcd.clear();

  sessionClockAt=millis();
  safeSerial.println(F("# DEVICE,part_a,A1.2"));
  safeSerial.println("System ready!");
}


// ================= BUTTON 1: START / STOP / END =================

void handleStartButton() {

  unsigned long now = millis();

  bool reading = digitalRead(START_BUTTON);


  // Detect physical button changes

  if (reading != startRawState) {

    startRawState = reading;
    startChangeTime = now;
  }


  // Debounce button

  if (now - startChangeTime >= DEBOUNCE_TIME) {

    if (startStableState != startRawState) {

      startStableState = startRawState;


      // BUTTON PRESSED

      if (startStableState == LOW) {

        startPressTime = now;
        longPressHandled = false;

        safeSerial.println("Start button pressed");
      }


      // BUTTON RELEASED

      else {

        if (!longPressHandled) {

          // Start new session after END

          if (sessionEnded) {

            sessionEnded = false;
            updateSessionClock();
            exerciseRunning = true;
            sessionTouched[currentExercise]=true;

            lastSquatRepTime = 0;
            lastLatRaiseRepTime = 0;

            resetSquatDetection();
            resetLatRaiseDetection();

            safeSerial.println("NEW SESSION STARTED");
          }


          // Toggle between RUNNING and STOPPED

          else {

            updateSessionClock();
            exerciseRunning = !exerciseRunning;
            if(exerciseRunning)sessionTouched[currentExercise]=true;

            lastSquatRepTime = 0;
            lastLatRaiseRepTime = 0;

            resetSquatDetection();
            resetLatRaiseDetection();


            if (exerciseRunning) {

              safeSerial.println("EXERCISE STARTED");

            } else {

              safeSerial.println("EXERCISE STOPPED");

              safeSerial.print("Saved reps: ");
              safeSerial.println(exerciseReps[currentExercise]);
            }
          }
        }
      }
    }
  }


  // Detect LONG PRESS

  if (startStableState == LOW &&
      !longPressHandled &&
      now - startPressTime >= LONG_PRESS_TIME) {

    longPressHandled = true;

    if (!sessionEnded) {

      endSession();
    }
  }
}


// ================= BUTTON 2: SWITCH EXERCISE =================

void handleExerciseButton() {

  unsigned long now = millis();

  bool reading = digitalRead(EXERCISE_BUTTON);


  // Detect physical button changes

  if (reading != exerciseRawState) {

    exerciseRawState = reading;
    exerciseChangeTime = now;
  }


  // Debounce button

  if (now - exerciseChangeTime >= DEBOUNCE_TIME) {

    if (exerciseStableState != exerciseRawState) {

      exerciseStableState = exerciseRawState;


      // Switch on release so a two-button erase hold cannot switch first.
      if(exerciseStableState==LOW)exerciseSwitchArmed=true;

      if (exerciseStableState == HIGH && exerciseSwitchArmed) {
        exerciseSwitchArmed=false;

        // Do not allow switching while running

        if (!exerciseRunning) {

          updateSessionClock();
          if(!saveCurrentSession())return;
          int previousExercise = currentExercise;
          resetSessionStats(previousExercise);


          // Reset exercise being switched FROM

          exerciseReps[previousExercise] = 0;


          // Switch to next exercise

          currentExercise++;

          if (currentExercise >= NUM_EXERCISES) {

            currentExercise = 0;
          }


          // Reset detection states

          lastSquatRepTime = 0;
          lastLatRaiseRepTime = 0;

          resetSquatDetection();
          resetLatRaiseDetection();


          safeSerial.print("Exercise changed to: ");
          safeSerial.println(exercises[currentExercise]);

          safeSerial.print("Previous exercise rep count reset: ");
          safeSerial.println(exercises[previousExercise]);
        }

        else {

          safeSerial.println("Cannot switch exercises while running");
          safeSerial.println("Press Start button to stop first");
        }
      }
    }
  }
}


// ================= ACCELEROMETER =================

void updateAccelerometer() {

  if (millis() - lastAccelUpdate >= ACCEL_DELAY) {

    lastAccelUpdate = millis();

    float rawX = analogRead(X_PIN);
    float rawY = analogRead(Y_PIN);
    float rawZ = analogRead(Z_PIN);


    // Track gravity/baseline component

    gravityX += GRAVITY_ALPHA * (rawX - gravityX);
    gravityY += GRAVITY_ALPHA * (rawY - gravityY);
    gravityZ += GRAVITY_ALPHA * (rawZ - gravityZ);


    // Calculate movement relative to baseline

    x = rawX - gravityX;
    y = rawY - gravityY;
    z = rawZ - gravityZ;


    // Update both rep counters
    // Only the currently selected exercise can count

    updateSquatCounter();
    updateLatRaiseCounter();


    // Keep Serial Monitor X, Y, Z readings

    if(safeSerial.canWrite(48)){ // Optional telemetry must not slow sampling.
    safeSerial.print("X:");
    safeSerial.print(x);

    safeSerial.print(" Y:");
    safeSerial.print(y);

    safeSerial.print(" Z:");
    safeSerial.println(z);
    }
  }
}


// ================= PULSE SENSOR =================

void updatePulseSensor() {

  if (!pulseSensorReady) {
    return;
  }

  irValue = pulseSensor.getIR();

  if (irValue < FINGER_THRESHOLD) {

    bpm = 0;
    averageBPM = 0;

    lastBeat = 0;
    validRates = 0;
    rateSpot = 0;

    return;
  }

  if (checkForBeat(irValue)) {

    long currentTime = millis();

    if (lastBeat != 0) {

      long delta = currentTime - lastBeat;

      float newBPM = 60000.0 / delta;

      if (newBPM >= 40 && newBPM <= 180) {

        bpm = newBPM;
        recordSessionBeat(newBPM);

        rates[rateSpot] = (byte)bpm;

        rateSpot = (rateSpot + 1) % RATE_SIZE;

        if (validRates < RATE_SIZE) {
          validRates++;
        }

        int total = 0;

        for (byte i = 0; i < validRates; i++) {
          total += rates[i];
        }

        averageBPM = (float)total / validRates;
      }
    }

    lastBeat = currentTime;
  }
}


// ================= LCD =================

void updateLCD() {

  if (millis() - lastLCDUpdate < LCD_DELAY) {
    return;
  }

  lastLCDUpdate = millis();


  // TOP ROW: Exercise name

  lcd.setCursor(0, 0);
  lcd.print("                ");

  lcd.setCursor(0, 0);
  lcd.print(exercises[currentExercise]);


  // STOP means exercise is running
  // RUN means exercise is stopped or ended

  lcd.setCursor(12, 0);
  lcd.print("    ");

  lcd.setCursor(12, 0);

  if (exerciseRunning) {

    lcd.print("STOP");

  } else {

    lcd.print("RUN");
  }


  // BOTTOM ROW: Current exercise rep counter

  lcd.setCursor(0, 1);
  lcd.print("           ");

  lcd.setCursor(0, 1);

  if (currentExercise == 0) {

    lcd.print("Reps: ");
    lcd.print(exerciseReps[0]);

  } else {

    lcd.print("Reps: ");
    lcd.print(exerciseReps[1]);
  }


  // Keep space before BPM

  lcd.setCursor(11, 1);
  lcd.print("  ");


  // BPM stays at columns 13-15

  lcd.setCursor(13, 1);

  if (!pulseSensorReady || irValue < FINGER_THRESHOLD || bpm == 0) {

    lcd.print("---");

  } else {

    char bpmText[4];

    snprintf(
      bpmText,
      sizeof(bpmText),
      "%3d",
      (int)averageBPM
    );

    lcd.print(bpmText);
  }
}


// ================= MAIN LOOP =================

void loop() {

  updateSessionClock();
  readHistoryCommands();
  if(!handleHistoryButtons()){
    handleStartButton();
    handleExerciseButton();
  }

  updateAccelerometer();

  updatePulseSensor();

  if(historyMenu==HISTORY_NORMAL)updateLCD();
}
