// ============================================================================
// MEGA-2: Ground Control Unit [STEPPER + FAN + RFID PLANE DETECTION]
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

// Passive Buzzer
const int PASSIVE_BUZZER_PIN = 7;

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
  readEnvironmentalData();
  
  checkUltrasonicAndControlGate();  // Check ultrasonic + RFID and control gate + fan
  
  // Stepper Motor Control - CONTINUOUS ROTATION
  controlStepperContinuous();

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
// ULTRASONIC + RFID DETECTION + GATE + FAN CONTROL
// ============================================================================
void checkUltrasonicAndControlGate() {

  if (millis() - lastSafetyCheck > 200) {
    lastSafetyCheck = millis();

    long distance = getUltrasonicDistance();
    bool previousObjectState = objectDetected;
    bool previousPlaneState = planeDetected;

    // Object detected within 20cm
    objectDetected = (distance < 20 && distance > 2);

    if (objectDetected) {
      // Check if it's a PLANE (RFID tag detected)
      planeDetected = checkRFIDTag();
      
      // ==========================================
      // PLANE DETECTED (RFID TAG PRESENT)
      // ==========================================
      if (planeDetected && !previousPlaneState) {
        planeConflictCount++;
        
        Serial.println("\n⚠⚠⚠ PLANE CONFLICT! ⚠⚠⚠");
        Serial.println("Another plane incoming!");
        
        // URGENT ALARM
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
        
        controlYellowLEDs(true);  // Keep yellow on
        
        // SHOW WARNING ON LCD
        lcd.clear();
        lcd.print("CONFLICT DETECT!");
        lcd.setCursor(0, 1);
        lcd.print("Another plane!");
        delay(2000);
        
        lcd.clear();
        lcd.print("REDUCE AIRSPEED");
        lcd.setCursor(0, 1);
        lcd.print("IMMEDIATELY!");
        delay(2000);
        
        // GATE CLOSES (backward compatibility)
        closeGate();
        
        // FAN RUNS (backward compatibility)
        startFan();
      }
      
      // ==========================================
      // REGULAR OBJECT (NO RFID TAG)
      // ==========================================
      else if (!planeDetected && !previousObjectState) {
        // Normal object detection - works as before
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
    
    // ==========================================
    // OBJECT CLEARED
    // ==========================================
    else if (!objectDetected && (previousObjectState || previousPlaneState)) {
      planeDetected = false;
      
      delay(1000);  // Wait 1 second
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
  if (bytes == sizeof(float)) {
    Wire.readBytes((char*)&preferredTemp, sizeof(float));
    tempReceived = true;

    lcd.clear();
    lcd.print("Config OK");
    lcd.setCursor(0,1);
    lcd.print("Temp ");
    lcd.print(preferredTemp,1);
    lcd.print("C");
    delay(1500);
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

  if (motionDetected && millis() - lastMotionTime < 1500)
    return;

  lcd.clear();
  lcd.setCursor(0,0);
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

  Serial.print("⏱ Uptime: ");
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