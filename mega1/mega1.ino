// ============================================================================
// MEGA-1: USER INTERFACE CONTROLLER
// Ready for Arduino MEGA 2560 - All pins optimized
// ============================================================================

#include <LiquidCrystal.h>
#include <Keypad.h>
#include <Wire.h>

// ===== PIN DEFINITIONS FOR MEGA 2560 =====
// LCD Display (standard 4-bit mode) - NO CHANGES FROM UNO
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// 4x4 Keypad - CHANGED TO HIGHER MEGA PINS
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
// MEGA-compatible pins (using high-number pins to avoid conflicts)
byte rowPins[ROWS] = {39, 41, 43, 45}; // Rows on MEGA
byte colPins[COLS] = {47, 49, 51, 53}; // Cols on MEGA

// Other components - CHANGED TO BETTER MEGA PINS
const int ACTIVE_BUZZER_PIN = 22;  // Active buzzer (moved from pin 10)
const int BUTTON_PIN = 18;         // Push button on interrupt pin (was 13)
const int LED_PIN = 13;            // Built-in LED (keep for 7-seg simulation)

// I2C pins are AUTOMATIC on MEGA:
// Pin 20 = SDA (connect to MEGA-2 Pin 20)
// Pin 21 = SCL (connect to MEGA-2 Pin 21)

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
  pinMode(ACTIVE_BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  
  Serial.println("╔════════════════════════════════════╗");
  Serial.println("║   MEGA-1: USER INTERFACE CONTROL   ║");
  Serial.println("║      Arduino MEGA 2560 Master      ║");
  Serial.println("╚════════════════════════════════════╝");
  Serial.println("");
  
  // Welcome message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MEGA-1 System");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(2000);
  
  Serial.println("✓ MEGA-1 Initialized");
  Serial.println("✓ LCD Display Ready");
  Serial.println("✓ Keypad Ready");
  Serial.println("✓ I2C Master Ready");
  Serial.println("");
  Serial.println("Starting passcode entry sequence");
  Serial.println("====================================");
  
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
  
  Serial.println("\n>>> Enter first passcode (4 digits)");
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
      
      Serial.print("    Digit entered: ");
      Serial.println(key);
      
      // Beep feedback
      digitalWrite(ACTIVE_BUZZER_PIN, HIGH);
      delay(50);
      digitalWrite(ACTIVE_BUZZER_PIN, LOW);
      
      // Move to second passcode if 4 digits entered
      if (passcode1.length() == 4) {
        Serial.print("✓ First passcode complete: ");
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
      Serial.println("    Cleared - re-enter");
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
  
  Serial.println("\n>>> Confirm passcode (enter again)");
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
      
      Serial.print("    Digit entered: ");
      Serial.println(key);
      
      // Beep feedback
      digitalWrite(ACTIVE_BUZZER_PIN, HIGH);
      delay(50);
      digitalWrite(ACTIVE_BUZZER_PIN, LOW);
      
      // Verify if 4 digits entered
      if (passcode2.length() == 4) {
        Serial.print("✓ Second passcode complete: ");
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
      Serial.println("    Cleared - re-enter");
    }
  }
}

// ===== PASSCODE VERIFICATION =====
void verifyPasscodes() {
  if (passcode1 == passcode2 && passcode1.length() == 4) {
    // SUCCESS
    correctPasscode = passcode1;
    
    Serial.println("\n✅ PASSCODE MATCH!");
    Serial.print("✓ Passcode set to: ");
    Serial.println(correctPasscode);
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Match! Success!");
    lcd.setCursor(0, 1);
    lcd.print(correctPasscode);
    
    // Activate buzzer for 1 second
    digitalWrite(ACTIVE_BUZZER_PIN, HIGH);
    delay(1000);
    digitalWrite(ACTIVE_BUZZER_PIN, LOW);
    
    // Flash LED (simulating 7-segment)
    flashPasscodeOnLED();
    
    // Move to temperature setting
    currentState = STATE_TEMP_SETTING;
    promptTemperatureSetting();
    
  } else {
    // FAILURE
    Serial.println("\n❌ PASSCODE MISMATCH!");
    Serial.println("⟳ Restarting passcode entry...");
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Mismatch!");
    lcd.setCursor(0, 1);
    lcd.print("Try Again...");
    
    // Error beeps
    for (int i = 0; i < 3; i++) {
      digitalWrite(ACTIVE_BUZZER_PIN, HIGH);
      delay(200);
      digitalWrite(ACTIVE_BUZZER_PIN, LOW);
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
  Serial.println("\n⚡ Flashing passcode on 7-segment display...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Code Display:");
  
  // Flash 5 times
  for (int i = 0; i < 5; i++) {
    // Show passcode
    lcd.setCursor(0, 1);
    lcd.print(correctPasscode);
    digitalWrite(LED_PIN, HIGH);
    
    Serial.print("   Flash ");
    Serial.print(i + 1);
    Serial.print("/5: ");
    Serial.println(correctPasscode);
    
    delay(1000);
    
    // Clear
    lcd.setCursor(0, 1);
    lcd.print("    ");
    digitalWrite(LED_PIN, LOW);
    
    delay(1000);
  }
  
  Serial.println("✓ Flash sequence complete");
}

// ===== TEMPERATURE SETTING =====
void promptTemperatureSetting() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Room Temp:");
  lcd.setCursor(0, 1);
  lcd.print("3 digits + #");
  
  Serial.println("\n>>> Enter preferred temperature");
  Serial.println("    Format: 3 digits + # key");
  Serial.println("    Example: 235# = 23.5°C");
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
      
      Serial.print("    Temp digit: ");
      Serial.println(key);
      
      // Beep
      digitalWrite(ACTIVE_BUZZER_PIN, HIGH);
      delay(50);
      digitalWrite(ACTIVE_BUZZER_PIN, LOW);
    }
    
    // Confirm with #
    if (key == '#' && tempInput.length() == 3) {
      int tempInt = tempInput.toInt();
      preferredTemp = tempInt / 10.0;
      
      Serial.print("\n✓ Temperature set to: ");
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
      Serial.println("    Cleared - re-enter");
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
  
  Serial.print("    Display: ");
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
  lcd.print("to Start System");
  
  Serial.println("\n>>> Press button (Pin 18) to start system");
  Serial.println("    Waiting for button press...");
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
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║    *** SYSTEM STARTING ***         ║");
  Serial.println("╚════════════════════════════════════╝");
  Serial.println("\n📡 Sending configuration to MEGA-2 via I2C...");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System is ON");
  lcd.setCursor(0, 1);
  lcd.print("Sending data...");
  
  // Send to MEGA-2 via I2C
  sendDataToMega2();
  
  delay(2000);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Active");
  lcd.setCursor(0, 1);
  lcd.print("Monitoring...");
  
  Serial.println("\n✓ MEGA-1 configuration complete");
  Serial.println("✓ System active and monitoring");
  Serial.println("====================================");
}

// ===== I2C COMMUNICATION TO MEGA-2 =====
void sendDataToMega2() {
  Wire.beginTransmission(8); // MEGA-2 I2C address
  Wire.write((byte*)&preferredTemp, sizeof(preferredTemp));
  Wire.endTransmission();
  
  Serial.print("✓ I2C: Sent ");
  Serial.print(preferredTemp, 1);
  Serial.println("°C to MEGA-2 (address 8)");
}

// ===== RUNNING STATE =====
void handleRunningState() {
  static unsigned long lastUpdate = 0;
  
  if (millis() - lastUpdate > 5000) {
    lastUpdate = millis();
    
    Serial.println("\n=== MEGA-1 STATUS REPORT ===");
    Serial.print("Passcode: ");
    Serial.println(correctPasscode);
    Serial.print("Preferred Temp: ");
    Serial.print(preferredTemp, 1);
    Serial.println("°C");
    Serial.println("System Status: Running");
    Serial.print("Uptime: ");
    Serial.print(millis() / 1000);
    Serial.println(" seconds");
    Serial.println("============================");
  }
  
  delay(100);
}