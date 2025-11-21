// ============================================================================
// MEGA-1: FLIGHT CONTROL UNIT [MPU6050 - GY512]
// ============================================================================

#include <LiquidCrystal.h>
#include <Keypad.h>
#include <Wire.h>
#include <MPU6050.h>

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
const int PASSIVE_BUZZER_PIN = 13;  // Passive buzzer for tones (PWM pin)
const int BUTTON_PIN = 18;

// ========== ULTRASONIC SENSOR ==========
const int ULTRASONIC_TRIG = 6;
const int ULTRASONIC_ECHO = 7;

// ========== SERVO MOTOR (Propeller - Continuous Rotation) ==========
#include <Servo.h>
Servo propellerServo;
const int SERVO_PIN = 31;  // PWM pin, unused
int servoSpeed = 90;  // 0-180: 0=full reverse, 90=stop, 180=full forward

// ========== I2C ==========
float preferredTemp = 0.0;

// ========== MPU6050 SENSOR ==========
MPU6050 mpu;

float pitch = 0;
float roll = 0;
float yaw = 0;

unsigned long lastMPUUpdate = 0;
const int MPU_UPDATE_INTERVAL = 1000;  // Very slow: 1 second

int16_t ax_offset = 0, ay_offset = 0, az_offset = 0;
int16_t gx_offset = 0, gy_offset = 0, gz_offset = 0;

float pitch_filtered = 0;
float roll_filtered = 0;
float yaw_filtered = 0;
const float FILTER_ALPHA = 0.15; // Much stronger filter (only 15% new data)

// ============================================================================
// BUZZER FEEDBACK
// ============================================================================
unsigned long lastBuzzerTime = 0;
const int BUZZER_DEBOUNCE = 500;

const float NOSE_DOWN_THRESHOLD = -20.0;
const float NOSE_UP_THRESHOLD = 20.0;
const float STALL_WARNING_THRESHOLD = 40.0;

void buzzNoseDown() {
  // Low tone: do (262 Hz)
  tone(PASSIVE_BUZZER_PIN, 262, 300);
  delay(400);
}

void buzzNoseUp() {
  // High tone: sol (392 Hz) - two beeps
  tone(PASSIVE_BUZZER_PIN, 392, 150);
  delay(200);
  tone(PASSIVE_BUZZER_PIN, 392, 150);
  delay(200);
}

void buzzStallWarning() {
  // Siren: alternating tones
  for(int i = 0; i < 4; i++) {
    tone(PASSIVE_BUZZER_PIN, 800, 100);
    delay(150);
    tone(PASSIVE_BUZZER_PIN, 600, 100);
    delay(150);
  }
}

void buzzRoll() {
  // Three quick tones: re-mi-re (294-330-294 Hz)
  tone(PASSIVE_BUZZER_PIN, 294, 80);
  delay(120);
  tone(PASSIVE_BUZZER_PIN, 330, 80);
  delay(120);
  tone(PASSIVE_BUZZER_PIN, 294, 80);
  delay(120);
}

void handlePitchBuzzer(float currentPitch) {
  unsigned long now = millis();
  
  if (now - lastBuzzerTime < BUZZER_DEBOUNCE) {
    return;
  }
  
  if (currentPitch > STALL_WARNING_THRESHOLD) {
    buzzStallWarning();
    lastBuzzerTime = now;
    Serial.println("!!! STALL WARNING !!!");
    return;
  }
  
  if (currentPitch > NOSE_UP_THRESHOLD && currentPitch <= STALL_WARNING_THRESHOLD) {
    buzzNoseUp();
    lastBuzzerTime = now;
    Serial.println(">> NOSE UP <<");
    return;
  }
  
  if (currentPitch < NOSE_DOWN_THRESHOLD) {
    buzzNoseDown();
    lastBuzzerTime = now;
    Serial.println(">> NOSE DOWN <<");
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
      Serial.println(">> ROLLING RIGHT <<");
    } else {
      Serial.println(">> ROLLING LEFT <<");
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
// ULTRASONIC SENSOR FOR LANDING
// ============================================================================

float getUltrasonicDistance() {
  // Send trigger pulse
  digitalWrite(ULTRASONIC_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG, LOW);
  
  // Measure echo time
  long duration = pulseIn(ULTRASONIC_ECHO, HIGH, 30000);
  
  // Convert to cm (speed of sound = 343 m/s)
  float distance = (duration * 0.0343) / 2;
  
  return distance;
}

unsigned long lastUltrasonicCheck = 0;
float lastDistance = 999;

void checkLandingAltitude() {
  if (millis() - lastUltrasonicCheck < 200) {
    return;  // Check every 200ms
  }
  lastUltrasonicCheck = millis();
  
  float distance = getUltrasonicDistance();
  
  // Only trigger alerts if distance is valid (below 400cm)
  if (distance < 400 && distance > 5) {
    
    if (distance < 10) {
      Serial.println("!!! PULL UP PULL UP !!!");
      tone(PASSIVE_BUZZER_PIN, 800, 100);
      delay(100);
      tone(PASSIVE_BUZZER_PIN, 800, 100);
      delay(100);
      servoSpeed = 180;  // Full throttle
    } else if (distance < 20) {
      Serial.println("PULL UP PULL UP");
      tone(PASSIVE_BUZZER_PIN, 700, 150);
      delay(100);
      servoSpeed = 160;
    } else if (distance < 30) {
      Serial.println("RETARD 30");
      tone(PASSIVE_BUZZER_PIN, 600, 200);
      delay(100);
      servoSpeed = 120;
    } else if (distance < 40) {
      Serial.println("RETARD 40");
      tone(PASSIVE_BUZZER_PIN, 500, 200);
      delay(100);
      servoSpeed = 90;
    } else if (distance < 50) {
      Serial.print("Ground detected: ");
      Serial.print(distance, 1);
      Serial.println(" cm");
      servoSpeed = 50;  // Reduced speed
    }
    
    lastDistance = distance;
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
// MPU6050 FUNCTIONS
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
    delay(1000);
  } else {
    Serial.println("MPU6050 connection failed!");
    lcd.clear();
    lcd.print("MPU6050 Error!");
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
  az_offset = (az_sum / samples) - 16384; // Remove gravity from Z
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
  int16_t ax, ay, az, gx, gy, gz;
  
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  
  // Apply calibration offsets
  ax -= ax_offset;
  ay -= ay_offset;
  az -= az_offset;
  gx -= gx_offset;
  gy -= gy_offset;
  gz -= gz_offset;
  
  // CORRECT AXIS MAPPING (chip mounted perpendicular)
  // Pitch: nose up/down (use AX for forward/backward tilt)
  float raw_pitch = atan2(ax, sqrt(ay*ay + az*az)) * 180.0 / PI;
  
  // Roll: wing up/down (use AY for left/right tilt)
  float raw_roll = atan2(ay, sqrt(ax*ax + az*az)) * 180.0 / PI;
  
  if (isnan(raw_pitch)) raw_pitch = 0;
  if (isnan(raw_roll)) raw_roll = 0;
  
  // Apply low-pass filter
  pitch_filtered = (FILTER_ALPHA * raw_pitch) + ((1 - FILTER_ALPHA) * pitch_filtered);
  roll_filtered = (FILTER_ALPHA * raw_roll) + ((1 - FILTER_ALPHA) * roll_filtered);
  
  pitch = pitch_filtered;
  roll = roll_filtered;
  
  // Yaw from gyroscope - ONLY UPDATE IF NOT DRIFTING
  float dt = MPU_UPDATE_INTERVAL / 1000.0;
  float raw_yaw_rate = (gz / 131.0) * dt;
  
  // Only update yaw if rotation is significant (> 2 degrees/sec)
  if (abs(raw_yaw_rate) > 0.5) {
    yaw += raw_yaw_rate;
  }
  
  // Apply filter to yaw too
  yaw_filtered = (FILTER_ALPHA * yaw) + ((1 - FILTER_ALPHA) * yaw_filtered);
  yaw = yaw_filtered;
  
  if (yaw > 180) yaw -= 360;
  if (yaw < -180) yaw += 360;
}

void displayOrientation() {
  Serial.print("Pitch: ");
  Serial.print(pitch, 1);
  Serial.print("° | Roll: ");
  Serial.print(roll, 1);
  Serial.print("° | Yaw: ");
  Serial.print(yaw, 1);
  Serial.print("° | Status: ");
  
  if (pitch > STALL_WARNING_THRESHOLD) {
    Serial.println("*** STALL WARNING ***");
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
  Serial.println("Hello Mega1 with MPU6050");
  
  lcd.begin(16, 2);
  pinMode(ACTIVE_BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Ultrasonic sensor pins
  pinMode(ULTRASONIC_TRIG, OUTPUT);
  pinMode(ULTRASONIC_ECHO, INPUT);
  
  // Passive buzzer pin
  pinMode(PASSIVE_BUZZER_PIN, OUTPUT);
  
  // Servo motor
  propellerServo.attach(SERVO_PIN);
  propellerServo.write(0);  // Start at 0 degrees
  
  // Passive buzzer pin
  pinMode(PASSIVE_BUZZER_PIN, OUTPUT);
  
  // Servo motor
  propellerServo.attach(SERVO_PIN);
  propellerServo.write(0);  // Start at 0 degrees

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
  delay(1500);
  
  initMPU6050();

  currentState = STATE_PASSCODE_FIRST;
  promptFirstPasscode();
}

// ============================================================================
// PASSCODE FUNCTIONS (unchanged)
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
  if (digitalRead(BUTTON_PIN)==LOW) {
    delay(50);
    if (digitalRead(BUTTON_PIN)==LOW) startSystem();
  }
}

void startSystem() {
  systemStarted = true;
  currentState = STATE_RUNNING;
  lcd.clear();
  lcd.print("System is ON");
  
  // Start propeller spinning continuously at full speed
  servoSpeed = 180;
  
  Wire.beginTransmission(8);
  Wire.write((byte*)&preferredTemp, sizeof(preferredTemp));
  Wire.endTransmission();
  delay(1000);
}

void handleRunningState() {
  if (millis() - lastMPUUpdate >= MPU_UPDATE_INTERVAL) {
    updateOrientation();
    displayOrientation();
    handlePitchBuzzer(pitch);
    handleRollBuzzer(roll);
    lastMPUUpdate = millis();
  }
  
  // Check ultrasonic sensor for landing
  checkLandingAltitude();
  
  // Update servo position
  propellerServo.write(servoSpeed);
  
  static unsigned long lastLCDUpdate = 0;
  static bool showPitchRoll = true;
  
  if (millis() - lastLCDUpdate >= 2000) {
    lcd.clear();
    if (showPitchRoll) {
      lcd.print("P:");
      lcd.print(pitch, 1);
      lcd.print(" R:");
      lcd.print(roll, 1);
      lcd.setCursor(0,1);
      lcd.print("Yaw:");
      lcd.print(yaw, 1);
    } else {
      lcd.print("Temp:");
      lcd.print(preferredTemp, 1);
      lcd.print("C");
      lcd.setCursor(0,1);
      lcd.print("Prop:");
      lcd.print(servoSpeed);
      lcd.print("%");
    }
    showPitchRoll = !showPitchRoll;
    lastLCDUpdate = millis();
  }
  
  delay(10);
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
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