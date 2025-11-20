// ============================================================================
// MEGA-2: ENVIRONMENTAL CONTROL SYSTEM (FIXED ULTRASONIC DETECTION)
// ============================================================================

#include <LiquidCrystal.h>
#include <Wire.h>
#include <DHT.h>

// ===== PIN DEFINITIONS =====
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

#define DHT_PIN 22
#define DHT_TYPE DHT11
const int PIR_PIN = 18;
const int ULTRASONIC_TRIG = 24;
const int ULTRASONIC_ECHO = 26;
const int PHOTORESISTOR_PIN = A0;

// Motor (L293D)
const int MOTOR_IN1 = 8;
const int MOTOR_IN2 = 9;
const int MOTOR_ENA = 10;

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

bool objectNearMotor = false;
unsigned long lastSafetyCheck = 0;

bool nightMode = false;

unsigned long lastDisplayUpdate = 0;
unsigned long lastSerialUpdate = 0;

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(9600);
  Wire.begin(8);
  Wire.onReceive(receiveFromMega1);

  lcd.begin(16, 2);
  dht.begin();

  pinMode(PIR_PIN, INPUT);
  pinMode(ULTRASONIC_TRIG, OUTPUT);
  pinMode(ULTRASONIC_ECHO, INPUT);
  pinMode(PHOTORESISTOR_PIN, INPUT);

  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_ENA, OUTPUT);

  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_ENA, 0);

  pinMode(PASSIVE_BUZZER_PIN, OUTPUT);

  for (int i = 0; i < 4; i++) {
    pinMode(yellowLEDs[i], OUTPUT);
    pinMode(redLEDs[i], OUTPUT);
  }

  lcd.clear();
  lcd.print("MEGA-2 Ready");
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
  
  checkUltrasonicSafety();  // ⭐ FIXED: Always check, not just when motor running
  
  controlMotor();

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

  nightMode = (lightLevel < 300);

  if (nightMode != previousNight) {

    if (nightMode) {
      Serial.println("\n🌙 *** NIGHT MODE ACTIVATED ***");
      preferredTemp = 22.0;

      controlRedLEDs(true);
      controlYellowLEDs(false);

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

  if (!nightMode) controlYellowLEDs(true);

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
// MOTOR CONTROL
// ============================================================================
void controlMotor() {
  bool shouldRun =
      tempReceived &&
      (currentTemp > preferredTemp) &&
      !objectNearMotor;

  if (shouldRun && !motorRunning) {
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
    analogWrite(MOTOR_ENA, 200);
    motorRunning = true;
    motorStartTime = millis();
  }

  else if (!shouldRun && motorRunning) {
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    analogWrite(MOTOR_ENA, 0);

    motorRunning = false;
    motorTotalTime += (millis() - motorStartTime) / 1000;
  }
}

// ============================================================================
// ULTRASONIC SAFETY (20 CM) - FIXED VERSION
// ============================================================================
void checkUltrasonicSafety() {

  if (millis() - lastSafetyCheck > 200) {
    lastSafetyCheck = millis();

    long distance = getUltrasonicDistance();
    bool prev = objectNearMotor;

    objectNearMotor = (distance < 20 && distance > 0);

    // Object just detected
    if (objectNearMotor && !prev) {
      if (motorRunning) {
        digitalWrite(MOTOR_IN1, LOW);
        digitalWrite(MOTOR_IN2, LOW);
        analogWrite(MOTOR_ENA, 0);
        motorRunning = false;
      }

      playWarning();

      lcd.clear();
      lcd.print("SAFETY STOP!");
      lcd.setCursor(0,1);
      lcd.print("Object <20cm");
    }

    // Object cleared
    else if (!objectNearMotor && prev) {
      noTone(PASSIVE_BUZZER_PIN);

      lcd.clear();
      lcd.print("Object Clear");
      lcd.setCursor(0,1);
      lcd.print("Resuming...");
      delay(1000);
      
      // Motor will restart automatically in controlMotor() if conditions are met
    }
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

  if (objectNearMotor) lcd.print("SAFETY STOP!");
  else if (motorRunning) {
    lcd.print("Fan ON ");
    unsigned long runtime = motorTotalTime + ((millis()-motorStartTime)/1000);
    lcd.print(runtime);
    lcd.print("s");
  }
  else {
    lcd.print("Motion:");
    lcd.print(motionCount);
  }
}

// ============================================================================
// SERIAL STATUS (WITH EMOJIS)
// ============================================================================
void sendStatus() {

  Serial.println("\n═══════════════════════════════════");
  Serial.println("         MEGA-2 STATUS REPORT");
  Serial.println("═══════════════════════════════════");

  Serial.print("🌡 Temperature: ");
  Serial.print(currentTemp,1);
  Serial.print("°C  (Preferred: ");
  Serial.print(preferredTemp,1);
  Serial.println("°C)");

  Serial.print("💧 Humidity: ");
  Serial.print(currentHumidity,0);
  Serial.println("%");

  Serial.print("👥 Motion Count: ");
  Serial.println(motionCount);

  Serial.print("❄ Motor: ");
  Serial.println(motorRunning ? "ON" : "OFF");

  Serial.print("🌙 Night Mode: ");
  Serial.println(nightMode ? "Active" : "Inactive");

  if (objectNearMotor) Serial.println("⚠ SAFETY: Object near motor!");

  Serial.print("⏱ Uptime: ");
  Serial.print(millis()/1000);
  Serial.println("s");

  Serial.print("\nJSON:{\"temp\":");
  Serial.print(currentTemp,1);
  Serial.print(",\"humidity\":");
  Serial.print(currentHumidity,0);
  Serial.print(",\"motor\":\"");
  Serial.print(motorRunning ? "ON" : "OFF");
  Serial.print("\",\"night\":");
  Serial.print(nightMode ? "true" : "false");
  Serial.print(",\"motion\":");
  Serial.print(motionCount);
  Serial.print(",\"preferred\":");
  Serial.print(preferredTemp,1);
  Serial.println("}");
}

//tf git changes made