/*============================================================================
 * MEGA-1: Flight Control Unit
 * 
 * DESCRIPTION:
 * This program runs on Arduino MEGA 2560 #1 and serves as the flight computer
 * for an aircraft simulation system. It manages flight-critical operations
 * including altitude monitoring, landing gear deployment, attitude tracking,
 * and motor control. Features secure passcode authentication, I2C communication
 * with ground station (MEGA-2), and real-time flight status monitoring.
 * 
 * Communication:
 *   I2C Bus (Master) - Single shared I2C bus communicating with:
 *                        - MPU6050 sensor (I2C address 0x68)
 *                        - MEGA-2 ground station (I2C address 8)
 * SYSTEM OPERATION:
 * 1. Passcode Entry: User enters and confirms 4-digit security code
 * 2. Temperature Setting: User sets preferred cabin temperature (3 digits)
 * 3. System Start: Press button to begin flight operations
 * 4. Flight Mode: Continuous monitoring of altitude, attitude, and systems
 * 5. Landing Sequence: Auto-deploys gear at 15cm altitude
 * 6. Reset: IR remote POWER button resets system to passcode entry
 * 
 * SAFETY FEATURES:
 *  Stall warning at 40° nose-up pitch
 *  Roll warnings beyond ±30° bank angle
 *  Altitude warnings at 20cm, 15cm, and 6cm
 *  7-second minimum gear deployment time before retraction
 *  Automatic fan shutdown when grounded (distance < 6cm)
 *  MPU6050 health monitoring with stuck value detection
 *  Yaw drift reset when grounded and stable
 * 
 * MPU6050 + MOTOR(FAN) + LANDING GEAR SERVO + NAV LIGHTS [L293D Motor Driver]
 * ============================================================================
 */ 


#include <LiquidCrystal.h>
#include <Keypad.h>
#include <Wire.h>
#include <MPU6050.h>
#include <Servo.h>
#include <IRremote.h>

// ========== NAVIGATION LIGHTS (Cessna 150 Style) ==========
const int LED_RED_LEFT = 31;      // Port (left) wingtip - Red
const int LED_GREEN_RIGHT = 32;   // Starboard (right) wingtip - Green
const int LED_WHITE_TAIL = 33;    // Tail strobe - White/Blue

bool navLightsOn = false;
unsigned long lastStrobeTime = 0;
const int STROBE_INTERVAL = 1000;  // Flash every 1 second
bool strobeState = false;

// ========== LCD ==========
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// ========== KEYPAD ==========
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {39, 41, 43, 45};
byte colPins[COLS] = {47, 49, 51, 53};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ========== BUZZER / BUTTON ==========
const int ACTIVE_BUZZER_PIN = 22;
const int PASSIVE_BUZZER_PIN = 13;
const int BUTTON_PIN = 18;

// ========== ULTRASONIC SENSOR ==========
const int ULTRASONIC_TRIG = 6;
const int ULTRASONIC_ECHO = 7;
float smoothedDistance = 999.0;
const float DISTANCE_FILTER_ALPHA = 0.3;

// ========== L293D MOTOR DRIVER (FAN/PROPELLER) ==========
const int MOTOR_IN1 = 28;
const int MOTOR_IN2 = 29;
const int MOTOR_EN = 30;
bool fanRunning = false;
int fanSpeed = 255;

//========== IRRemote ====================
const int IR_RECEIVE_PIN = 15;

// ========== LANDING GEAR (Standard Servo) ==========
Servo landingGearServo;
const int LANDING_GEAR_PIN = 27;
const int GEAR_UP_ANGLE = 90;
const int GEAR_DOWN_ANGLE = 180;
bool gearDeployed = false;
unsigned long gearDeployTime = 0;
const unsigned long MIN_GEAR_DOWN_TIME = 7000;

// ========== I2C ==========
float preferredTemp = 0.0;

// ========== MPU6050 SENSOR ==========
MPU6050 mpu;
bool mpuAvailable = false;

//Core flight Maneuvers
float pitch = 0;
float roll = 0;
float yaw = 0;

unsigned long lastMPUUpdate = 0;
const int MPU_UPDATE_INTERVAL = 1000;

int16_t ax_offset = 0, ay_offset = 0, az_offset = 0;
int16_t gx_offset = 0, gy_offset = 0, gz_offset = 0;

float pitch_filtered = 0;
float roll_filtered = 0;
float yaw_filtered = 0;
const float FILTER_ALPHA = 0.15;


// MPU6050 variables 
unsigned long lastIMUHealthCheck = 0;
int imuErrorCount = 0;
const int IMU_MAX_ERRORS = 5;

// ============================================================================
// NAVIGATION LIGHTS FUNCTIONS
// ============================================================================

void initNavLights() {
  pinMode(LED_RED_LEFT, OUTPUT);
  pinMode(LED_GREEN_RIGHT, OUTPUT);
  pinMode(LED_WHITE_TAIL, OUTPUT);
  
  // Turn off all lights initially
  digitalWrite(LED_RED_LEFT, LOW);
  digitalWrite(LED_GREEN_RIGHT, LOW);
  digitalWrite(LED_WHITE_TAIL, LOW);
  
  Serial.println("Navigation lights initialized - OFF");
}

void turnOnNavLights() {
  if (navLightsOn) return;
  
  navLightsOn = true;
  
  digitalWrite(LED_RED_LEFT, HIGH);
  digitalWrite(LED_GREEN_RIGHT, HIGH);
  
  Serial.println("NAVIGATION LIGHTS ON");
}

void turnOffNavLights() {
  if (!navLightsOn) return;
  
  navLightsOn = false;
  
  digitalWrite(LED_RED_LEFT, LOW);
  digitalWrite(LED_GREEN_RIGHT, LOW);
  digitalWrite(LED_WHITE_TAIL, LOW);
  
  Serial.println("NAVIGATION LIGHTS OFF");
}

void updateNavLights() {
  if (!navLightsOn) return;
  
  // Keep port (red) and starboard (green) steady
  digitalWrite(LED_RED_LEFT, HIGH);
  digitalWrite(LED_GREEN_RIGHT, HIGH);
  
  // Flash tail strobe
  if (millis() - lastStrobeTime >= STROBE_INTERVAL) {
    strobeState = !strobeState;
    digitalWrite(LED_WHITE_TAIL, strobeState ? HIGH : LOW);
    lastStrobeTime = millis();
  }
}

// ============================================================================
// BUZZER FEEDBACK
// ============================================================================
unsigned long lastBuzzerTime = 0;
const int BUZZER_DEBOUNCE = 500;

const float NOSE_DOWN_THRESHOLD = -20.0;
const float NOSE_UP_THRESHOLD = 20.0;
const float STALL_WARNING_THRESHOLD = 40.0;

void buzzNoseDown() {
  tone(PASSIVE_BUZZER_PIN, 262, 200);
}

void buzzNoseUp() {
  tone(PASSIVE_BUZZER_PIN, 392, 200);
}

void buzzStallWarning() {
  tone(PASSIVE_BUZZER_PIN, 800, 200);
}

void buzzRoll() {
  tone(PASSIVE_BUZZER_PIN, 330, 150);
}

void handlePitchBuzzer(float currentPitch) {
  unsigned long now = millis();
  
  if (now - lastBuzzerTime < BUZZER_DEBOUNCE) {
    return;
  }
  
  if (currentPitch > STALL_WARNING_THRESHOLD) {
    buzzStallWarning();
    lastBuzzerTime = now;
    Serial.println("STALL WARNING");
    return;
  }
  
  if (currentPitch > NOSE_UP_THRESHOLD && currentPitch <= STALL_WARNING_THRESHOLD) {
    buzzNoseUp();
    lastBuzzerTime = now;
    Serial.println("NOSE UP");
    return;
  }
  
  if (currentPitch < NOSE_DOWN_THRESHOLD) {
    buzzNoseDown();
    lastBuzzerTime = now;
    Serial.println("NOSE DOWN");
    return;
  }
}

void handleRollBuzzer(float currentRoll) {
  unsigned long now = millis();
  
  if (now - lastBuzzerTime < BUZZER_DEBOUNCE) {
    return;
  }
  
  if (currentRoll > 30 || currentRoll < -30) {
    buzzRoll();
    lastBuzzerTime = now;
    if (currentRoll > 0) {
      Serial.println("ROLLING RIGHT");
    } else {
      Serial.println("ROLLING LEFT");
    }
    return;
  }
}

// ============================================================================
// 7-SEGMENT DISPLAY
// ============================================================================

#define SR_DATA   8
#define SR_CLOCK 10
#define SR_LATCH  9

#define DIGIT1 23
#define DIGIT2 24
#define DIGIT3 25
#define DIGIT4 26

byte segmentMap[10] = {
  B00111111, B00000110, B01011011, B01001111, B01100110,
  B01101101, B01111101, B00000111, B01111111, B01101111
};

void sr_send(byte data) {
  digitalWrite(SR_LATCH, LOW);
  shiftOut(SR_DATA, SR_CLOCK, MSBFIRST, data);
  digitalWrite(SR_LATCH, HIGH);
}

void clearDigits() {
  digitalWrite(DIGIT1, HIGH);
  digitalWrite(DIGIT2, HIGH);
  digitalWrite(DIGIT3, HIGH);
  digitalWrite(DIGIT4, HIGH);
}

void showDigit(int digit, int position) {
  clearDigits();
  sr_send(segmentMap[digit]);
  switch(position) {
    case 0: digitalWrite(DIGIT1, LOW); break;
    case 1: digitalWrite(DIGIT2, LOW); break;
    case 2: digitalWrite(DIGIT3, LOW); break;
    case 3: digitalWrite(DIGIT4, LOW); break;
  }
}

// ============================================================================
// FAN/PROPELLER CONTROL (L293D MOTOR DRIVER)
// ============================================================================
void startFan() {
  if (fanRunning) return;
  
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_EN, fanSpeed);
  
  fanRunning = true;
  Serial.println("FAN STARTED (L293D MOTOR ON)");
}

void stopFan() {
  if (!fanRunning) return;
  
  analogWrite(MOTOR_EN, 0);
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  
  fanRunning = false;
  Serial.println("FAN STOPPED (L293D MOTOR OFF)");
}

void setFanSpeed(int speed) {
  if (speed < 0) speed = 0;
  if (speed > 255) speed = 255;
  
  fanSpeed = speed;
  if (fanRunning) {
    analogWrite(MOTOR_EN, fanSpeed);
  }
}

// ============================================================================
// LANDING GEAR CONTROL
// ============================================================================

void deployLandingGear() {
  if (gearDeployed) return;
  
  Serial.println("DEPLOYING LANDING GEAR");
  
  tone(PASSIVE_BUZZER_PIN, 600, 200);
  delay(250);
  tone(PASSIVE_BUZZER_PIN, 500, 200);
  delay(250);
  
  for (int angle = GEAR_UP_ANGLE; angle <= GEAR_DOWN_ANGLE; angle += 3) {
    landingGearServo.write(angle);
    delay(20);
  }
  
  gearDeployed = true;
  gearDeployTime = millis();
  
  tone(PASSIVE_BUZZER_PIN, 800, 300);
  Serial.println("LANDING GEAR DEPLOYED");

  stopFan();
  delay(500);
}

void retractLandingGear() {
  if (!gearDeployed) return;
  
  Serial.println("RETRACTING LANDING GEAR");
  
  for (int angle = GEAR_DOWN_ANGLE; angle >= GEAR_UP_ANGLE; angle -= 3) {
    landingGearServo.write(angle);
    delay(20);
  }
  
  gearDeployed = false;
  
  tone(PASSIVE_BUZZER_PIN, 1000, 200);
  Serial.println("LANDING GEAR RETRACTED");
  
  startFan();
  delay(500);
}


// ============================================================================
// ULTRASONIC SENSOR
// ============================================================================

float getUltrasonicDistance() {
  digitalWrite(ULTRASONIC_TRIG, LOW);
  delayMicroseconds(2);
  
  digitalWrite(ULTRASONIC_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG, LOW);
  
  long duration = pulseIn(ULTRASONIC_ECHO, HIGH, 30000);
  
  if (duration == 0) {
    return smoothedDistance; // Return last good reading instead of 999
  }
  
  float distance = (duration * 0.0343) / 2.0;
  
  if (distance < 2.0 || distance > 400.0) {
    return smoothedDistance; // Return last good reading instead of 999
  }
  
  return distance;
}

unsigned long lastUltrasonicCheck = 0;
float lastDistance = 999;
int groundedCount = 0;
const int ALTITUDE_THRESHOLD = 15; // Deploy gear at or below 15cm
unsigned long lastWarningTime = 0;



void checkLandingAltitude() {
  if (millis() - lastUltrasonicCheck < 200) {
    return;
  }
  lastUltrasonicCheck = millis();
  
  float rawDistance = getUltrasonicDistance();
  
  if (rawDistance != smoothedDistance && rawDistance < 400 && rawDistance > 2) {
    smoothedDistance = (DISTANCE_FILTER_ALPHA * rawDistance) + 
                       ((1 - DISTANCE_FILTER_ALPHA) * smoothedDistance);
    lastDistance = smoothedDistance;
  }
  
  float distance = lastDistance;
  
  // DEPLOY GEAR
  if (distance <= ALTITUDE_THRESHOLD && !gearDeployed) {
    deployLandingGear();
    return;
  }
  
  // CHECK GROUNDED
  if (distance < 6 && gearDeployed) {
    groundedCount++;
    if (groundedCount >= 3) {
      Serial.println("GROUNDED - STOPPING FAN");
      stopFan();
      groundedCount = 0;
      tone(PASSIVE_BUZZER_PIN, 1000, 500);
      // No LCD print - will show in main display
      return;
    }
  } else {
    groundedCount = 0;
  }
  
  // RETRACT GEAR
  if (distance > ALTITUDE_THRESHOLD && gearDeployed && fanRunning == false) {
    if (millis() - gearDeployTime >= MIN_GEAR_DOWN_TIME) {
      retractLandingGear();
    }
  }
  
  // ALTITUDE WARNINGS (no LCD, just tones)
  if (millis() - lastWarningTime > 1000) {
    if (distance < 15 && distance >= 6) {
      Serial.println("LOW ALTITUDE WARNING");
      tone(PASSIVE_BUZZER_PIN, 800, 100);
      lastWarningTime = millis();
    } else if (distance < 20 && distance >= 15) {
      Serial.println("APPROACHING GROUND");
      tone(PASSIVE_BUZZER_PIN, 600, 100);
      lastWarningTime = millis();
    }
  }
}
String passcode1 = "";
String passcode2 = "";
String correctPasscode = "";
bool systemStarted = false;

enum SystemState {
  STATE_PASSCODE_FIRST,
  STATE_PASSCODE_SECOND,
  STATE_PASSCODE_VERIFY,
  STATE_TEMP_SETTING,
  STATE_WAITING_START,
  STATE_RUNNING
};

SystemState currentState = STATE_PASSCODE_FIRST;

// ============================================================================
// MPU6050 FUNCTIONS (IMUU!!)
// ============================================================================

void initMPU6050() {
  Serial.println("Initializing MPU6050...");
  
  mpu.initialize();
  
  Wire.beginTransmission(0x68);
  int error = Wire.endTransmission();
  
  if (error == 0) {
    Serial.println("MPU6050 found on I2C bus!");
    
    Serial.println("Calibrating... Keep sensor FLAT and STILL!");
    lcd.clear();
    lcd.print("Calibrating MPU");
    lcd.setCursor(0,1);
    lcd.print("Keep FLAT!");
    
    calibrateMPU6050();
    
    Serial.println("Calibration complete!");
    mpuAvailable = true;
    delay(1000);
  } else {
    Serial.println("MPU6050 connection failed!");
    Serial.println("Continuing without flight control...");
    lcd.clear();
    lcd.print("MPU6050 Missing");
    lcd.setCursor(0,1);
    lcd.print("Continuing...");
    mpuAvailable = false;
    delay(2000);
  }
}

void calibrateMPU6050() {
  long ax_sum = 0, ay_sum = 0, az_sum = 0;
  long gx_sum = 0, gy_sum = 0, gz_sum = 0;
  int samples = 200;
  
  for(int i = 0; i < samples; i++) {
    int16_t ax, ay, az, gx, gy, gz;
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    
    ax_sum += ax;
    ay_sum += ay;
    az_sum += az;
    gx_sum += gx;
    gy_sum += gy;
    gz_sum += gz;
    
    delay(5);
  }
  
  ax_offset = ax_sum / samples;
  ay_offset = ay_sum / samples;
  az_offset = (az_sum / samples) - 16384;
  gx_offset = gx_sum / samples;
  gy_offset = gy_sum / samples;
  gz_offset = gz_sum / samples;
  
  Serial.print("Offsets: AX=");
  Serial.print(ax_offset);
  Serial.print(" AY=");
  Serial.print(ay_offset);
  Serial.print(" AZ=");
  Serial.println(az_offset);
}

void updateOrientation() {
  if (!mpuAvailable) return;
  
  int16_t ax, ay, az, gx, gy, gz;
  
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  
  ax -= ax_offset;
  ay -= ay_offset;
  az -= az_offset;
  gx -= gx_offset;
  gy -= gy_offset;
  gz -= gz_offset;
  
  // To Calculate raw pitch and roll
  float raw_pitch = atan2(ax, sqrt(ay*ay + az*az)) * 180.0 / PI;
  float raw_roll = atan2(ay, sqrt(ax*ax + az*az)) * 180.0 / PI;
  
  // Safety checks
  if (isnan(raw_pitch) || abs(raw_pitch) > 180) raw_pitch = pitch_filtered;
  if (isnan(raw_roll) || abs(raw_roll) > 180) raw_roll = roll_filtered;
  
  // Apply filtering
  pitch_filtered = (FILTER_ALPHA * raw_pitch) + ((1 - FILTER_ALPHA) * pitch_filtered);
  roll_filtered = (FILTER_ALPHA * raw_roll) + ((1 - FILTER_ALPHA) * roll_filtered);
  
  pitch = pitch_filtered;
  roll = roll_filtered;
  
  // YAW CALCULATION - prevent overflow and drift
  float dt = MPU_UPDATE_INTERVAL / 1000.0;
  float raw_yaw_rate = (gz / 131.0);
  
  // Dead zone to prevent drift when stationary
  if (abs(raw_yaw_rate) > 0.5) {
    yaw += raw_yaw_rate * dt;
  }
  
  // CRITICAL: Keeping yaw in valid range to prevent overflow
  while (yaw > 180) yaw -= 360;
  while (yaw < -180) yaw += 360;
  
  // Apply light filtering to yaw
  yaw_filtered = (0.05 * yaw) + (0.95 * yaw_filtered);
  
  // Keep filtered yaw in range too
  while (yaw_filtered > 180) yaw_filtered -= 360;
  while (yaw_filtered < -180) yaw_filtered += 360;
}

void displayOrientation() {
  if (!mpuAvailable) return;
  
  Serial.print("Pitch: ");
  Serial.print(pitch, 1);
  Serial.print(" deg | Roll: ");
  Serial.print(roll, 1);
  Serial.print(" deg | Yaw: ");
  Serial.print(yaw, 1);
  Serial.print(" deg | Alt: ");
  Serial.print(lastDistance, 1);
  Serial.print("cm | Gear: ");
  Serial.print(gearDeployed ? "DOWN" : "UP");
  Serial.print(" | Fan: ");
  Serial.print(fanRunning ? "ON" : "OFF");
  Serial.print(" | Nav: ");
  Serial.print(navLightsOn ? "ON" : "OFF");
  Serial.print(" | Status: ");
  
  if (pitch > STALL_WARNING_THRESHOLD) {
    Serial.println("STALL WARNING");
  } else if (pitch > NOSE_UP_THRESHOLD) {
    Serial.println("NOSE UP");
  } else if (pitch < NOSE_DOWN_THRESHOLD) {
    Serial.println("NOSE DOWN");
  } else if (roll > 30) {
    Serial.println("ROLLING RIGHT");
  } else if (roll < -30) {
    Serial.println("ROLLING LEFT");
  } else {
    Serial.println("LEVEL");
  }
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(9600);
  Wire.begin();
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("MEGA-1: Flight System + Landing Gear + Nav Lights [L293D Motor]");
  
  lcd.begin(16, 2);
  pinMode(ACTIVE_BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  pinMode(ULTRASONIC_TRIG, OUTPUT);
  pinMode(ULTRASONIC_ECHO, INPUT);
  
  pinMode(PASSIVE_BUZZER_PIN, OUTPUT);
  
  // L293D Motor pins
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_EN, OUTPUT);
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  analogWrite(MOTOR_EN, 0);
  Serial.println("L293D motor driver initialized - OFF");
  
  // Initialize Navigation Lights
  initNavLights();
  
  landingGearServo.attach(LANDING_GEAR_PIN);
  landingGearServo.write(GEAR_UP_ANGLE);
  gearDeployed = false;
  Serial.println("Landing gear initialized - UP");

  pinMode(SR_DATA, OUTPUT);
  pinMode(SR_CLOCK, OUTPUT);
  pinMode(SR_LATCH, OUTPUT);

  pinMode(DIGIT1, OUTPUT);
  pinMode(DIGIT2, OUTPUT);
  pinMode(DIGIT3, OUTPUT);
  pinMode(DIGIT4, OUTPUT);

  clearDigits();

  lcd.clear();
  lcd.print("MEGA-1 System");
  lcd.setCursor(0, 1);
  lcd.print("Motor+Gear+Light");
  delay(1500);
  
  initMPU6050();

  currentState = STATE_PASSCODE_FIRST;
  promptFirstPasscode();
}

// ============================================================================
// PASSCODE FUNCTIONS
// ============================================================================
void promptFirstPasscode() {
  lcd.clear();
  lcd.print("Enter Passcode:");
  lcd.setCursor(0,1);
  lcd.print("(4 digits)");
  passcode1 = "";
}

void handleFirstPasscode() {
  char key = keypad.getKey();
  if (!key) return;

  if (key >= '0' && key <= '9' && passcode1.length() < 4) {
    passcode1 += key;
    lcd.setCursor(0,1);
    for(int i=0;i<passcode1.length();i++) lcd.print("*");
    digitalWrite(ACTIVE_BUZZER_PIN, HIGH);
    delay(50);
    digitalWrite(ACTIVE_BUZZER_PIN, LOW);
    if (passcode1.length() == 4) {
      currentState = STATE_PASSCODE_SECOND;
      delay(300);
      promptSecondPasscode();
    }
  }
  if (key=='*') {
    passcode1 = "";
    lcd.setCursor(0,1);
    lcd.print("                ");
  }
}

void promptSecondPasscode() {
  lcd.clear();
  lcd.print("Confirm Code:");
  lcd.setCursor(0,1);
  lcd.print("(4 digits)");
  passcode2 = "";
}

void handleSecondPasscode() {
  char key = keypad.getKey();
  if (!key) return;

  if (key >= '0' && key <= '9' && passcode2.length() < 4) {
    passcode2 += key;
    lcd.setCursor(0,1);
    for(int i=0;i<passcode2.length();i++) lcd.print("*");
    digitalWrite(ACTIVE_BUZZER_PIN, HIGH);
    delay(50);
    digitalWrite(ACTIVE_BUZZER_PIN, LOW);
    if (passcode2.length() == 4) {
      currentState = STATE_PASSCODE_VERIFY;
    }
  }
  if (key=='*') {
    passcode2 = "";
    lcd.setCursor(0,1);
    lcd.print("                ");
  }
}

void verifyPasscodes() {
  if (passcode1 == passcode2) {
    correctPasscode = passcode1;
    lcd.clear();
    lcd.print("Match!");
    digitalWrite(ACTIVE_BUZZER_PIN, HIGH);
    delay(1000);
    digitalWrite(ACTIVE_BUZZER_PIN, LOW);
    flashPasscodeOn7seg();
    currentState = STATE_TEMP_SETTING;
    promptTemperatureSetting();
  } else {
    lcd.clear();
    lcd.print("Mismatch!");
    delay(1000);
    passcode1 = "";
    passcode2 = "";
    currentState = STATE_PASSCODE_FIRST;
    promptFirstPasscode();
  }
}

void flashPasscodeOn7seg() {
  unsigned long endTime = millis() + 5000;
  while (millis() < endTime) {
    unsigned long showUntil = millis() + 800;
    while (millis() < showUntil) {
      for(int d=0; d<4; d++) {
        showDigit(correctPasscode[d]-'0', d);
        delay(3);
      }
    }
    clearDigits();
    delay(200);
  }
}

String tempInput = "";

void promptTemperatureSetting() {
  lcd.clear();
  lcd.print("Set Room Temp:");
  lcd.setCursor(0,1);
  lcd.print("3 digits + #");
}

void handleTemperatureSetting() {
  char key = keypad.getKey();
  if (!key) return;

  if (key>='0' && key<='9' && tempInput.length()<3) {
    tempInput += key;
    lcd.setCursor(tempInput.length()+4,1);
    lcd.print(key);
  }
  if (key=='*') {
    tempInput="";
    lcd.setCursor(0,1);
    lcd.print("3 digits + #    ");
  }
  if (key=='#' && tempInput.length()==3) {
    preferredTemp = tempInput.toInt() / 10.0;
    lcd.clear();
    lcd.print("Set: ");
    lcd.print(preferredTemp,1);
    lcd.print("C");
    delay(1000);
    currentState = STATE_WAITING_START;
    promptWaitingForStart();
  }
}

void promptWaitingForStart() {
  lcd.clear();
  lcd.print("Press Button");
  lcd.setCursor(0,1);
  lcd.print("to Start");
}

void handleWaitingForStart() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(100);
    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("Button pressed");
      Serial.print("systemStarted = ");
      Serial.println(systemStarted ? "true" : "false");
      if (systemStarted) {
        Serial.println("Calling resetToPasscode()");
        resetToPasscode();
      } else {
        Serial.println("Calling startSystem()");
        startSystem();
      }
    }
  }
}

void startSystem() {
  systemStarted = true;
  currentState = STATE_RUNNING;
  lcd.clear();
  lcd.print("System is ON");
  lcd.setCursor(0,1);
  lcd.print("Taking Off!");
  
  startFan();
  turnOnNavLights();  // Turn on navigation lights when system starts
  
  if (gearDeployed) {
    retractLandingGear();
  }
  
  tone(PASSIVE_BUZZER_PIN, 1000, 200);
  
  Wire.beginTransmission(8);
  Wire.write(1); 
  Wire.write((byte*)&preferredTemp, sizeof(preferredTemp));
  Wire.endTransmission();
  delay(1500);
}
void handleRunningState() {
  updateNavLights();
  
  if (mpuAvailable && millis() - lastMPUUpdate >= MPU_UPDATE_INTERVAL) {
    updateOrientation();
    displayOrientation();
    handlePitchBuzzer(pitch);
    handleRollBuzzer(roll);
    lastMPUUpdate = millis();
  }
  
  checkIMUHealth();
  checkLandingAltitude();
  
  static unsigned long lastLCDUpdate = 0;
  static bool showPitchRoll = true;
  
  if (millis() - lastLCDUpdate >= 2000) {
    lcd.clear();  
    
    if (showPitchRoll && mpuAvailable) {
      // Show flight data with fixed formatting
      lcd.setCursor(0, 0);
      
      // Format: "P:XXX R:XXX Y:XXX"
      char line1[17];
      sprintf(line1, "P:%-4d R:%-4d Y:%-4d", (int)pitch, (int)roll, (int)yaw);
      lcd.print(line1);
      
      lcd.setCursor(0, 1);
      
      // Format: "Alt:XXXcm GR:XX"
      char line2[17];
      sprintf(line2, "Alt:%-4dcm %s", (int)lastDistance, gearDeployed ? "GR:DN" : "GR:UP");
      lcd.print(line2);
      
    } else {
      // Show system status
      lcd.setCursor(0, 0);
      
      char line1[17];
      sprintf(line1, "Fan:%-3s Nav:%-3s", fanRunning ? "ON" : "OFF", navLightsOn ? "ON" : "OFF");
      lcd.print(line1);
      
      lcd.setCursor(0, 1);
      
      char line2[17];
      sprintf(line2, "T:%.1fC Alt:%-4d", preferredTemp, (int)lastDistance);
      lcd.print(line2);
    }
    
    showPitchRoll = !showPitchRoll;
    lastLCDUpdate = millis();
  }
  
  delay(10);
}
//======================================================================
//RESET 
//=================================================================
void resetToPasscode() {
  stopFan();
  turnOffNavLights();  // Turn off navigation lights on reset
  landingGearServo.write(GEAR_UP_ANGLE);
  gearDeployed = false;

  passcode1 = "";
  passcode2 = "";
  correctPasscode = "";
  tempInput = "";
  systemStarted = false;
  preferredTemp = 0.0;
  smoothedDistance = 999.0;
  lastDistance = 999.0;
  groundedCount = 0;
  pitch = roll = yaw = 0;
  pitch_filtered = roll_filtered = yaw_filtered = 0;

  currentState = STATE_PASSCODE_FIRST;
  lcd.clear();
  lcd.print("System RESET");
  lcd.setCursor(0,1);
  lcd.print("POWER pressed");
  delay(1000);

  promptFirstPasscode();
}

// Add this new function before loop()
void checkIMUHealth() {
  if (!mpuAvailable) return;
  
  if (millis() - lastIMUHealthCheck < 5000) return;
  lastIMUHealthCheck = millis();
  
  // Check for stuck values
  static float lastPitch = 0;
  static float lastRoll = 0;
  static int unchangedCount = 0;
  
  if (abs(pitch - lastPitch) < 0.1 && abs(roll - lastRoll) < 0.1) {
    unchangedCount++;
    if (unchangedCount > 3) {
      Serial.println("IMU appears stuck - resetting filters");
      pitch_filtered = pitch;
      roll_filtered = roll;
      yaw_filtered = yaw;
      unchangedCount = 0;
    }
  } else {
    unchangedCount = 0;
  }
  
  lastPitch = pitch;
  lastRoll = roll;
  
  // Reset yaw drift periodically when grounded and stable
  if (gearDeployed && !fanRunning && abs(pitch) < 5 && abs(roll) < 5) {
    if (abs(yaw) > 10) {
      Serial.println("Resetting yaw drift while grounded");
      yaw = 0;
      yaw_filtered = 0;
    }
  }
  
  Serial.print("IMU Health: P=");
  Serial.print(pitch, 1);
  Serial.print(" R=");
  Serial.print(roll, 1);
  Serial.print(" Y=");
  Serial.println(yaw, 1);
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {

  // Check for IR signals regardless of state
  if (IrReceiver.decode()) {
    uint8_t cmd = IrReceiver.decodedIRData.command;

    if (cmd == 0x45) {  // POWER button code
      resetToPasscode();
      IrReceiver.resume();
      return;  // Skip rest of loop to avoid conflict
    }
    IrReceiver.resume();
  }

  switch(currentState) {
    case STATE_PASSCODE_FIRST:
      handleFirstPasscode();
      break;
    case STATE_PASSCODE_SECOND:
      handleSecondPasscode();
      break;
    case STATE_PASSCODE_VERIFY:
      verifyPasscodes();
      break;
    case STATE_TEMP_SETTING:
      handleTemperatureSetting();
      break;
    case STATE_WAITING_START:
      handleWaitingForStart();
      break;
    case STATE_RUNNING:
      handleRunningState();
      break;
  }
}