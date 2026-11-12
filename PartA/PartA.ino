
#include <Wire.h>
#include <LiquidCrystal.h>
#include "MAX30105.h"
#include "heartRate.h"

// ================= LCD =================

LiquidCrystal lcd(7, 8, 9, 10, 11, 12);


// ================= BUTTONS =================

const int START_BUTTON = 2;
const int EXERCISE_BUTTON = 3;

const unsigned long DEBOUNCE_TIME = 50;
const unsigned long LONG_PRESS_TIME = 1000;

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

  exerciseRunning = false;
  sessionEnded = true;

  // Reset ALL exercise rep counts

  for (int i = 0; i < NUM_EXERCISES; i++) {

    exerciseReps[i] = 0;
  }

  lastSquatRepTime = 0;
  lastLatRaiseRepTime = 0;

  resetSquatDetection();
  resetLatRaiseDetection();

  Serial.println("SESSION ENDED");
  Serial.println("ALL REP COUNTERS RESET");
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

      Serial.print("Successful squat reps: ");
      Serial.println(exerciseReps[0]);
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

      Serial.println("Lat raise movement detected");
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

      Serial.print("Successful lat raise reps: ");
      Serial.println(exerciseReps[1]);
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

    Serial.println("MAX30102 connected!");

  } else {

    Serial.println("MAX30102 not detected!");
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

  Serial.println("System ready!");
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

        Serial.println("Start button pressed");
      }


      // BUTTON RELEASED

      else {

        if (!longPressHandled) {

          // Start new session after END

          if (sessionEnded) {

            sessionEnded = false;
            exerciseRunning = true;

            lastSquatRepTime = 0;
            lastLatRaiseRepTime = 0;

            resetSquatDetection();
            resetLatRaiseDetection();

            Serial.println("NEW SESSION STARTED");
          }


          // Toggle between RUNNING and STOPPED

          else {

            exerciseRunning = !exerciseRunning;

            lastSquatRepTime = 0;
            lastLatRaiseRepTime = 0;

            resetSquatDetection();
            resetLatRaiseDetection();


            if (exerciseRunning) {

              Serial.println("EXERCISE STARTED");

            } else {

              Serial.println("EXERCISE STOPPED");

              Serial.print("Saved reps: ");
              Serial.println(exerciseReps[currentExercise]);
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


      // Switch once when button is pressed

      if (exerciseStableState == LOW) {

        // Do not allow switching while running

        if (!exerciseRunning) {

          int previousExercise = currentExercise;


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


          Serial.print("Exercise changed to: ");
          Serial.println(exercises[currentExercise]);

          Serial.print("Previous exercise rep count reset: ");
          Serial.println(exercises[previousExercise]);
        }

        else {

          Serial.println("Cannot switch exercises while running");
          Serial.println("Press Start button to stop first");
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

    Serial.print("X:");
    Serial.print(x);

    Serial.print(" Y:");
    Serial.print(y);

    Serial.print(" Z:");
    Serial.println(z);
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

  handleStartButton();

  handleExerciseButton();

  updateAccelerometer();

  updatePulseSensor();

  updateLCD();
}
