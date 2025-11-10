// ============================================================================
// This is UNO-1 code on simulation its named two: USER INTERFACE CONTROLLER 
// Simplified for Arduino UNO pin limitations wil have to change the pins as we move to mega 
// ============================================================================

#include <LiquidCrystal.h>
#include <Keypad.h>
#include <Wire.h>

// ===== PIN DEFINITIONS FOR UNO =====
// LCD Display (standard 4-bit mode)
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// 4x4 Keypad - ADJUSTED FOR UNO
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
// UNO-compatible pins (avoid I2C pins 18/19 = A4/A5)
byte rowPins[ROWS] = {A0, A1, A2, A3}; // Use analog pins as digital
byte colPins[COLS] = {6, 7, 8, 9};

// Other components
const int BUZZER_PIN = 10;     // Active buzzer
const int BUTTON_PIN = 13;     // Push button (with built-in LED)
const int LED_PIN = 13;        // Built-in LED for 7-segment simulation

// ===== COMPONENT INITIALIZATION =====
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ===== SYSTEM VARIABLES =====
String passcode1 = "";
String passcode2 = "";
String correctPasscode = "";
float preferredTemp = 0.0;
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

void setup() {
  Serial.begin(9600);
  Wire.begin(); // I2C Master mode
  
  // Initialize components
  lcd.begin(16, 2);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  
  Serial.println("=== UNO-1: USER INTERFACE ===");
  Serial.println("Two-UNO Project - Master");
  Serial.println("");
  
  // Welcome message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("UNO-1 System");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(2000);
  
  Serial.println("UNO-1 Initialized");
  Serial.println("Starting passcode entry");
  Serial.println("========================");
  
  // Start passcode entry
  currentState = STATE_PASSCODE_FIRST;
  promptFirstPasscode();
}

void loop() {
  switch (currentState) {
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
  
  delay(100);
}

// ===== PASSCODE ENTRY: FIRST ATTEMPT =====
void promptFirstPasscode() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter Passcode:");
  lcd.setCursor(0, 1);
  lcd.print("(4 digits)");
  
  Serial.println("Enter first passcode (4 digits)");
  passcode1 = "";
}

void handleFirstPasscode() {
  char key = keypad.getKey();
  
  if (key) {
    if (key >= '0' && key <= '9' && passcode1.length() < 4) {
      passcode1 += key;
      
      // Display asterisks for security
      lcd.setCursor(0, 1);
      for (int i = 0; i < passcode1.length(); i++) {
        lcd.print("*");
      }
      
      Serial.print("Digit entered: ");
      Serial.println(key);
      
      // Beep feedback
      digitalWrite(BUZZER_PIN, HIGH);
      delay(50);
      digitalWrite(BUZZER_PIN, LOW);
      
      // Move to second passcode if 4 digits entered
      if (passcode1.length() == 4) {
        Serial.print("First passcode: ");
        Serial.println(passcode1);
        delay(500);
        currentState = STATE_PASSCODE_SECOND;
        promptSecondPasscode();
      }
    }
    
    // Clear button
    if (key == '*' && passcode1.length() > 0) {
      passcode1 = "";
      lcd.setCursor(0, 1);
      lcd.print("                ");
      Serial.println("Cleared");
    }
  }
}

// ===== PASSCODE ENTRY: SECOND ATTEMPT =====
void promptSecondPasscode() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Confirm Code:");
  lcd.setCursor(0, 1);
  lcd.print("(4 digits)");
  
  Serial.println("Confirm passcode");
  passcode2 = "";
}

void handleSecondPasscode() {
  char key = keypad.getKey();
  
  if (key) {
    if (key >= '0' && key <= '9' && passcode2.length() < 4) {
      passcode2 += key;
      
      // Display asterisks
      lcd.setCursor(0, 1);
      for (int i = 0; i < passcode2.length(); i++) {
        lcd.print("*");
      }
      
      Serial.print("Digit entered: ");
      Serial.println(key);
      
      // Beep feedback
      digitalWrite(BUZZER_PIN, HIGH);
      delay(50);
      digitalWrite(BUZZER_PIN, LOW);
      
      // Verify if 4 digits entered
      if (passcode2.length() == 4) {
        Serial.print("Second passcode: ");
        Serial.println(passcode2);
        delay(500);
        currentState = STATE_PASSCODE_VERIFY;
      }
    }
    
    // Clear button
    if (key == '*' && passcode2.length() > 0) {
      passcode2 = "";
      lcd.setCursor(0, 1);
      lcd.print("                ");
      Serial.println("Cleared");
    }
  }
}

// ===== PASSCODE VERIFICATION =====
void verifyPasscodes() {
  if (passcode1 == passcode2 && passcode1.length() == 4) {
    // SUCCESS
    correctPasscode = passcode1;
    
    Serial.println("PASSCODE MATCH!");
    Serial.print("Passcode: ");
    Serial.println(correctPasscode);
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Match! Success!");
    lcd.setCursor(0, 1);
    lcd.print(correctPasscode);
    
    // Activate buzzer for 1 second
    digitalWrite(BUZZER_PIN, HIGH);
    delay(1000);
    digitalWrite(BUZZER_PIN, LOW);
    
    // Flash LED (simulating 7-segment)
    flashPasscodeOnLED();
    
    // Move to temperature setting
    currentState = STATE_TEMP_SETTING;
    promptTemperatureSetting();
    
  } else {
    // FAILURE
    Serial.println("✗ MISMATCH!");
    Serial.println("Restarting...");
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Mismatch!");
    lcd.setCursor(0, 1);
    lcd.print("Try Again...");
    
    // Error beeps
    for (int i = 0; i < 3; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(200);
      digitalWrite(BUZZER_PIN, LOW);
      delay(200);
    }
    
    delay(2000);
    
    // Restart
    passcode1 = "";
    passcode2 = "";
    currentState = STATE_PASSCODE_FIRST;
    promptFirstPasscode();
  }
}

// ===== LED FLASH (Simulating 7-Segment) =====
void flashPasscodeOnLED() {
  Serial.println("Flashing passcode on LED...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Code Display:");
  
  // Flash 5 times
  for (int i = 0; i < 5; i++) {
    // Show passcode
    lcd.setCursor(0, 1);
    lcd.print(correctPasscode);
    digitalWrite(LED_PIN, HIGH);
    
    Serial.print("Flash ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(correctPasscode);
    
    delay(1000);
    
    // Clear
    lcd.setCursor(0, 1);
    lcd.print("    ");
    digitalWrite(LED_PIN, LOW);
    
    delay(1000);
  }
  
  Serial.println("Flash complete");
}

// ===== TEMPERATURE SETTING =====
void promptTemperatureSetting() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Room Temp:");
  lcd.setCursor(0, 1);
  lcd.print("3 digits + #");
  
  Serial.println("Enter temp (3 digits + #)");
  Serial.println("Example: 235# = 23.5C");
}

String tempInput = "";

void handleTemperatureSetting() {
  char key = keypad.getKey();
  
  if (key) {
    // Number input
    if (key >= '0' && key <= '9' && tempInput.length() < 3) {
      tempInput += key;
      lcd.setCursor(tempInput.length() + 4, 1);
      lcd.print(key);
      
      Serial.print("Temp digit: ");
      Serial.println(key);
      
      // Beep
      digitalWrite(BUZZER_PIN, HIGH);
      delay(50);
      digitalWrite(BUZZER_PIN, LOW);
    }
    
    // Confirm with #
    if (key == '#' && tempInput.length() == 3) {
      int tempInt = tempInput.toInt();
      preferredTemp = tempInt / 10.0;
      
      Serial.print("Temperature set: ");
      Serial.print(preferredTemp, 1);
      Serial.println("°C");
      
      displayTemperature();
      
      currentState = STATE_WAITING_START;
      promptWaitingForStart();
    }
    
    // Clear with *
    if (key == '*') {
      tempInput = "";
      lcd.setCursor(0, 1);
      lcd.print("3 digits + #    ");
      Serial.println("Cleared");
    }
  }
}

void displayTemperature() {
  float tempF = (preferredTemp * 9.0/5.0) + 32.0;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set: ");
  lcd.print(preferredTemp, 1);
  lcd.print("C");
  
  lcd.setCursor(0, 1);
  lcd.print("    ");
  lcd.print(tempF, 1);
  lcd.print("F");
  
  Serial.print("Display: ");
  Serial.print(preferredTemp, 1);
  Serial.print("°C / ");
  Serial.print(tempF, 1);
  Serial.println("°F");
  
  delay(3000);
}

// ===== WAITING FOR START BUTTON =====
void promptWaitingForStart() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Press Button");
  lcd.setCursor(0, 1);
  lcd.print("to Start");
  
  Serial.println("Press button to start");
}

void handleWaitingForStart() {
  if (digitalRead(BUTTON_PIN) == LOW) { // Button pressed (active low with pull-up)
    delay(50); // Debounce
    if (digitalRead(BUTTON_PIN) == LOW) {
      startSystem();
    }
  }
}

// ===== SYSTEM START =====
void startSystem() {
  systemStarted = true;
  currentState = STATE_RUNNING;
  
  Serial.println("*** SYSTEM STARTING ***");
  Serial.println("Sending to UNO-2...");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System is ON");
  lcd.setCursor(0, 1);
  lcd.print("Sending data...");
  
  // Send to UNO-2 via I2C
  sendDataToUno2();
  
  delay(2000);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Active");
  lcd.setCursor(0, 1);
  lcd.print("Monitoring...");
  
  Serial.println("UNO-1 complete");
  Serial.println("==================");
}

// ===== I2C COMMUNICATION TO UNO-2 =====
void sendDataToUno2() {
  Wire.beginTransmission(8); // UNO-2 address
  Wire.write((byte*)&preferredTemp, sizeof(preferredTemp));
  Wire.endTransmission();
  
  Serial.print("I2C: Sent ");
  Serial.print(preferredTemp, 1);
  Serial.println("°C to UNO-2");
}

// ===== RUNNING STATE =====
void handleRunningState() {
  static unsigned long lastUpdate = 0;
  
  if (millis() - lastUpdate > 5000) {
    lastUpdate = millis();
    
    Serial.println("=== UNO-1 STATUS ===");
    Serial.print("Passcode: ");
    Serial.println(correctPasscode);
    Serial.print("Preferred: ");
    Serial.print(preferredTemp, 1);
    Serial.println("°C");
    Serial.println("Status: Running");
    Serial.println("===================");
  }
  
  delay(100);
}