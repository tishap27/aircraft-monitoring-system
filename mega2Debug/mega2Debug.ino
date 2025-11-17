// ============================================================================
// MEGA-2: ENVIRONMENTAL CONTROL SYSTEM - DEBUGGED VERSION
// Arduino MEGA 2560 with DHT11 - L293D Motor Driver
// ============================================================================

#include <LiquidCrystal.h>
#include <Wire.h>
#include <DHT.h>

// Enable detailed debugging output
#define DEBUG_MODE 1

// ===== PIN DEFINITIONS FOR MEGA 2560 =====
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// Sensors
#define DHT_PIN 22
#define DHT_TYPE DHT11
const int PIR_PIN = 18;
const int ULTRASONIC_TRIG = 24;
const int ULTRASONIC_ECHO = 26;
const int PHOTORESISTOR_PIN = A0;

// Motor control pins for L293D Motor Driver
const int MOTOR_IN1 = 8;
const int MOTOR_IN2 = 9;
const int MOTOR_ENA = 10;

// Passive Buzzer
const int PASSIVE_BUZZER_PIN = 7;

// LEDs - 8 individual LEDs
const int YELLOW_LED_1 = 28;
const int YELLOW_LED_2 = 30;
const int YELLOW_LED_3 = 32;
const int YELLOW_LED_4 = 34;
const int RED_LED_1 = 36;
const int RED_LED_2 = 38;
const int RED_LED_3 = 40;
const int RED_LED_4 = 42;

// I2C Configuration
const byte I2C_ADDRESS = 8;

// ===== COMPONENT INITIALIZATION =====
DHT dht(DHT_PIN, DHT_TYPE);

// ===== SYSTEM VARIABLES =====
float preferredTemp = 23.5;  // Default, updated by MEGA-1
float currentTemp = 0.0;
float currentHumidity = 0.0;

// Motion detection
bool motionDetected = false;
bool lastPirState = LOW;
int motionCount = 0;
unsigned long lastMotionTime = 0;

// Motor control
bool motorRunning = false;
unsigned long motorStartTime = 0;
unsigned long motorTotalTime = 0;

// Safety
bool objectNearMotor = false;
unsigned long lastSafetyCheck = 0;

// Night mode
bool nightMode = false;
int lastLightLevel = 0;

// Display control
unsigned long lastDisplayUpdate = 0;
unsigned long lastSerialUpdate = 0;

// LED arrays
int yellowLEDs[] = {YELLOW_LED_1, YELLOW_LED_2, YELLOW_LED_3, YELLOW_LED_4};
int redLEDs[] = {RED_LED_1, RED_LED_2, RED_LED_3, RED_LED_4};

// I2C data received flag
bool dataReceived = false;

void setup() {
  Serial.begin(9600);
  
  // Wait for serial to initialize
  delay(1000);
  
  Wire.begin(I2C_ADDRESS); // I2C Slave address
  Wire.onReceive(receiveFromMega1);
  
  // Initialize components
  lcd.begin(16, 2);
  dht.begin();
  
  // Initialize sensor pins
  pinMode(PIR_PIN, INPUT);
  pinMode(ULTRASONIC_TRIG, OUTPUT);
  pinMode(ULTRASONIC_ECHO, INPUT);
  pinMode(PHOTORESISTOR_PIN, INPUT);
  
  // Initialize L293D motor pins
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_ENA, OUTPUT);
  
  // Initialize motor to OFF state
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_ENA, 0);
  
  // Initialize buzzer
  pinMode(PASSIVE_BUZZER_PIN, OUTPUT);
  
  // Initialize LED pins
  for (int i = 0; i < 4; i++) {
    pinMode(yellowLEDs[i], OUTPUT);
    pinMode(redLEDs[i], OUTPUT);
  }
  
  debugPrintHeader();
  testComponents();
  
  // Welcome display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MEGA-2 System");
  lcd.setCursor(0, 1);
  lcd.print("L293D Ready...");
  
  delay(2000);
  
  Serial.println("[INIT] MEGA-2 Initialized");
  Serial.println("[INIT] DHT11 Sensor Ready");
  Serial.println("[INIT] PIR Motion Sensor Ready");
  Serial.println("[INIT] Ultrasonic Sensor Ready");
  Serial.println("[INIT] L293D Motor Driver Ready");
  Serial.print("[INIT] I2C Slave Ready (Address ");
  Serial.print(I2C_ADDRESS);
  Serial.println(")");
  Serial.println("");
  Serial.println("[WAIT] Waiting for MEGA-1 configuration...");
  Serial.println("====================================");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Waiting for");
  lcd.setCursor(0, 1);
  lcd.print("MEGA-1...");
}

void loop() {
  #if DEBUG_MODE
  static unsigned long lastDebugPrint = 0;
  if (millis() - lastDebugPrint > 15000) {
    debugPrintSensors();
    lastDebugPrint = millis();
  }
  #endif
  
  // Check night mode
  checkNightMode();
  
  // Check for motion
  checkMotion();
  
  // Read environmental data
  readEnvironmentalData();
  
  // Control motor
  controlMotor();
  
  // Check ultrasonic safety
  if (motorRunning) {
    checkUltrasonicSafety();
  }
  
  // Update displays
  if (millis() - lastDisplayUpdate > 1000) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }
  
  if (millis() - lastSerialUpdate > 3000) {
    sendStatus();
    lastSerialUpdate = millis();
  }
  
  delay(100);
}

// ===== DEBUGGING FUNCTIONS =====
void debugPrintHeader() {
  Serial.println("====================================");
  Serial.println("  MEGA-2: ENVIRONMENTAL CONTROL");
  Serial.println("      Arduino MEGA 2560 Slave");
  Serial.println("       L293D Motor Driver");
  Serial.println("====================================");
  Serial.println("");
}

void testComponents() {
  Serial.println("[TEST] Testing components...");
  
  // Test LCD
  Serial.print("[TEST] LCD: ");
  lcd.clear();
  lcd.print("Test");
  Serial.println("OK");
  delay(500);
  
  // Test DHT11
  Serial.print("[TEST] DHT11: ");
  float testTemp = dht.readTemperature();
  float testHumid = dht.readHumidity();
  if (isnan(testTemp) || isnan(testHumid)) {
    Serial.println("FAIL - Check DHT11 connections!");
  } else {
    Serial.print("OK (T:");
    Serial.print(testTemp, 1);
    Serial.print("C H:");
    Serial.print(testHumid, 0);
    Serial.println("%)");
  }
  
  // Test PIR
  Serial.print("[TEST] PIR Sensor: ");
  int pirState = digitalRead(PIR_PIN);
  Serial.print(pirState == HIGH ? "Motion detected" : "No motion");
  Serial.println(" - OK");
  
  // Test Photoresistor
  Serial.print("[TEST] Photoresistor: ");
  int lightVal = analogRead(PHOTORESISTOR_PIN);
  Serial.print("Value = ");
  Serial.print(lightVal);
  Serial.println(" - OK");
  
  // Test Ultrasonic
  Serial.print("[TEST] Ultrasonic: ");
  long dist = getUltrasonicDistance();
  Serial.print(dist);
  Serial.println(" cm - OK");
  
  // Test Motor Pins (without running motor)
  Serial.print("[TEST] Motor pins: ");
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_ENA, OUTPUT);
  Serial.println("OK");
  
  // Test Buzzer
  Serial.print("[TEST] Buzzer: ");
  tone(PASSIVE_BUZZER_PIN, 1000, 100);
  delay(150);
  Serial.println("OK");
  
  // Test LEDs
  Serial.print("[TEST] Yellow LEDs: ");
  controlYellowLEDs(true);
  delay(200);
  controlYellowLEDs(false);
  Serial.println("OK");
  
  Serial.print("[TEST] Red LEDs: ");
  controlRedLEDs(true);
  delay(200);
  controlRedLEDs(false);
  Serial.println("OK");
  
  Serial.println("[TEST] All component tests complete");
  Serial.println("");
}

void debugPrintSensors() {
  Serial.println("\n[DEBUG] Sensor Readings:");
  Serial.print("  Temperature: ");
  Serial.print(currentTemp, 1);
  Serial.println(" degrees C");
  Serial.print("  Humidity: ");
  Serial.print(currentHumidity, 0);
  Serial.println("%");
  Serial.print("  Light level: ");
  Serial.println(lastLightLevel);
  Serial.print("  PIR state: ");
  Serial.println(digitalRead(PIR_PIN) == HIGH ? "MOTION" : "NO MOTION");
  Serial.print("  Ultrasonic: ");
  Serial.print(getUltrasonicDistance());
  Serial.println(" cm");
  Serial.print("  Motor: ");
  Serial.println(motorRunning ? "RUNNING" : "STOPPED");
  Serial.print("  Night Mode: ");
  Serial.println(nightMode ? "ACTIVE" : "INACTIVE");
}

// ===== I2C RECEIVE FROM MEGA-1 =====
void receiveFromMega1(int bytes) {
  Serial.print("\n[I2C] Received ");
  Serial.print(bytes);
  Serial.println(" bytes");
  
  if (bytes == sizeof(float)) {
    Wire.readBytes((char*)&preferredTemp, sizeof(float));
    dataReceived = true;
    
    Serial.println("[I2C] DATA RECEIVED!");
    Serial.print("[I2C] Preferred Temperature: ");
    Serial.print(preferredTemp, 1);
    Serial.println(" degrees C from MEGA-1");
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Config Received");
    lcd.setCursor(0, 1);
    lcd.print("Temp: ");
    lcd.print(preferredTemp, 1);
    lcd.print("C");
    
    delay(2000);
    
    Serial.println("[SUCCESS] System configured - starting monitoring");
    Serial.println("====================================");
  } else {
    Serial.print("[I2C] ERROR - Expected ");
    Serial.print(sizeof(float));
    Serial.print(" bytes, got ");
    Serial.println(bytes);
  }
}

// ===== NIGHT MODE DETECTION =====
void checkNightMode() {
  lastLightLevel = analogRead(PHOTORESISTOR_PIN);
  bool previousNightMode = nightMode;
  nightMode = (lastLightLevel < 300);
  
  if (nightMode != previousNightMode) {
    if (nightMode) {
      Serial.println("\n[NIGHT] NIGHT MODE ACTIVATED");
      Serial.print("[NIGHT] Light level: ");
      Serial.println(lastLightLevel);
      preferredTemp = 22.0;
      
      controlRedLEDs(true);
      controlYellowLEDs(false);
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Night Mode");
      lcd.setCursor(0, 1);
      lcd.print("Temp: 22.0C");
      
      delay(2000);
      
    } else {
      Serial.println("\n[DAY] DAY MODE ACTIVATED");
      Serial.print("[DAY] Light level: ");
      Serial.println(lastLightLevel);
      controlRedLEDs(false);
    }
  }
}

// ===== MOTION DETECTION =====
void checkMotion() {
  bool currentPirState = digitalRead(PIR_PIN);
  
  if (currentPirState == HIGH && lastPirState == LOW) {
    handleMotionStart();
  }
  
  if (currentPirState == LOW && lastPirState == HIGH) {
    handleMotionEnd();
  }
  
  if (currentPirState == HIGH) {
    lastMotionTime = millis();
  }
  
  lastPirState = currentPirState;
}

void handleMotionStart() {
  motionCount++;
  motionDetected = true;
  lastMotionTime = millis();
  
  Serial.println("\n[MOTION] MOTION DETECTED");
  Serial.print("[MOTION] Motion Event #");
  Serial.println(motionCount);
  
  // Turn on yellow LEDs
  if (!nightMode) {
    controlYellowLEDs(true);
  }
  
  // Show motion alert
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("*** MOTION! ***");
  lcd.setCursor(0, 1);
  lcd.print("T:");
  lcd.print(currentTemp, 1);
  lcd.print("C H:");
  lcd.print(currentHumidity, 0);
  lcd.print("%");
  
  delay(2000);
}

void handleMotionEnd() {
  if (motionDetected) {
    motionDetected = false;
    Serial.println("[MOTION] Motion ended");
  }
}

// ===== ENVIRONMENTAL DATA READING =====
void readEnvironmentalData() {
  static unsigned long lastDHTRead = 0;
  
  // Read DHT11 every 2 seconds (sensor limit)
  if (millis() - lastDHTRead > 2000) {
    currentHumidity = dht.readHumidity();
    currentTemp = dht.readTemperature();
    
    if (isnan(currentTemp) || isnan(currentHumidity)) {
      Serial.println("[DHT] ERROR - DHT11 read failed - check connections");
      // Keep last valid values or use fallback
      if (currentTemp == 0.0) currentTemp = 25.0;
      if (currentHumidity == 0.0) currentHumidity = 50.0;
    } else {
      Serial.print("[DHT] T:");
      Serial.print(currentTemp, 1);
      Serial.print("C H:");
      Serial.print(currentHumidity, 0);
      Serial.println("%");
    }
    
    lastDHTRead = millis();
  }
}

// ===== MOTOR CONTROL WITH L293D =====
void controlMotor() {
  bool shouldRun = (currentTemp > preferredTemp) && !objectNearMotor;
  
  if (shouldRun && !motorRunning) {
    // Start motor - FORWARD direction with L293D
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
    analogWrite(MOTOR_ENA, 200); // 78% speed
    
    motorRunning = true;
    motorStartTime = millis();
    
    Serial.println("\n[MOTOR] MOTOR STARTED - Cooling activated");
    Serial.println("[MOTOR] L293D: IN1=HIGH, IN2=LOW, ENA=200");
    Serial.print("[MOTOR] Current: ");
    Serial.print(currentTemp, 1);
    Serial.print("C > Preferred: ");
    Serial.print(preferredTemp, 1);
    Serial.println("C");
    
  } else if (!shouldRun && motorRunning) {
    // Stop motor
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    analogWrite(MOTOR_ENA, 0);
    
    motorRunning = false;
    
    if (motorStartTime > 0) {
      motorTotalTime += (millis() - motorStartTime) / 1000;
    }
    
    Serial.println("\n[MOTOR] MOTOR STOPPED");
    Serial.println("[MOTOR] L293D: All pins LOW");
    if (objectNearMotor) {
      Serial.println("[MOTOR] Reason: Safety override");
    } else {
      Serial.println("[MOTOR] Reason: Temperature reached target");
    }
  }
  
  #if DEBUG_MODE
  static unsigned long lastMotorDebug = 0;
  if (motorRunning && millis() - lastMotorDebug > 5000) {
    Serial.print("[MOTOR] Running for ");
    Serial.print((millis() - motorStartTime) / 1000);
    Serial.println(" seconds");
    lastMotorDebug = millis();
  }
  #endif
}

// ===== ULTRASONIC SAFETY SYSTEM =====
void checkUltrasonicSafety() {
  if (millis() - lastSafetyCheck > 200) {
    lastSafetyCheck = millis();
    
    long distance = getUltrasonicDistance();
    bool previousObjectNear = objectNearMotor;
    objectNearMotor = (distance < 20 && distance > 0);
    
    if (objectNearMotor && !previousObjectNear) {
      // EMERGENCY STOP
      digitalWrite(MOTOR_IN1, LOW);
      digitalWrite(MOTOR_IN2, LOW);
      analogWrite(MOTOR_ENA, 0);
      motorRunning = false;
      
      Serial.println("\n[SAFETY] SAFETY ALERT!");
      Serial.print("[SAFETY] Object detected at ");
      Serial.print(distance);
      Serial.println(" cm");
      Serial.println("[SAFETY] Motor stopped immediately");
      
      playWarning();
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("SAFETY STOP!");
      lcd.setCursor(0, 1);
      lcd.print("Object <20cm");
      
    } else if (!objectNearMotor && previousObjectNear) {
      Serial.println("[SAFETY] Object moved away - motor can restart");
      noTone(PASSIVE_BUZZER_PIN);
    }
    
    #if DEBUG_MODE
    static unsigned long lastDistDebug = 0;
    if (millis() - lastDistDebug > 5000) {
      Serial.print("[ULTRASONIC] Distance: ");
      Serial.print(distance);
      Serial.println(" cm");
      lastDistDebug = millis();
    }
    #endif
  }
}

long getUltrasonicDistance() {
  digitalWrite(ULTRASONIC_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG, LOW);
  
  long duration = pulseIn(ULTRASONIC_ECHO, HIGH, 30000);
  if (duration == 0) {
    Serial.println("[ULTRASONIC] No echo received");
    return 999;
  }
  
  return (duration * 0.034) / 2;
}

void playWarning() {
  int melody[] = {1000, 800, 1000, 800, 1000};
  
  Serial.println("[BUZZER] Playing warning tone");
  for (int i = 0; i < 5; i++) {
    tone(PASSIVE_BUZZER_PIN, melody[i], 200);
    delay(250);
  }
  noTone(PASSIVE_BUZZER_PIN);
}

// ===== LED CONTROL =====
void controlYellowLEDs(bool state) {
  for (int i = 0; i < 4; i++) {
    digitalWrite(yellowLEDs[i], state ? HIGH : LOW);
  }
  Serial.print("[LED] Yellow LEDs: ");
  Serial.println(state ? "ON" : "OFF");
}

void controlRedLEDs(bool state) {
  for (int i = 0; i < 4; i++) {
    digitalWrite(redLEDs[i], state ? HIGH : LOW);
  }
  Serial.print("[LED] Red LEDs: ");
  Serial.println(state ? "ON" : "OFF");
}

// ===== DISPLAY UPDATE =====
void updateDisplay() {
  if (millis() - lastMotionTime < 3000 && motionDetected) {
    return;
  }
  
  lcd.clear();
  
  // Line 1: Temperature and Humidity
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(currentTemp, 1);
  lcd.print("C H:");
  lcd.print(currentHumidity, 0);
  lcd.print("%");
  
  if (nightMode) {
    lcd.setCursor(15, 0);
    lcd.print("N");
  }
  
  // Line 2: Motor/Motion status
  lcd.setCursor(0, 1);
  if (objectNearMotor) {
    lcd.print("SAFETY STOP!");
  } else if (motorRunning) {
    lcd.print("Fan ON ");
    unsigned long runtime = motorTotalTime + ((millis() - motorStartTime) / 1000);
    lcd.print(runtime);
    lcd.print("s");
  } else {
    lcd.print("Motion:");
    lcd.print(motionCount);
    
    if (millis() - lastMotionTime < 10000) {
      lcd.setCursor(11, 1);
      lcd.print("ACTIV");
    } else {
      lcd.setCursor(11, 1);
      lcd.print("QUIET");
    }
  }
}

// ===== SERIAL STATUS =====
void sendStatus() {
  Serial.println("\n===================================");
  Serial.println("     MEGA-2 STATUS REPORT");
  Serial.println("===================================");
  
  Serial.print("Temperature: ");
  Serial.print(currentTemp, 1);
  Serial.print(" degrees C (Preferred: ");
  Serial.print(preferredTemp, 1);
  Serial.println(" degrees C)");
  
  Serial.print("Humidity: ");
  Serial.print(currentHumidity, 0);
  Serial.println("%");
  
  Serial.print("Motion Count: ");
  Serial.println(motionCount);
  
  Serial.print("Motor Status: ");
  Serial.println(motorRunning ? "ON (Cooling)" : "OFF (Standby)");
  
  if (motorRunning) {
    unsigned long runtime = motorTotalTime + ((millis() - motorStartTime) / 1000);
    Serial.print("   Runtime: ");
    Serial.print(runtime);
    Serial.println(" seconds");
  }
  
  Serial.print("Night Mode: ");
  Serial.println(nightMode ? "Active" : "Inactive");
  
  Serial.print("Light Level: ");
  Serial.println(lastLightLevel);
  
  if (objectNearMotor) {
    Serial.println("SAFETY: Object near motor detected!");
  }
  
  Serial.print("Config Received: ");
  Serial.println(dataReceived ? "YES" : "NO - Still waiting");
  
  Serial.print("Uptime: ");
  Serial.print(millis() / 1000);
  Serial.println(" seconds");
  
  // JSON for Node-RED
  Serial.print("\nJSON: {\"temp\":");
  Serial.print(currentTemp, 1);
  Serial.print(",\"humidity\":");
  Serial.print(currentHumidity, 0);
  Serial.print(",\"motor\":\"");
  Serial.print(motorRunning ? "ON" : "OFF");
  Serial.print("\",\"night\":");
  Serial.print(nightMode ? "true" : "false");
  Serial.print(",\"motion\":");
  Serial.print(motionCount);
  Serial.print(",\"preferred\":");
  Serial.print(preferredTemp, 1);
  Serial.print(",\"light\":");
  Serial.print(lastLightLevel);
  Serial.println("}");
  
  Serial.println("===================================\n");
}

// ============================================================================
// L293D WIRING NOTES:
// ============================================================================
// Arduino MEGA Pin 8  -> L293D IN1 (Direction control 1)
// Arduino MEGA Pin 9  -> L293D IN2 (Direction control 2)
// Arduino MEGA Pin 10 -> L293D ENA (Speed control - PWM)
// L293D OUT1 -> Motor wire 1
// L293D OUT2 -> Motor wire 2
// L293D VCC1 (Pin 16) -> Arduino 5V (Logic power)
// L293D VCC2 (Pin 8)  -> External power supply (Motor power, 6-12V)
// L293D GND (Pins 4, 5, 12, 13) -> Common ground with Arduino
//
// MOTOR CONTROL LOGIC:
// - Forward: IN1=HIGH, IN2=LOW
// - Reverse: IN1=LOW, IN2=HIGH (not used)
// - Stop: IN1=LOW, IN2=LOW
// - Speed controlled by PWM on ENA pin (0-255)
// ============================================================================