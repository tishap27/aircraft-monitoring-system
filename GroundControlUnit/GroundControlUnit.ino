// ============================================================================
// MEGA-2: Ground Control Unit [STEPPER + FAN + RFID PLANE DETECTION] MEGA 2
// ============================================================================

#include <LiquidCrystal.h>
#include <Wire.h>
#include <DHT.h>
#include <Servo.h>
#include <SPI.h>
#include <MFRC522.h>

// ===== PIN DEFINITIONS =====
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

#define DHT_PIN 22
#define DHT_TYPE DHT11
const int PIR_PIN = 18;
const int ULTRASONIC_TRIG = 24;
const int ULTRASONIC_ECHO = 26;
const int PHOTORESISTOR_PIN = A0;

// RFID RC522 Module (SAME AS YOUR TEST)
#define RST_PIN 49
#define SS_PIN 53
MFRC522 rfid(SS_PIN, RST_PIN);

// Motor (L293D) - FAN CONTROL ON OBJECT DETECTION
const int MOTOR_IN1 = 8;
const int MOTOR_IN2 = 9;
const int MOTOR_ENA = 10; 

// SERVO MOTOR (Gate/Barrier)
const int SERVO_GATE_PIN = 44;
Servo gateServo;
const int GATE_OPEN_ANGLE = 0;
const int GATE_CLOSED_ANGLE = 90;
bool gateIsClosed = false;

// Stepper Motor Pins (ULN2003)
const int STEPPER_PIN1 = 31;
const int STEPPER_PIN2 = 33;
const int STEPPER_PIN3 = 35;
const int STEPPER_PIN4 = 37;

// ========== ROTARY ENCODER ==========
const int ENCODER_CLK = 45;
const int ENCODER_DT = 43;
const int ENCODER_SW = 41;

int lastCLK = HIGH;
int currentAirspeed = 140;  // Default landing airspeed in knots
const int TARGET_AIRSPEED = 100;  // Target to resolve conflict
bool conflictActive = false;
bool conflictResolved = false;


// Passive Buzzer
const int PASSIVE_BUZZER_PIN = 7;
//LCD 
bool showingStartupMessage = false;
unsigned long startupMessageTime = 0;
// LEDs
const int YELLOW_LED_1 = 28;
const int YELLOW_LED_2 = 30;
const int YELLOW_LED_3 = 32;
const int YELLOW_LED_4 = 34;
const int RED_LED_1 = 36;
const int RED_LED_2 = 38;
const int RED_LED_3 = 40;
const int RED_LED_4 = 42;

int yellowLEDs[] = {YELLOW_LED_1, YELLOW_LED_2, YELLOW_LED_3, YELLOW_LED_4};
int redLEDs[]    = {RED_LED_1, RED_LED_2, RED_LED_3, RED_LED_4};

DHT dht(DHT_PIN, DHT_TYPE);

// ===== SYSTEM VARIABLES =====
float preferredTemp = 23.5;
float currentTemp = 0.0;
float currentHumidity = 0.0;

bool tempReceived = false;
bool systemStarted = false;

bool motionDetected = false;
bool lastPirState = LOW;
int motionCount = 0;
unsigned long lastMotionTime = 0;

bool motorRunning = false;
unsigned long motorStartTime = 0;
unsigned long motorTotalTime = 0;

bool objectDetected = false;
bool planeDetected = false;  // NEW: RFID plane detection
unsigned long lastSafetyCheck = 0;
int detectionCount = 0;
int planeConflictCount = 0;  // NEW: Plane conflict counter

bool nightMode = false;

unsigned long lastDisplayUpdate = 0;
unsigned long lastSerialUpdate = 0;

// Stepper Motor Variables
int stepperSpeed = 5;
bool stepperRunning = false;
int stepperPosition = 0;

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(9600);
  Wire.begin(8);
  Wire.onReceive(receiveFromMega1);

  // Initialize RFID (SAME AS YOUR TEST)
  SPI.begin();
  rfid.PCD_Init();
  Serial.println("✓ RFID RC522 initialized");
  
  lcd.begin(16, 2);
  dht.begin();

  pinMode(PIR_PIN, INPUT);
  pinMode(ULTRASONIC_TRIG, OUTPUT);
  pinMode(ULTRASONIC_ECHO, INPUT);
  pinMode(PHOTORESISTOR_PIN, INPUT);

  // Initialize Motor Pins for FAN control
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_ENA, OUTPUT);

  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_ENA, 0);

  // Initialize Servo Motor (Gate)
  gateServo.attach(SERVO_GATE_PIN);
  gateServo.write(GATE_OPEN_ANGLE);
  gateIsClosed = false;
  Serial.println("Gate Servo initialized - OPEN position");

  // Initialize Stepper Motor Pins
  pinMode(STEPPER_PIN1, OUTPUT);
  pinMode(STEPPER_PIN2, OUTPUT);
  pinMode(STEPPER_PIN3, OUTPUT);
  pinMode(STEPPER_PIN4, OUTPUT);

  // Initialize Rotary Encoder
  pinMode(ENCODER_CLK, INPUT);
  pinMode(ENCODER_DT, INPUT);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  Serial.println("Rotary Encoder initialized");
  
  disableStepperMotor();

  pinMode(PASSIVE_BUZZER_PIN, OUTPUT);

  for (int i = 0; i < 4; i++) {
    pinMode(yellowLEDs[i], OUTPUT);
    pinMode(redLEDs[i], OUTPUT);
  }

  lcd.clear();
  lcd.print("MEGA-2 + RFID");
  lcd.setCursor(0, 1);
  lcd.print("Ready!");
  delay(1500);

  lcd.clear();
  lcd.print("Waiting for");
  lcd.setCursor(0,1);
  lcd.print("MEGA-1...");
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  checkNightMode();
  checkMotion();
  checkRotaryEncoder();
  readEnvironmentalData();
  
   // Only run main systems if MEGA-1 has started
  if (systemStarted) {
   checkUltrasonicAndControlGate();
    controlStepperContinuous();
  }
  //checkUltrasonicAndControlGate();  // Check ultrasonic + RFID and control gate + fan
  
  // Stepper Motor Control - CONTINUOUS ROTATION
  //controlStepperContinuous();

  if (millis() - lastDisplayUpdate > 1000) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }

  if (millis() - lastSerialUpdate > 3000) {
    sendStatus();
    lastSerialUpdate = millis();
  }
}

// ============================================================================
// RFID DETECTION FUNCTION
// ============================================================================
bool checkRFIDTag() {
  // Check if a new card is present
  if (!rfid.PICC_IsNewCardPresent()) {
    return false;
  }
  
  // Try to read the card
  if (!rfid.PICC_ReadCardSerial()) {
    return false;
  }
  
  // Card detected! Print UID
  Serial.print("✈ RFID PLANE TAG DETECTED! UID: ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) Serial.print("0");
    Serial.print(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) Serial.print(":");
  }
  Serial.println();
  
  // Halt PICC and stop encryption
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  
  return true;  // RFID plane tag detected
}

// ============================================================================
// SERVO GATE CONTROL FUNCTIONS
// ============================================================================

void openGate() {
  if (!gateIsClosed) return;  // Already open
  
  Serial.println(">>> OPENING GATE <<<");
  lcd.clear();
  lcd.print("Opening Gate...");
  
  // Smoothly open gate
  for (int angle = GATE_CLOSED_ANGLE; angle >= GATE_OPEN_ANGLE; angle -= 2) {
    gateServo.write(angle);
    delay(15);
  }
  
  gateIsClosed = false;
  controlYellowLEDs(false);
  noTone(PASSIVE_BUZZER_PIN);
  
  // STOP FAN when gate opens
  stopFan();
  
  lcd.setCursor(0, 1);
  lcd.print("Gate: OPEN");
  delay(500);
}

void closeGate() {
  if (gateIsClosed) return;  // Already closed
  
  Serial.println(">>> CLOSING GATE <<<");
  lcd.clear();
  lcd.print("OBJECT DETECTED!");
  lcd.setCursor(0, 1);
  lcd.print("Closing Gate...");
  
  // Play warning sound
  playWarning();
  
  // Flash yellow LEDs
  controlYellowLEDs(true);
  
  // Smoothly close gate
  for (int angle = GATE_OPEN_ANGLE; angle <= GATE_CLOSED_ANGLE; angle += 2) {
    gateServo.write(angle);
    delay(15);
  }
  
  gateIsClosed = true;
  detectionCount++;
  
  // START FAN when gate closes
  startFan();
  
  delay(500);
}

// ============================================================================
// FAN CONTROL FUNCTIONS
// ============================================================================

void startFan() {
  if (motorRunning) return;
  
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_ENA, 255);
  
  motorRunning = true;
  motorStartTime = millis();
  
  Serial.println(">>> FAN STARTED <<<");
}

void stopFan() {
  if (!motorRunning) return;
  
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_ENA, 0);
  
  motorRunning = false;
  motorTotalTime = millis() - motorStartTime;
  
  Serial.println(">>> FAN STOPPED <<<");
}

// ============================================================================
// PLANE CONFLICT ALARM (URGENT!)
// ============================================================================
void playPlaneConflictAlarm() {
  // Urgent rapid beeping for plane conflict
  for (int i = 0; i < 8; i++) {
    tone(PASSIVE_BUZZER_PIN, 1500, 100);
    delay(150);
  }
  noTone(PASSIVE_BUZZER_PIN);
}
// ============================================================================
// ROTARY ENCODER - AIRSPEED CONTROL
// ============================================================================
void checkRotaryEncoder() {
  if (!conflictActive) return;  // Only work during conflict
  
  int currentCLK = digitalRead(ENCODER_CLK);
  
  // Check if CLK state changed (rotation detected)
  if (currentCLK != lastCLK && currentCLK == LOW) {
    int dtValue = digitalRead(ENCODER_DT);
    
    // Clockwise rotation = DECREASE airspeed
    if (dtValue == HIGH) {
      currentAirspeed -= 5;  // Decrease by 5 knots
      if (currentAirspeed < 80) currentAirspeed = 80;  // Minimum speed
      
      Serial.print("Airspeed DECREASED to: ");
      Serial.print(currentAirspeed);
      Serial.println(" knots");
      
      tone(PASSIVE_BUZZER_PIN, 400, 50);  // Low beep
    }
    // Counter-clockwise = INCREASE airspeed
    else {
      currentAirspeed += 5;  // Increase by 5 knots
      if (currentAirspeed > 160) currentAirspeed = 160;  // Maximum speed
      
      Serial.print("Airspeed INCREASED to: ");
      Serial.print(currentAirspeed);
      Serial.println(" knots");
      
      tone(PASSIVE_BUZZER_PIN, 600, 50);  // Higher beep
    }
    
    // Check if conflict resolved
    if (currentAirspeed <= TARGET_AIRSPEED && !conflictResolved) {
      resolveConflict();
    }
  }
  
  lastCLK = currentCLK;
  
  // Check encoder button press (reset/confirm)
  if (digitalRead(ENCODER_SW) == LOW) {
    delay(50);  // Debounce
    if (digitalRead(ENCODER_SW) == LOW) {
      Serial.println("Encoder button pressed - Airspeed confirmed");
      tone(PASSIVE_BUZZER_PIN, 1000, 200);
      delay(300);
    }
  }
}

void resolveConflict() {
  conflictResolved = true;
  conflictActive = false;
  
  Serial.println("\n✓✓✓ CONFLICT RESOLVED! ✓✓✓");
  Serial.print("Final Airspeed: ");
  Serial.print(currentAirspeed);
  Serial.println(" knots");
  
  // Success beeps
  for (int i = 0; i < 3; i++) {
    tone(PASSIVE_BUZZER_PIN, 1200, 100);
    delay(150);
  }
  
  // Turn off yellow LEDs
  controlYellowLEDs(false);
  
  // Show success on LCD
  lcd.clear();
  lcd.print("CONFLICT CLEAR!");
  lcd.setCursor(0, 1);
  lcd.print("Speed: ");
  lcd.print(currentAirspeed);
  lcd.print(" kts");
  delay(3000);
}
// ============================================================================
// ULTRASONIC + RFID DETECTION + GATE + FAN CONTROL
// ============================================================================
// ============================================================================
// ULTRASONIC + RFID DETECTION + GATE + FAN CONTROL (FIXED)
// ============================================================================
void checkUltrasonicAndControlGate() {
  if (millis() - lastSafetyCheck > 200) {
    lastSafetyCheck = millis();
    
    // ===== CHECK RFID INDEPENDENTLY (NOT tied to ultrasonic) =====
    bool newRFIDDetected = checkRFIDTag();
    bool previousPlaneState = planeDetected;
    
    if (newRFIDDetected && !previousPlaneState) {
      planeDetected = true;  // PLANE DETECTED via RFID!
      handlePlaneConflict();
      return;  // Handle plane conflict immediately
    }
    
    // ===== NOW CHECK ULTRASONIC for regular objects =====
    long distance = getUltrasonicDistance();
    bool previousObjectState = objectDetected;
    
    // Object detected within 20cm (but NOT a plane via RFID)
    objectDetected = (distance < 20 && distance > 2);
    
    if (objectDetected && !planeDetected) {
      // Regular object (no RFID tag)
      if (!previousObjectState) {
        closeGate();
        lcd.clear();
        lcd.print("GATE CLOSED!");
        lcd.setCursor(0,1);
        lcd.print("Dist: ");
        lcd.print(distance);
        lcd.print("cm");
        delay(1000);
      }
    }
    
    // Object cleared
    else if (!objectDetected && (previousObjectState || previousPlaneState)) {
      planeDetected = false;
      delay(1000);
      openGate();
      controlYellowLEDs(false);
      lcd.clear();
      lcd.print("Path Clear");
      lcd.setCursor(0,1);
      lcd.print("Gate: OPEN");
      delay(1000);
    }
  }
}

// ===== NEW FUNCTION: Handle plane conflict =====
void handlePlaneConflict() {
  planeConflictCount++;
  conflictActive = true;
  conflictResolved = false;
  currentAirspeed = 140;
  
  Serial.println("\n🛩 PLANE CONFLICT!!!!");
  Serial.println("Another plane incoming!");
  Serial.print("Current Airspeed: ");
  Serial.print(currentAirspeed);
  Serial.println(" knots");
  Serial.print("Target Airspeed: ");
  Serial.print(TARGET_AIRSPEED);
  Serial.println(" knots or below");
  
  playPlaneConflictAlarm();
  
  // Flash ALL LEDs rapidly
  for (int i = 0; i < 5; i++) {
    controlYellowLEDs(true);
    controlRedLEDs(true);
    delay(100);
    controlYellowLEDs(false);
    controlRedLEDs(false);
    delay(100);
  }
  controlYellowLEDs(true);
  
  lcd.clear();
  lcd.print("CONFLICT DETECT!");
  lcd.setCursor(0, 1);
  lcd.print("Speed: ");
  lcd.print(currentAirspeed);
  lcd.print(" kts!");
  delay(2000);
  
  lcd.clear();
  lcd.print("REDUCE AIRSPEED");
  lcd.setCursor(0, 1);
  lcd.print("Target: ");
  lcd.print(TARGET_AIRSPEED);
  lcd.print(" kts");
  delay(2000);
  
  closeGate();
  startFan();
}

// ============================================================================
// STEPPER MOTOR FUNCTIONS
// ============================================================================

void disableStepperMotor() {
  digitalWrite(STEPPER_PIN1, LOW);
  digitalWrite(STEPPER_PIN2, LOW);
  digitalWrite(STEPPER_PIN3, LOW);
  digitalWrite(STEPPER_PIN4, LOW);
}

void stepperMotor(int steps, bool clockwise) {
  if (stepperRunning) return;
  
  stepperRunning = true;

  int step_sequence[8][4] = {
    {1, 0, 0, 0},
    {1, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
    {0, 0, 0, 1},
    {1, 0, 0, 1}
  };

  for (int i = 0; i < steps; i++) {
    int step = clockwise ? (i % 8) : (7 - (i % 8));
    
    digitalWrite(STEPPER_PIN1, step_sequence[step][0]);
    digitalWrite(STEPPER_PIN2, step_sequence[step][1]);
    digitalWrite(STEPPER_PIN3, step_sequence[step][2]);
    digitalWrite(STEPPER_PIN4, step_sequence[step][3]);

    if (clockwise) {
      stepperPosition += 1;
      if (stepperPosition > 1024) stepperPosition = 0;
    } else {
      stepperPosition -= 1;
      if (stepperPosition < 0) stepperPosition = 1024;
    }

    delay(stepperSpeed);
  }

  disableStepperMotor();
  stepperRunning = false;
}

void controlStepperContinuous() {
  static unsigned long lastRotation = 0;

  if (stepperRunning) return;

  // STEPPER CONTINUOUSLY ROTATES - SCANS ALL ANGLES (every 1.5 seconds)
  if (millis() - lastRotation > 1500) {
    stepperMotor(256, true);
    lastRotation = millis();
  }
}

// ============================================================================
// RECEIVE FROM MEGA-1 (PREFERRED TEMP)
// ============================================================================
void receiveFromMega1(int bytes) {
  Serial.println("\n>>> I2C DATA RECEIVED! <<<");
  Serial.print("Bytes: ");
  Serial.println(bytes);
  
  if (bytes >= 1) {
    byte startSignal = Wire.read();
    Serial.print("Signal: ");
    Serial.println(startSignal);
    
    if (startSignal == 1) {
      Serial.println("✓ START SIGNAL CONFIRMED!");
      
      if (bytes == 5) {
        Wire.readBytes((char*)&preferredTemp, sizeof(float));
        Serial.print("✓ Temp: ");
        Serial.print(preferredTemp, 1);
        Serial.println("°C");
      }
     
      tempReceived = true;
      systemStarted = true;

      // SET FLAG TO SHOW STARTUP MESSAGE
      showingStartupMessage = true;
      startupMessageTime = millis();

      lcd.clear();
      lcd.print("MEGA-1 Started!");
      lcd.setCursor(0,1);
      lcd.print("Temp ");
      lcd.print(preferredTemp, 1);
      lcd.print("C");
      
      tone(PASSIVE_BUZZER_PIN, 1000, 200);
      delay(300);
      tone(PASSIVE_BUZZER_PIN, 1200, 200);
      delay(300);
      
      for (int i = 0; i < 3; i++) {
        controlYellowLEDs(true);
        delay(100);
        controlYellowLEDs(false);
        delay(100);
      }
      
      Serial.println(" MEGA-2 ACTIVATED!");
    }
  }
}

// ============================================================================
// NIGHT MODE
// ============================================================================
void checkNightMode() {
  int lightLevel = analogRead(PHOTORESISTOR_PIN);
  bool previousNight = nightMode;

  nightMode = (lightLevel < 100);

  if (nightMode != previousNight) {

    if (nightMode) {
      Serial.println("\n*** NIGHT MODE ACTIVATED ***");
      preferredTemp = 22.0;

      controlRedLEDs(true);
      if (!objectDetected && !planeDetected) controlYellowLEDs(false);

      lcd.clear();
      lcd.print("Night Mode");
      lcd.setCursor(0,1);
      lcd.print("Temp:22.0C");
      delay(2000);

    } else {
      Serial.println("\n☀ *** DAY MODE ACTIVATED ***");
      controlRedLEDs(false);
    }
  }
}

// ============================================================================
// MOTION DETECTION
// ============================================================================
void checkMotion() {
  bool pir = digitalRead(PIR_PIN);

  if (pir == HIGH && lastPirState == LOW) handleMotionStart();
  if (pir == LOW  && lastPirState == HIGH) motionDetected = false;

  if (pir == HIGH) lastMotionTime = millis();
  lastPirState = pir;
}

void handleMotionStart() {
  motionDetected = true;
  motionCount++;
  lastMotionTime = millis();

  if (!nightMode && !objectDetected && !planeDetected) controlYellowLEDs(true);

  lcd.clear();
  lcd.print("*** MOTION ***");
  lcd.setCursor(0,1);
  lcd.print("T:");
  lcd.print(currentTemp,1);
  lcd.print(" H:");
  lcd.print(currentHumidity,0);
  lcd.print("%");

  delay(1200);
}

// ============================================================================
// READ DHT11
// ============================================================================
void readEnvironmentalData() {
  currentHumidity = dht.readHumidity();
  currentTemp     = dht.readTemperature();

  if (isnan(currentTemp) || isnan(currentHumidity)) {
    currentTemp = 25.0;
    currentHumidity = 50.0;
  }
}

// ============================================================================
// DISTANCE FUNCTION
// ============================================================================
long getUltrasonicDistance() {
  digitalWrite(ULTRASONIC_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG, LOW);

  long duration = pulseIn(ULTRASONIC_ECHO, HIGH, 30000);
  if (duration == 0) return 999;

  return (duration * 0.034) / 2;
}

// ============================================================================
// WARNING MELODY
// ============================================================================
void playWarning() {
  int melody[] = {1000, 800, 1000, 800, 1000};

  for (int i = 0; i < 5; i++) {
    tone(PASSIVE_BUZZER_PIN, melody[i], 200);
    delay(250);
  }
  noTone(PASSIVE_BUZZER_PIN);
}

// ============================================================================
// LED CONTROL
// ============================================================================
void controlYellowLEDs(bool state) {
  for (int i = 0; i < 4; i++) {
    digitalWrite(yellowLEDs[i], state ? HIGH : LOW);
  }
}

void controlRedLEDs(bool state) {
  for (int i = 0; i < 4; i++) {
    digitalWrite(redLEDs[i], state ? HIGH : LOW);
  }
}

// ============================================================================
// LCD DISPLAY
// ============================================================================
void updateDisplay() {

  // IF SHOWING STARTUP MESSAGE, KEEP IT FOR 3 SECONDS
  if (showingStartupMessage) {
    if (millis() - startupMessageTime < 3000) {
      return;  // Don't update display yet
    } else {
      showingStartupMessage = false;  // Time's up, resume normal display
    }
  }

  if (motionDetected && millis() - lastMotionTime < 1500)
    return;

  lcd.clear();
  lcd.setCursor(0,0);

   // Show airspeed during active conflict
  if (conflictActive) {
    lcd.print("AIRSPEED:");
    lcd.print(currentAirspeed);
    lcd.print("kts");
    lcd.setCursor(0,1);
    lcd.print("Target:");
    lcd.print(TARGET_AIRSPEED);
    lcd.print(" ROTATE!");
    return;
  }
  
  lcd.print("T:");
  lcd.print(currentTemp,1);
  lcd.print("C H:");
  lcd.print(currentHumidity,0);
  lcd.print("%");

  if (nightMode) {
    lcd.setCursor(15,0);
    lcd.print("N");
  }

  lcd.setCursor(0,1);

  if (planeDetected) {
    lcd.print("PLANE! CNF:");
    lcd.print(planeConflictCount);
  }
  else if (objectDetected) {
    lcd.print("Gate: CLOSED #");
    lcd.print(detectionCount);
  }
  else if (stepperRunning) {
    lcd.print("Stepper:");
    lcd.print(stepperPosition/4);
  }
  else {
    lcd.print("Gate:OPEN M:");
    lcd.print(motionCount);
  }
}

// ============================================================================
// SERIAL STATUS
// ============================================================================
void sendStatus() {

  Serial.println("\n═══════════════════════════════════");
  Serial.println("         MEGA-2 STATUS REPORT");
  Serial.println("═══════════════════════════════════");

  Serial.print("Temperature: ");
  Serial.print(currentTemp,1);
  Serial.print("°C  (Preferred: ");
  Serial.print(preferredTemp,1);
  Serial.println("°C)");

  Serial.print("Humidity: ");
  Serial.print(currentHumidity,0);
  Serial.println("%");

  Serial.print("Motion Count: ");
  Serial.println(motionCount);

  Serial.print("Gate Status: ");
  Serial.println(gateIsClosed ? "CLOSED" : "OPEN");

  Serial.print("Object Detections: ");
  Serial.println(detectionCount);

  Serial.print("Plane Conflicts: ");
  Serial.println(planeConflictCount);

  Serial.print("Fan Status: ");
  Serial.println(motorRunning ? "RUNNING" : "STOPPED");

  Serial.print("Stepper: ");
  Serial.print(stepperRunning ? "MOVING" : "IDLE");
  Serial.print(" (Position: ");
  Serial.print(stepperPosition/4);
  Serial.println("/256)");

  Serial.print("Night Mode: ");
  Serial.println(nightMode ? "Active" : "Inactive");

  if (planeDetected) Serial.println("⚠⚠⚠ PLANE CONFLICT DETECTED!");
  else if (objectDetected) Serial.println("⚠ Object detected - Gate CLOSED, Fan RUNNING");

  Serial.print("Uptime: ");
  Serial.print(millis()/1000);
  Serial.println("s");

  Serial.print("\nJSON:{\"temp\":");
  Serial.print(currentTemp,1);
  Serial.print(",\"humidity\":");
  Serial.print(currentHumidity,0);
  Serial.print(",\"gate\":\"");
  Serial.print(gateIsClosed ? "CLOSED" : "OPEN");
  Serial.print("\",\"fan\":\"");
  Serial.print(motorRunning ? "RUNNING" : "STOPPED");
  Serial.print("\",\"detections\":");
  Serial.print(detectionCount);
  Serial.print(",\"plane_conflicts\":");
  Serial.print(planeConflictCount);
  Serial.print(",\"plane_detected\":");
  Serial.print(planeDetected ? "true" : "false");
  Serial.print(",\"stepper\":");
  Serial.print(stepperPosition/4);
  Serial.print(",\"night\":");
  Serial.print(nightMode ? "true" : "false");
  Serial.print(",\"motion\":");
  Serial.print(motionCount);
  Serial.print(",\"preferred\":");
  Serial.print(preferredTemp,1);
  Serial.println("}");
}