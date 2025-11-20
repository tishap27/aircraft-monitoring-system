// ============================================================================
// MEGA-1: USER INTERFACE CONTROLLER + 4-DIGIT 7-SEGMENT (74HC595)
// ============================================================================

#include <LiquidCrystal.h>
#include <Keypad.h>
#include <Wire.h>

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
const int BUTTON_PIN = 18;

// ========== I2C ==========
float preferredTemp = 0.0;

// ============================================================================
// 7-SEGMENT (COMMON CATHODE) + 74HC595
// ============================================================================

// Shift Register Pins
#define SR_DATA   8   // DS (Pin 14)
#define SR_CLOCK 10   // SH_CP (Pin 11)
#define SR_LATCH  9   // ST_CP (Pin 12)

// Digit select pins (common cathodes)
#define DIGIT1 23
#define DIGIT2 24
#define DIGIT3 25
#define DIGIT4 26

// Standard segment order for common cathode: A B C D E F G DP
byte segmentMap[10] = {
  // DP G F E D C B A
  B00111111, // 0
  B00000110, // 1
  B01011011, // 2
  B01001111, // 3
  B01100110, // 4
  B01101101, // 5
  B01111101, // 6
  B00000111, // 7
  B01111111, // 8
  B01101111  // 9
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
// SYSTEM STATE VARIABLES
// ============================================================================
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
// SETUP
// ============================================================================
void setup() {
  Serial.begin(9600);
  Wire.begin();

  lcd.begin(16, 2);
  pinMode(ACTIVE_BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

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

  currentState = STATE_PASSCODE_FIRST;
  promptFirstPasscode();
}

// ============================================================================
// FIRST PASSCODE ENTRY
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

// ============================================================================
// SECOND PASSCODE ENTRY
// ============================================================================
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

// ============================================================================
// VERIFY PASSCODES
// ============================================================================
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

// ============================================================================
// FLASH PASSCODE ON 7-SEG (5 sec, 1 sec interval)
// ============================================================================
void flashPasscodeOn7seg() {
  unsigned long endTime = millis() + 5000;

  while (millis() < endTime) {

    // Show digits for ~800ms
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

// ============================================================================
// TEMPERATURE SETTING
// ============================================================================
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

// ============================================================================
// WAIT FOR BUTTON
// ============================================================================
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

// ============================================================================
// START SYSTEM
// ============================================================================
void startSystem() {
  systemStarted = true;
  currentState = STATE_RUNNING;

  lcd.clear();
  lcd.print("System is ON");

  Wire.beginTransmission(8);
  Wire.write((byte*)&preferredTemp, sizeof(preferredTemp));
  Wire.endTransmission();

  delay(1000);
}

// ============================================================================
// RUNNING MODE
// ============================================================================
void handleRunningState() {
  delay(100);
}// ============================================================================
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

  // 7-segment refresh (if you want it always ON, uncomment section below)
  /*
  if (correctPasscode.length() == 4) {
    for(int d = 0; d < 4; d++) {
      showDigit(correctPasscode[d] - '0', d);
      delay(2);
    }
  }
  */
}

//git tf changes made

