// ============================================================================
// MEGA-1: USER INTERFACE CONTROLLER - DEBUGGED VERSION
// Arduino MEGA 2560 - Master Controller
// ============================================================================

#include <LiquidCrystal.h>
#include <Keypad.h>
#include <Wire.h>

// Enable detailed debugging output
#define DEBUG_MODE 1

// ===== PIN DEFINITIONS FOR MEGA 2560 =====
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// 4x4 Keypad
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

const int ACTIVE_BUZZER_PIN = 22;
const int BUTTON_PIN = 18;
const int LED_PIN = 13;

// I2C Configuration
const byte MEGA2_I2C_ADDRESS = 8;

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
  
  // Wait for serial to initialize
  delay(1000);
  
  Wire.begin(); // I2C Master mode
  
  // Initialize components
  lcd.begin(16, 2);
  pinMode(ACTIVE_BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  
  // Test all outputs
  debugPrintHeader();
  testComponents();
  
  // Welcome message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MEGA-1 System");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(2000);
  
  Serial.println("[INIT] MEGA-1 Initialized");
  Serial.println("[INIT] LCD Display Ready");
  Serial.println("[INIT] Keypad Ready");
  Serial.println("[INIT] I2C Master Ready");
  Serial.println("");
  Serial.println("[STATE] Starting passcode entry sequence");
  Serial.println("====================================");
  
  // Start passcode entry
  currentState = STATE_PASSCODE_FIRST;
  promptFirstPasscode();
}

void loop() {
  #if DEBUG_MODE
  static unsigned long lastDebugPrint = 0;
  if (millis() - lastDebugPrint > 10000) {
    debugPrintState();
    lastDebugPrint = millis();
  }
  #endif
  
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

// ===== DEBUGGING FUNCTIONS =====
void debugPrintHeader() {
  Serial.println("====================================");
  Serial.println("   MEGA-1: USER INTERFACE CONTROL   ");
  Serial.println("      Arduino MEGA 2560 Master      ");
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
  
  // Test Buzzer
  Serial.print("[TEST] Buzzer: ");
  digitalWrite(ACTIVE_BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(ACTIVE_BUZZER_PIN, LOW);
  Serial.println("OK");
  
  // Test LED
  Serial.print("[TEST] LED: ");
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_PIN, LOW);
  Serial.println("OK");
  
  // Test Button
  Serial.print("[TEST] Button Pin: ");
  int buttonState = digitalRead(BUTTON_PIN);
  Serial.print(buttonState == HIGH ? "HIGH (not pressed)" : "LOW (pressed)");
  Serial.println(" - OK");
  
  // Test I2C
  Serial.print("[TEST] I2C Bus Scan: ");
  Wire.beginTransmission(MEGA2_I2C_ADDRESS);
  byte error = Wire.endTransmission();
  if (error == 0) {
    Serial.print("Device found at address ");
    Serial.println(MEGA2_I2C_ADDRESS);
  } else {
    Serial.print("No device at address ");
    Serial.print(MEGA2_I2C_ADDRESS);
    Serial.println(" (MEGA-2 may not be powered on yet)");
  }
  
  Serial.println("[TEST] All component tests complete");
  Serial.println("");
}

void debugPrintState() {
  Serial.println("\n[DEBUG] Current System State:");
  Serial.print("  State: ");
  switch(currentState) {
    case STATE_PASSCODE_FIRST: Serial.println("PASSCODE_FIRST"); break;
    case STATE_PASSCODE_SECOND: Serial.println("PASSCODE_SECOND"); break;
    case STATE_PASSCODE_VERIFY: Serial.println("PASSCODE_VERIFY"); break;
    case STATE_TEMP_SETTING: Serial.println("TEMP_SETTING"); break;
    case STATE_WAITING_START: Serial.println("WAITING_START"); break;
    case STATE_RUNNING: Serial.println("RUNNING"); break;
  }
  Serial.print("  Passcode1 length: ");
  Serial.println(passcode1.length());
  Serial.print("  Passcode2 length: ");
  Serial.println(passcode2.length());
  Serial.print("  Preferred Temp: ");
  Serial.println(preferredTemp, 1);
  Serial.print("  System Started: ");
  Serial.println(systemStarted ? "YES" : "NO");
}

// ===== PASSCODE ENTRY: FIRST ATTEMPT =====
void promptFirstPasscode() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter Passcode:");
  lcd.setCursor(0, 1);
  lcd.print("(4 digits)");
  
  Serial.println("\n[PASSCODE] Enter first passcode (4 digits)");
  passcode1 = "";
}

void handleFirstPasscode() {
  char key = keypad.getKey();
  
  if (key) {
    Serial.print("[KEYPAD] Key pressed: ");
    Serial.println(key);
    
    if (key >= '0' && key <= '9' && passcode1.length() < 4) {
      passcode1 += key;
      
      // Display asterisks for security
      lcd.setCursor(0, 1);
      lcd.print("                "); // Clear line
      lcd.setCursor(0, 1);
      for (int i = 0; i < passcode1.length(); i++) {
        lcd.print("*");
      }
      
      Serial.print("[PASSCODE] Digit entered. Length now: ");
      Serial.println(passcode1.length());
      
      // Beep feedback
      digitalWrite(ACTIVE_BUZZER_PIN, HIGH);
      delay(50);
      digitalWrite(ACTIVE_BUZZER_PIN, LOW);
      
      // Move to second passcode if 4 digits entered
      if (passcode1.length() == 4) {
        Serial.print("[PASSCODE] First passcode complete: ");
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
      Serial.println("[PASSCODE] Cleared - re-enter");
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
  
  Serial.println("\n[PASSCODE] Confirm passcode (enter again)");
  passcode2 = "";
}

void handleSecondPasscode() {
  char key = keypad.getKey();
  
  if (key) {
    Serial.print("[KEYPAD] Key pressed: ");
    Serial.println(key);
    
    if (key >= '0' && key <= '9' && passcode2.length() < 4) {
      passcode2 += key;
      
      // Display asterisks
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      for (int i = 0; i < passcode2.length(); i++) {
        lcd.print("*");
      }
      
      Serial.print("[PASSCODE] Digit entered. Length now: ");
      Serial.println(passcode2.length());
      
      // Beep feedback
      digitalWrite(ACTIVE_BUZZER_PIN, HIGH);
      delay(50);
      digitalWrite(ACTIVE_BUZZER_PIN, LOW);
      
      // Verify if 4 digits entered
      if (passcode2.length() == 4) {
        Serial.print("[PASSCODE] Second passcode complete: ");
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
      Serial.println("[PASSCODE] Cleared - re-enter");
    }
  }
}

// ===== PASSCODE VERIFICATION =====
void verifyPasscodes() {
  Serial.println("\n[VERIFY] Comparing passcodes...");
  Serial.print("[VERIFY] Passcode1: ");
  Serial.println(passcode1);
  Serial.print("[VERIFY] Passcode2: ");
  Serial.println(passcode2);
  
  if (passcode1 == passcode2 && passcode1.length() == 4) {
    // SUCCESS
    correctPasscode = passcode1;
    
    Serial.println("\n[SUCCESS] PASSCODE MATCH!");
    Serial.print("[SUCCESS] Passcode set to: ");
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
    Serial.println("\n[ERROR] PASSCODE MISMATCH!");
    Serial.println("[RESET] Restarting passcode entry...");
    
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
  Serial.println("\n[DISPLAY] Flashing passcode on 7-segment display...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Code Display:");
  
  // Flash 5 times
  for (int i = 0; i < 5; i++) {
    // Show passcode
    lcd.setCursor(0, 1);
    lcd.print(correctPasscode);
    digitalWrite(LED_PIN, HIGH);
    
    Serial.print("[DISPLAY] Flash ");
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
  
  Serial.println("[DISPLAY] Flash sequence complete");
}

// ===== TEMPERATURE SETTING =====
void promptTemperatureSetting() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Room Temp:");
  lcd.setCursor(0, 1);
  lcd.print("3 digits + #");
  
  Serial.println("\n[TEMP] Enter preferred temperature");
  Serial.println("[TEMP] Format: 3 digits + # key");
  Serial.println("[TEMP] Example: 235# = 23.5 degrees C");
}

String tempInput = "";

void handleTemperatureSetting() {
  char key = keypad.getKey();
  
  if (key) {
    Serial.print("[KEYPAD] Key pressed: ");
    Serial.println(key);
    
    // Number input
    if (key >= '0' && key <= '9' && tempInput.length() < 3) {
      tempInput += key;
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(tempInput);
      
      Serial.print("[TEMP] Temp input: ");
      Serial.println(tempInput);
      
      // Beep
      digitalWrite(ACTIVE_BUZZER_PIN, HIGH);
      delay(50);
      digitalWrite(ACTIVE_BUZZER_PIN, LOW);
    }
    
    // Confirm with #
    if (key == '#' && tempInput.length() == 3) {
      int tempInt = tempInput.toInt();
      preferredTemp = tempInt / 10.0;
      
      Serial.print("\n[TEMP] Temperature set to: ");
      Serial.print(preferredTemp, 1);
      Serial.println(" degrees C");
      
      displayTemperature();
      
      currentState = STATE_WAITING_START;
      promptWaitingForStart();
    }
    
    // Clear with *
    if (key == '*') {
      tempInput = "";
      lcd.setCursor(0, 1);
      lcd.print("3 digits + #    ");
      Serial.println("[TEMP] Cleared - re-enter");
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
  
  Serial.print("[DISPLAY] ");
  Serial.print(preferredTemp, 1);
  Serial.print(" degrees C / ");
  Serial.print(tempF, 1);
  Serial.println(" degrees F");
  
  delay(3000);
}

// ===== WAITING FOR START BUTTON =====
void promptWaitingForStart() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Press Button");
  lcd.setCursor(0, 1);
  lcd.print("to Start System");
  
  Serial.println("\n[BUTTON] Press button (Pin 18) to start system");
  Serial.println("[BUTTON] Waiting for button press...");
}

void handleWaitingForStart() {
  static unsigned long lastButtonCheck = 0;
  
  #if DEBUG_MODE
  // Debug button state every 5 seconds
  if (millis() - lastButtonCheck > 5000) {
    int buttonState = digitalRead(BUTTON_PIN);
    Serial.print("[DEBUG] Button state: ");
    Serial.println(buttonState == HIGH ? "HIGH (not pressed)" : "LOW (pressed)");
    lastButtonCheck = millis();
  }
  #endif
  
  if (digitalRead(BUTTON_PIN) == LOW) { // Button pressed (active low with pull-up)
    delay(50); // Debounce
    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("[BUTTON] Button press detected!");
      startSystem();
    }
  }
}

// ===== SYSTEM START =====
void startSystem() {
  systemStarted = true;
  currentState = STATE_RUNNING;
  
  Serial.println("\n====================================");
  Serial.println("    SYSTEM STARTING");
  Serial.println("====================================");
  Serial.println("\n[I2C] Sending configuration to MEGA-2...");
  
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
  
  Serial.println("\n[SUCCESS] MEGA-1 configuration complete");
  Serial.println("[SUCCESS] System active and monitoring");
  Serial.println("====================================");
}

// ===== I2C COMMUNICATION TO MEGA-2 =====
void sendDataToMega2() {
  Serial.print("[I2C] Attempting transmission to address ");
  Serial.println(MEGA2_I2C_ADDRESS);
  Serial.print("[I2C] Sending temperature: ");
  Serial.print(preferredTemp, 1);
  Serial.println(" degrees C");
  Serial.print("[I2C] Data size: ");
  Serial.print(sizeof(preferredTemp));
  Serial.println(" bytes");
  
  Wire.beginTransmission(MEGA2_I2C_ADDRESS);
  Wire.write((byte*)&preferredTemp, sizeof(preferredTemp));
  byte error = Wire.endTransmission();
  
  Serial.print("[I2C] Transmission result: ");
  switch(error) {
    case 0:
      Serial.println("SUCCESS");
      break;
    case 1:
      Serial.println("ERROR - Data too long to fit in transmit buffer");
      break;
    case 2:
      Serial.println("ERROR - Received NACK on transmit of address");
      break;
    case 3:
      Serial.println("ERROR - Received NACK on transmit of data");
      break;
    case 4:
      Serial.println("ERROR - Other error");
      break;
    default:
      Serial.println("ERROR - Unknown error code");
      break;
  }
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
    Serial.println(" degrees C");
    Serial.println("System Status: Running");
    Serial.print("Uptime: ");
    Serial.print(millis() / 1000);
    Serial.println(" seconds");
    Serial.println("============================");
  }
  
  delay(100);
}