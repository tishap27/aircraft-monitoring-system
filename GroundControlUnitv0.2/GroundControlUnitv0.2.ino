// ============================================================================
// MEGA-2: Ground Control Unit + DUAL MAX7219 RUNWAY LIGHTS (SYNCHRONIZED)
// ============================================================================

#include <LiquidCrystal.h>
#include <Wire.h>
#include <DHT.h>
#include <Servo.h>
#include <SPI.h>
#include <MFRC522.h>
#include <MD_MAX72xx.h>

// ===== PIN DEFINITIONS =====
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// MAX7219 Runway Lights (Two modules for extended runway)
#define MAX7219_CLK_PIN  47
#define MAX7219_CS_PIN   46
#define MAX7219_DIN_PIN  39

#define MAX2_DIN_PIN  25
#define MAX2_CS_PIN   27
#define MAX2_CLK_PIN  29

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 1

MD_MAX72XX runwayLights = MD_MAX72XX(HARDWARE_TYPE, MAX7219_DIN_PIN, MAX7219_CLK_PIN, MAX7219_CS_PIN, MAX_DEVICES);
MD_MAX72XX runwayLights2 = MD_MAX72XX(HARDWARE_TYPE, MAX2_DIN_PIN, MAX2_CLK_PIN, MAX2_CS_PIN, MAX_DEVICES);

#define DHT_PIN 22
#define DHT_TYPE DHT11
const int PIR_PIN = 18;
const int ULTRASONIC_TRIG = 24;
const int ULTRASONIC_ECHO = 26;
const int PHOTORESISTOR_PIN = A0;

// ========== ULTRASONIC SENSOR ==========
unsigned long lastUltrasonicCheck = 0;
float smoothedDistance = 999.0;
const float DISTANCE_FILTER_ALPHA = 0.3;
const int OBJECT_THRESHOLD = 50;   // adjust as needed

// RFID RC522 Module
#define RST_PIN 49
#define SS_PIN 53
MFRC522 rfid(SS_PIN, RST_PIN);

// Motor (L293D) - FAN CONTROL
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

// Rotary Encoder
const int ENCODER_CLK = 45;
const int ENCODER_DT = 43;
const int ENCODER_SW = 41;

int lastCLK = HIGH;
int currentAirspeed = 140;
const int TARGET_AIRSPEED = 100;
bool conflictActive = false;
bool conflictResolved = false;

// Passive Buzzer
const int PASSIVE_BUZZER_PIN = 7;

// LCD 
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
bool planeDetected = false;
unsigned long lastSafetyCheck = 0;
int detectionCount = 0;
int planeConflictCount = 0;

bool nightMode = false;

unsigned long lastDisplayUpdate = 0;
unsigned long lastSerialUpdate = 0;
unsigned long lastRunwayUpdate = 0;

// Stepper Motor Variables
int stepperSpeed = 5;
bool stepperRunning = false;
int stepperPosition = 0;

const int STEPPER_MAX_POSITION = 512;  //  - Half rotation (180°)
const int STEPPER_MIN_POSITION = 0;    // 
bool stepperDirection = true;  

// Runway Light Animation Variables
int runwayAnimFrame = 0;
int runwayBrightness = 8;

// Function declarations (add these before setup())
void updateRunwayLights();
void runwayStartupAnimation();
void runwayClearPattern();
void runwayWarningPattern();
void runwayBlockedPattern();
void runwayConflictPattern();
void runwayClosed();
void setRunwayBrightness(int brightness);
// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(9600);
  Wire.begin(8);
  Wire.onReceive(receiveFromMega1);

  // Initialize BOTH MAX7219 Runway Lights
  Serial.println("Initializing MAX7219 Runway Lights (Module 1)...");
  runwayLights.begin();
  runwayLights.control(MD_MAX72XX::INTENSITY, runwayBrightness);
  runwayLights.clear();
  Serial.println("Module 1 Ready!");
  
  Serial.println("Initializing MAX7219 Runway Lights (Module 2)...");
  runwayLights2.begin();
  runwayLights2.control(MD_MAX72XX::INTENSITY, runwayBrightness);
  runwayLights2.clear();
  Serial.println("Module 2 Ready!");
  
  // Synchronized startup animation
  runwayStartupAnimation();
  
  Serial.println("DUAL MAX7219 Runway System Ready!");

  // Initialize RFID
  SPI.begin();
  rfid.PCD_Init();
  Serial.println("RFID RC522 initialized");
  
  lcd.begin(16, 2);
  dht.begin();

  pinMode(PIR_PIN, INPUT);
  pinMode(ULTRASONIC_TRIG, OUTPUT);
  pinMode(ULTRASONIC_ECHO, INPUT);
  pinMode(PHOTORESISTOR_PIN, INPUT);

  // Initialize Motor Pins
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_ENA, OUTPUT);
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_ENA, 0);

  // Initialize Servo
  gateServo.attach(SERVO_GATE_PIN);
  gateServo.write(GATE_OPEN_ANGLE);
  gateIsClosed = false;

  // Initialize Stepper
  pinMode(STEPPER_PIN1, OUTPUT);
  pinMode(STEPPER_PIN2, OUTPUT);
  pinMode(STEPPER_PIN3, OUTPUT);
  pinMode(STEPPER_PIN4, OUTPUT);
  disableStepperMotor();

  // Initialize Encoder
  pinMode(ENCODER_CLK, INPUT);
  pinMode(ENCODER_DT, INPUT);
  pinMode(ENCODER_SW, INPUT_PULLUP);

  pinMode(PASSIVE_BUZZER_PIN, OUTPUT);

  for (int i = 0; i < 4; i++) {
    pinMode(yellowLEDs[i], OUTPUT);
    pinMode(redLEDs[i], OUTPUT);
  }

  lcd.clear();
  lcd.print("MEGA-2 DUAL");
  lcd.setCursor(0, 1);
  lcd.print("Runway Ready!");
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
  
  if (systemStarted) {
    checkRFIDPlane();
    checkUltrasonicAndControlGate();
    controlStepperContinuous();
  }

  // Update BOTH runway lights with synchronized patterns
  updateRunwayLights();

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
// MAX7219 DUAL RUNWAY LIGHT FUNCTIONS (SYNCHRONIZED)
// ============================================================================

void runwayStartupAnimation() {
  // Sweep from left to right
  // Synchronized sweep from left to right on BOTH modules
  for (int col = 0; col < 8; col++) {
    runwayLights.clear();
    runwayLights2.clear();
    for (int row = 0; row < 8; row++) {
      runwayLights.setPoint(row, col, true);
      runwayLights2.setPoint(row, col, true);
    }
    delay(80);
  }

  // Flash all
  // Synchronized flash all on BOTH modules
  for (int i = 0; i < 3; i++) {
    runwayLights.clear();
    runwayLights2.clear();
    delay(100);
    for (int row = 0; row < 8; row++) {
      for (int col = 0; col < 8; col++) {
        runwayLights.setPoint(row, col, true);
        runwayLights2.setPoint(row, col, true);
      }
    }
    delay(100);
  }
  runwayLights.clear();
  runwayLights2.clear();
}

void updateRunwayLights() {
  static unsigned long updateInterval = 150; // Add this missing variable
  
  if (millis() - lastRunwayUpdate < updateInterval) return;
  lastRunwayUpdate = millis();

  // Choose pattern based on state - BOTH modules show same pattern
  if (conflictActive) {
    runwayConflictPattern();
  } else if (planeDetected) {
    runwayWarningPattern();
  } else if (objectDetected) {
    runwayBlockedPattern();
  } else {
    runwayClearPattern();
  }

  runwayAnimFrame++;
  if (runwayAnimFrame > 7) runwayAnimFrame = 0;
}
// RUNWAY PATTERN: Clear for landing (smooth scroll)
// RUNWAY PATTERN: Clear for landing (smooth scroll) - SYNCHRONIZED
void runwayClearPattern() {
  runwayLights.clear();
  runwayLights2.clear();

  // Create scrolling centerline lights
  int offset = runwayAnimFrame;

  // Center column lights (runway centerline)
  // Apply to BOTH modules simultaneously
  for (int row = 0; row < 8; row++) {
    // Center column lights (runway centerline)
    if ((row + offset) % 2 == 0) {
      runwayLights.setPoint(row, 3, true);
      runwayLights.setPoint(row, 4, true);
      runwayLights2.setPoint(row, 3, true);
      runwayLights2.setPoint(row, 4, true);
    }
  }
  
  // Edge lights (runway edges)
  for (int row = 0; row < 8; row++) {
    
    // Edge lights (runway edges)
    runwayLights.setPoint(row, 0, true);
    runwayLights.setPoint(row, 7, true);
    runwayLights2.setPoint(row, 0, true);
    runwayLights2.setPoint(row, 7, true);
  }
}

// RUNWAY PATTERN: Warning (alternating sides)
// RUNWAY PATTERN: Warning (alternating sides) - SYNCHRONIZED
void runwayWarningPattern() {
  runwayLights.clear();
  runwayLights2.clear();
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       
  // Alternating left-right warning
  // Alternating left-right warning on BOTH modules
  if (runwayAnimFrame % 2 == 0) {
    // Light up left side
    for (int row = 0; row < 8; row++) {
      for (int col = 0; col < 4; col++) {
        runwayLights.setPoint(row, col, true);
        runwayLights2.setPoint(row, col, true);
      }
    }
  } else {
    // Light up right side
    for (int row = 0; row < 8; row++) {
      for (int col = 4; col < 8; col++) {
        runwayLights.setPoint(row, col, true);
        runwayLights2.setPoint(row, col, true);
      }
    }
  }
}

// RUNWAY PATTERN: Blocked (X pattern)
// RUNWAY PATTERN: Blocked (X pattern) - SYNCHRONIZED
void runwayBlockedPattern() {
  runwayLights.clear();
  runwayLights2.clear();

  if (runwayAnimFrame % 2 == 0) {
    // Draw X pattern
    // Draw X pattern on BOTH modules
    for (int i = 0; i < 8; i++) {
      runwayLights.setPoint(i, i, true);         // Top-left to bottom-right
      runwayLights.setPoint(i, 7 - i, true);     // Top-right to bottom-left
      runwayLights.setPoint(i, i, true);
      runwayLights.setPoint(i, 7 - i, true);
      runwayLights2.setPoint(i, i, true);
      runwayLights2.setPoint(i, 7 - i, true);
    }
  }
}

// RUNWAY PATTERN: Conflict (rapid flash all)
// RUNWAY PATTERN: Conflict (rapid flash all) - SYNCHRONIZED
void runwayConflictPattern() {
  if (runwayAnimFrame % 2 == 0) {
    // All lights ON
    // All lights ON on BOTH modules
    for (int row = 0; row < 8; row++) {
      for (int col = 0; col < 8; col++) {
        runwayLights.setPoint(row, col, true);
        runwayLights2.setPoint(row, col, true);
      }
    }
  } else {
    // All lights OFF (creates urgent flashing)
    runwayLights.clear();
    runwayLights2.clear();
  }
}

// RUNWAY PATTERN: Closed (horizontal bars)
// RUNWAY PATTERN: Closed (horizontal bars) - SYNCHRONIZED
void runwayClosed() {
  runwayLights.clear();
  runwayLights2.clear();

  // Static horizontal bars (closed runway symbol)
  // Static horizontal bars (closed runway symbol) on BOTH modules
  for (int col = 0; col < 8; col++) {
    runwayLights.setPoint(1, col, true);
    runwayLights.setPoint(3, col, true);
    runwayLights.setPoint(5, col, true);
    runwayLights.setPoint(7, col, true);
    runwayLights2.setPoint(1, col, true);
    runwayLights2.setPoint(3, col, true);
    runwayLights2.setPoint(5, col, true);
    runwayLights2.setPoint(7, col, true);
  }
}

// Adjust runway light brightness (call during night mode)
// Adjust runway light brightness on BOTH modules
void setRunwayBrightness(int brightness) {
  runwayBrightness = constrain(brightness, 0, 15);
  runwayLights.control(MD_MAX72XX::INTENSITY, runwayBrightness);
  runwayLights2.control(MD_MAX72XX::INTENSITY, runwayBrightness);
}

// ============================================================================
// RFID DETECTION
// ============================================================================
bool checkRFIDTag() {
  if (!rfid.PICC_IsNewCardPresent()) return false;
  if (!rfid.PICC_ReadCardSerial()) return false;
  
  Serial.print("RFID PLANE TAG DETECTED! UID: ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) Serial.print("0");
    Serial.print(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) Serial.print(":");
  }
  Serial.println();
  
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  return true;
}

void checkRFIDPlane() {
  bool newRFIDDetected = checkRFIDTag();
  
  if (newRFIDDetected && !conflictActive) {
    planeDetected = true;
    handlePlaneConflict();
  }
  
  if (conflictResolved && planeDetected) {
    planeDetected = false;
    conflictResolved = false;
  }
}
// ============================================================================
// SERVO GATE CONTROL
// ============================================================================
void openGate() {
  if (!gateIsClosed) return;
  
  Serial.println("OPENING GATE");
  lcd.clear();
  lcd.print("Opening Gate...");
  
  for (int angle = GATE_CLOSED_ANGLE; angle >= GATE_OPEN_ANGLE; angle -= 2) {
    gateServo.write(angle);
    delay(15);
  }
  
  gateIsClosed = false;
  controlYellowLEDs(false);
  noTone(PASSIVE_BUZZER_PIN);
  
  // Stop fan when gate opens (path is clear)
  if (motorRunning) {
    stopFan();
  }
  
  lcd.setCursor(0, 1);
  lcd.print("Gate: OPEN");
  delay(500);
}


void closeGate() {
  if (gateIsClosed) return;
  
  Serial.println("CLOSING GATE");
  lcd.clear();
  lcd.print("OBJECT DETECTED!");
  lcd.setCursor(0, 1);
  lcd.print("Closing Gate...");
  
  playWarning();
  controlYellowLEDs(true);
  
  for (int angle = GATE_OPEN_ANGLE; angle <= GATE_CLOSED_ANGLE; angle += 2) {
    gateServo.write(angle);
    delay(15);
  }
  
  gateIsClosed = true;
  detectionCount++;
  
  // Start fan when gate closes (alert mode)
  if (!motorRunning) {
    startFan();
  }
  
  delay(500);
}


// ============================================================================
// FAN CONTROL
// ============================================================================
void startFan() {
  if (motorRunning) {
    Serial.println("FAN ALREADY RUNNING");
    return;
  }
  
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_ENA, 255);
  motorRunning = true;
  motorStartTime = millis();
  Serial.println("FAN STARTED");
}

void stopFan() {
  if (!motorRunning) {
    Serial.println("FAN ALREADY STOPPED");
    return;
  }
  
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_ENA, 0);
  motorRunning = false;
  motorTotalTime = millis() - motorStartTime;
  Serial.println("FAN STOPPED");
}

// ============================================================================
// PLANE CONFLICT HANDLING
// ============================================================================
void playPlaneConflictAlarm() {
  for (int i = 0; i < 8; i++) {
    tone(PASSIVE_BUZZER_PIN, 1500, 100);
    delay(150);
  }
  noTone(PASSIVE_BUZZER_PIN);
}

void checkRotaryEncoder() {
  if (!conflictActive) return;
  
  int currentCLK = digitalRead(ENCODER_CLK);
  
  if (currentCLK != lastCLK && currentCLK == LOW) {
    int dtValue = digitalRead(ENCODER_DT);
    
    if (dtValue == HIGH) {
      currentAirspeed -= 5;
      if (currentAirspeed < 80) currentAirspeed = 80;
      Serial.print("Airspeed DECREASED to: ");
      Serial.print(currentAirspeed);
      Serial.println(" knots");
      tone(PASSIVE_BUZZER_PIN, 400, 50);
    } else {
      currentAirspeed += 5;
      if (currentAirspeed > 160) currentAirspeed = 160;
      Serial.print("Airspeed INCREASED to: ");
      Serial.print(currentAirspeed);
      Serial.println(" knots");
      tone(PASSIVE_BUZZER_PIN, 600, 50);
    }
    
    if (currentAirspeed <= TARGET_AIRSPEED && !conflictResolved) {
      resolveConflict();
    }
  }
  
  lastCLK = currentCLK;
  
  if (digitalRead(ENCODER_SW) == LOW) {
    delay(50);
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
  planeDetected = false;  // ADD THIS LINE to reset plane detection
  
  Serial.println("\nCONFLICT RESOLVED!");
  Serial.print("Final Airspeed: ");
  Serial.print(currentAirspeed);
  Serial.println(" knots");
  
  for (int i = 0; i < 3; i++) {
    tone(PASSIVE_BUZZER_PIN, 1200, 100);
    delay(150);
  }
  
  controlYellowLEDs(false);
  
  lcd.clear();
  lcd.print("CONFLICT CLEAR!");
  lcd.setCursor(0, 1);
  lcd.print("Speed: ");
  lcd.print(currentAirspeed);
  lcd.print(" kts");
  delay(3000);
}
// ============================================================================
// ULTRASONIC + RFID DETECTION
// ============================================================================
void checkUltrasonicAndControlGate() {
  if (millis() - lastUltrasonicCheck < 200) {  // Check every 200ms
    return;
  }
  lastUltrasonicCheck = millis();
  
  // Get distance reading
  float distance = getUltrasonicDistance();
  
  // Simple: Object detected? Close gate. No object? Open gate.
  if (distance <= OBJECT_THRESHOLD && distance > 2) {
    // OBJECT DETECTED - Close gate immediately
    if (!gateIsClosed) {
      Serial.print("OBJECT DETECTED at ");
      Serial.print(distance, 1);
      Serial.println(" cm - CLOSING GATE");
      
      closeGate();
      
      lcd.clear();
      lcd.print("OBJECT DETECTED!");
      lcd.setCursor(0,1);
      lcd.print("Dist: ");
      lcd.print(distance, 1);
      lcd.print("cm");
    }
  } else {
    // NO OBJECT - Open gate immediately
    if (gateIsClosed && !planeDetected) {  // Don't open if plane conflict active
      Serial.println("PATH CLEAR - OPENING GATE");
      
      openGate();
      
      lcd.clear();
      lcd.print("Path Clear");
      lcd.setCursor(0,1);
      lcd.print("Gate: OPEN");
    }
  }
  
  // Debug output every second
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 1000) {
    Serial.print("Dist: ");
    Serial.print(distance, 1);
    Serial.print("cm | Gate: ");
    Serial.print(gateIsClosed ? "CLOSED" : "OPEN");
    Serial.print(" | Fan: ");
    Serial.println(motorRunning ? "ON" : "OFF");
    lastDebug = millis();
  }
}

void handlePlaneConflict() {
  planeConflictCount++;
  conflictActive = true;
  conflictResolved = false;
  currentAirspeed = 140;
  
  Serial.println("\nPLANE CONFLICT!");
  Serial.println("Another plane incoming!");
  Serial.print("Current Airspeed: ");
  Serial.print(currentAirspeed);
  Serial.println(" knots");
  Serial.print("Target Airspeed: ");
  Serial.print(TARGET_AIRSPEED);
  Serial.println(" knots or below");
  
  playPlaneConflictAlarm();
  
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
// STEPPER MOTOR
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
    {1, 0, 0, 0}, {1, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 1, 0},
    {0, 0, 1, 0}, {0, 0, 1, 1}, {0, 0, 0, 1}, {1, 0, 0, 1}
  };

  for (int i = 0; i < steps; i++) {
    int step = clockwise ? (i % 8) : (7 - (i % 8));
    
    digitalWrite(STEPPER_PIN1, step_sequence[step][0]);
    digitalWrite(STEPPER_PIN2, step_sequence[step][1]);
    digitalWrite(STEPPER_PIN3, step_sequence[step][2]);
    digitalWrite(STEPPER_PIN4, step_sequence[step][3]);

    // Update position tracking
    if (clockwise) {
      stepperPosition++;
      if (stepperPosition > STEPPER_MAX_POSITION) stepperPosition = STEPPER_MAX_POSITION;
    } else {
      stepperPosition--;
      if (stepperPosition < STEPPER_MIN_POSITION) stepperPosition = STEPPER_MIN_POSITION;
    }
    
    delay(stepperSpeed);
  }

  disableStepperMotor();
  stepperRunning = false;
  
  Serial.print("Stepper position: ");
  Serial.print(stepperPosition);
  Serial.print("/512 (");
  Serial.print((stepperPosition * 180) / 512);
  Serial.println(" degrees)");
}


void controlStepperContinuous() {
  static unsigned long lastRotation = 0;
  if (stepperRunning) return;
  
  if (millis() - lastRotation > 1500) {
    // Check current position and decide direction
    if (stepperPosition >= STEPPER_MAX_POSITION) {
      // At right limit, go left
      stepperDirection = false;
      Serial.println("Stepper: Moving LEFT (180°)");
    } 
    else if (stepperPosition <= STEPPER_MIN_POSITION) {
      // At left limit, go right
      stepperDirection = true;
      Serial.println("Stepper: Moving RIGHT (180°)");
    }
    
    // Move 512 steps (180 degrees) in current direction
    stepperMotor(512, stepperDirection);
    lastRotation = millis();
  }
}

// ============================================================================
// I2C RECEIVE FROM MEGA-1
// ============================================================================
void receiveFromMega1(int bytes) {
  Serial.println("\nI2C DATA RECEIVED!");
  
  if (bytes >= 1) {
    byte startSignal = Wire.read();
    
    if (startSignal == 1) {
      if (bytes == 5) {
        Wire.readBytes((char*)&preferredTemp, sizeof(float));
        Serial.print("Temp: ");
        Serial.print(preferredTemp, 1);
        Serial.println(" deg C");
      }
     
      tempReceived = true;
      systemStarted = true;
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
      
      Serial.println("MEGA-2 ACTIVATED!");
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
      Serial.println("\nNIGHT MODE ACTIVATED");
      preferredTemp = 22.0;
      controlRedLEDs(true);
      setRunwayBrightness(4);  // Dim runway lights at night
      
      if (!objectDetected && !planeDetected) controlYellowLEDs(false);
      
      lcd.clear();
      lcd.print("Night Mode");
      lcd.setCursor(0,1);
      lcd.print("Temp:22.0C");
      delay(2000);
    } else {
      Serial.println("\nDAY MODE ACTIVATED");
      controlRedLEDs(false);
      setRunwayBrightness(8);  // Brighter during day
    }
  }
}

// ============================================================================
// MOTION DETECTION
// ============================================================================
void checkMotion() {
  bool pir = digitalRead(PIR_PIN);

  if (pir == HIGH && lastPirState == LOW) handleMotionStart();
  if (pir == LOW && lastPirState == HIGH) motionDetected = false;
  if (pir == HIGH) lastMotionTime = millis();
  
  lastPirState = pir;
}

void handleMotionStart() {
  motionDetected = true;
  motionCount++;
  lastMotionTime = millis();

  if (!nightMode && !objectDetected && !planeDetected) controlYellowLEDs(true);

  lcd.clear();
  lcd.print("MOTION");
  lcd.setCursor(0,1);
  lcd.print("T:");
  lcd.print(currentTemp,1);
  lcd.print(" H:");
  lcd.print(currentHumidity,0);
  lcd.print("%");
  delay(1200);
}

// ============================================================================
// DHT11 SENSOR
// ============================================================================
void readEnvironmentalData() {
  currentHumidity = dht.readHumidity();
  currentTemp = dht.readTemperature();

  if (isnan(currentTemp) || isnan(currentHumidity)) {
    currentTemp = 25.0;
    currentHumidity = 50.0;
  }
}

// ============================================================================
// ULTRASONIC SENSOR
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
// SOUND & LED CONTROL
// ============================================================================
void playWarning() {
  int melody[] = {1000, 800, 1000, 800, 1000};
  for (int i = 0; i < 5; i++) {
    tone(PASSIVE_BUZZER_PIN, melody[i], 200);
    delay(250);
  }
  noTone(PASSIVE_BUZZER_PIN);
}

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
  Serial.println("°C)");   //put the hex code for degree like one of the labs

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

  if (planeDetected) Serial.println("WARNING: PLANE CONFLICT DETECTED!");
  else if (objectDetected) Serial.println("WARNING: Object detected - Gate CLOSED, Fan RUNNING");

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