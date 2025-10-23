// ======================================================================================================
// UNO-2 RIGHT VALA : ENVIRONMENTAL CONTROL
// Simplified for Arduino UNO pin limitations with L293D motor driver control will have to change to mega
// ======================================================================================================

#include <LiquidCrystal.h>
#include <Wire.h>

// ===== PIN DEFINITIONS FOR UNO =====
// LCD Display
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// Sensors
const int PIR_PIN = 8;              // PIR motion sensor
const int TMP36_PIN = A0;           // Temperature sensor
const int ULTRASONIC_TRIG = 6;      // Ultrasonic trigger
const int ULTRASONIC_ECHO = 7;      // Ultrasonic echo
const int PHOTORESISTOR_PIN = A1;   // Light sensor (with 10k resistor to GND)

// Actuators (L293D Connected)
const int MOTOR_EN = 9;             // PWM enable pin for motor speed
const int MOTOR_IN1 = A2;           // Motor direction input 1 (digital)
const int MOTOR_IN2 = A3;           // Motor direction input 2 (digital)
const int BUZZER_PIN = 10;          // Buzzer

// LEDs (Simplified - only 4 total due to UNO pin limitations)
const int YELLOW_LED = 13;          // Motion indicator (1 LED represents 4)
const int RED_LED = 1;             // Night mode indicator (1 LED represents 4)
const int STATUS_LED = 13;          // Built-in LED for motor/status

// ===== SYSTEM VARIABLES =====
float preferredTemp = 23.5;  // Default, updated by UNO-1
float currentTemp = 0.0;

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

// Display control
unsigned long lastDisplayUpdate = 0;
unsigned long lastSerialUpdate = 0;

void setup() {
  Serial.begin(9600);
  Wire.begin(8); // I2C Slave address 8
  Wire.onReceive(receiveFromUno1);

  // Initialize LCD
  lcd.begin(16, 2);

  // Initialize pins
  pinMode(PIR_PIN, INPUT);
  pinMode(ULTRASONIC_TRIG, OUTPUT);
  pinMode(ULTRASONIC_ECHO, INPUT);
  pinMode(PHOTORESISTOR_PIN, INPUT);

  pinMode(MOTOR_EN, OUTPUT);
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);

  Serial.println("=== UNO-2: ENVIRONMENTAL CONTROL ===");
  Serial.println("Two-UNO Project - .....");
  Serial.println("");

  // Welcome display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("UNO-2 System");
  lcd.setCursor(0, 1);
  lcd.print("Ready...");

  delay(2000);

  Serial.println("UNO-2 Initialized");
  Serial.println("Waiting for UNO-1...");
  Serial.println("====================");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Waiting for");
  lcd.setCursor(0, 1);
  lcd.print("UNO-1...");
}

void loop() {
  // Check night mode
  checkNightMode();

  // Check for motion
  checkMotion();

  // Read temperature
  readTemperature();

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

// ===== I2C RECEIVE FROM UNO-1 =====
void receiveFromUno1(int bytes) {
  if (bytes == sizeof(float)) {
    Wire.readBytes((char*)&preferredTemp, sizeof(float));

    Serial.print("I2C: Received ");
    Serial.print(preferredTemp, 1);
    Serial.println("°C from UNO-1");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Config Received");
    lcd.setCursor(0, 1);
    lcd.print("Temp: ");
    lcd.print(preferredTemp, 1);
    lcd.print("C");

    delay(2000);
  }
}

// ===== NIGHT MODE DETECTION =====
void checkNightMode() {
  int lightLevel = analogRead(PHOTORESISTOR_PIN);
  bool previousNightMode = nightMode;
  nightMode = (lightLevel < 300);

  if (nightMode != previousNightMode) {
    if (nightMode) {
      Serial.println("*** NIGHT MODE ***");
      preferredTemp = 22.0;

      digitalWrite(RED_LED, HIGH);
      digitalWrite(YELLOW_LED, LOW);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Night Mode");
      lcd.setCursor(0, 1);
      lcd.print("Temp: 22.0C");

      delay(2000);

    } else {
      Serial.println("*** DAY MODE ***");
      digitalWrite(RED_LED, LOW);
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

  Serial.println("*** MOTION DETECTED ***");
  Serial.print("Count: ");
  Serial.println(motionCount);

  // Turn on yellow LED
  if (!nightMode) {
    digitalWrite(YELLOW_LED, HIGH);
  }

  // Show motion alert
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("*** MOTION! ***");
  lcd.setCursor(0, 1);
  lcd.print("T:");
  lcd.print(currentTemp, 1);
  lcd.print("C #");
  lcd.print(motionCount);

  delay(2000);
}

void handleMotionEnd() {
  if (motionDetected) {
    motionDetected = false;
    Serial.println("Motion ended");
  }
}

// ===== TEMPERATURE READING =====
void readTemperature() {
  int sensorValue = analogRead(TMP36_PIN);
  float voltage = sensorValue * (5.0 / 1023.0);
  currentTemp = (voltage - 0.5) * 100.0;
}

// ===== MOTOR CONTROL =====
void controlMotor() {
  bool shouldRun = (currentTemp > preferredTemp) && !objectNearMotor;

  if (shouldRun && !motorRunning) {
    // Motor forward
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
    analogWrite(MOTOR_EN, 200);
    motorRunning = true;
    motorStartTime = millis();
    digitalWrite(STATUS_LED, HIGH);

    Serial.println("Motor STARTED");
  } else if (!shouldRun && motorRunning) {
    // Stop motor
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    analogWrite(MOTOR_EN, 0);
    motorRunning = false;
    digitalWrite(STATUS_LED, LOW);

    if (motorStartTime > 0) {
      motorTotalTime += (millis() - motorStartTime) / 1000;
    }

    Serial.println("Motor STOPPED");
    if (objectNearMotor) {
      Serial.println("Reason: Safety");
    } else {
      Serial.println("Reason: Temp OK");
    }
  }
}

// ===== ULTRASONIC SAFETY =====
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
      analogWrite(MOTOR_EN, 0);
      motorRunning = false;
      digitalWrite(STATUS_LED, LOW);

      Serial.println("SAFETY ALERT!");
      Serial.print("Distance: ");
      Serial.print(distance);
      Serial.println(" cm");

      playWarning();

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("SAFETY STOP!");
      lcd.setCursor(0, 1);
      lcd.print("Object <20cm");

    } else if (!objectNearMotor && previousObjectNear) {
      Serial.println(" ** Object moved");
      noTone(BUZZER_PIN);
    }
  }
}

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

void playWarning() {
  int melody[] = {1000, 800, 1000, 800, 1000};

  for (int i = 0; i < 5; i++) {
    tone(BUZZER_PIN, melody[i], 200);
    delay(250);
  }
  noTone(BUZZER_PIN);
}

// ===== DISPLAY UPDATE =====
void updateDisplay() {
  if (millis() - lastMotionTime < 3000 && motionDetected) {
    return;
  }

  lcd.clear();

  // Line 1: Temperature
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(currentTemp, 1);
  lcd.print("C");

  if (nightMode) {
    lcd.setCursor(10, 0);
    lcd.print("NIGHT");
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
    }
  }
}

// ===== SERIAL STATUS =====
void sendStatus() {
  Serial.println("=== UNO-2 STATUS ===");
  Serial.print("Temperature: ");
  Serial.print(currentTemp, 1);
  Serial.println("C");
  Serial.print("Preferred: ");
  Serial.print(preferredTemp, 1);
  Serial.println("C");
  Serial.print("Motion Count: ");
  Serial.println(motionCount);
  Serial.print("Motor: ");
  Serial.println(motorRunning ? "ON" : "OFF");

  if (motorRunning) {
    unsigned long runtime = motorTotalTime + ((millis() - motorStartTime) / 1000);
    Serial.print("Runtime: ");
    Serial.print(runtime);
    Serial.println("s");
  }

  Serial.print("Night Mode: ");
  Serial.println(nightMode ? "Active" : "Inactive");

  if (objectNearMotor) {
    Serial.println("SAFETY: Object detected");
  }

  // JSON for Node-RED
  Serial.print("JSON: {\"temp\":");
  Serial.print(currentTemp, 1);
  Serial.print(",\"motor\":\"");
  Serial.print(motorRunning ? "ON" : "OFF");
  Serial.print("\",\"night\":");
  Serial.print(nightMode ? "true" : "false");
  Serial.print(",\"motion\":");
  Serial.print(motionCount);
  Serial.println("}");
  
  Serial.println("===================");
}
