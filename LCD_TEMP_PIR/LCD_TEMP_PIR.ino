#include <LiquidCrystal.h>

// ===== PIN DEFINITIONS =====
// LCD pins 
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// Sensor pins
const int PIR_PIN = 8;        // PIR motion sensor
const int TMP36_PIN = A0;     // Temperature sensor

// ===== SYSTEM VARIABLES =====
// Motion detection
bool motionActive = false;
bool lastPirState = LOW;
int motionCount = 0;
unsigned long lastMotionTime = 0;
unsigned long motionStartTime = 0;

// Temperature monitoring
float currentTemp = 0.0;
float tempF = 0.0;

// Display control
unsigned long lastDisplayUpdate = 0;
unsigned long lastSerialUpdate = 0;
int displayMode = 0; // 0=normal, 1=motion alert

void setup() {
  Serial.begin(9600);
  
  // Initialize components
  lcd.begin(16, 2);
  pinMode(PIR_PIN, INPUT);
  
  Serial.println("=== COMPLETE INTEGRATION TEST ===");
  Serial.println("PIR Motion + TMP36 Temperature + LCD Display");
  Serial.println("");
  
  // Welcome display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Starting");
  lcd.setCursor(0, 1);
  lcd.print("Please Wait...");
  
  delay(3000);
  
  // System ready
  Serial.println("System Components:");
  Serial.println("LCD Display: Ready");
  Serial.println("TMP36 Temperature Sensor: Ready");
  Serial.println("PIR Motion Sensor: Ready");
  Serial.println("");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("All Systems");
  lcd.setCursor(0, 1);
  lcd.print("Ready!");
  
  delay(2000);
  
  Serial.println("*** MONITORING STARTED ***");
  Serial.println("Move ball near PIR sensor to test");
  Serial.println("================================");
  
  // Initial readings
  readTemperature();
  updateDisplay();
}

void loop() {
  // Read all sensors
  checkMotion();
  readTemperature();
  
  // Update displays
  if (millis() - lastDisplayUpdate > 1000) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }
  
  // Send serial updates
  if (millis() - lastSerialUpdate > 3000) {
    sendSerialStatus();
    lastSerialUpdate = millis();
  }
  
  delay(100);
}

// ===== MOTION DETECTION =====
void checkMotion() {
  bool currentPirState = digitalRead(PIR_PIN);
  
  // Motion started (LOW to HIGH)
  if (currentPirState == HIGH && lastPirState == LOW) {
    handleMotionStart();
  }
  
  // Motion ended (HIGH to LOW)
  if (currentPirState == LOW && lastPirState == HIGH) {
    handleMotionEnd();
  }
  
  // Update motion timing
  if (currentPirState == HIGH) {
    lastMotionTime = millis();
  }
  
  lastPirState = currentPirState;
}

void handleMotionStart() {
  motionCount++;
  motionActive = true;
  motionStartTime = millis();
  displayMode = 1; // Switch to motion alert mode
  
  Serial.println("");
  Serial.println("*** MOTION DETECTED! ***");
  Serial.print("Motion Event #");
  Serial.println(motionCount);
  Serial.print("Temperature at detection: ");
  Serial.print(currentTemp, 1);
  Serial.println("C");
  Serial.print("Time: ");
  Serial.print(millis() / 1000);
  Serial.println(" seconds");
  Serial.println("================================");
  
  // Immediate LCD update for motion
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("*** MOTION! ***");
  lcd.setCursor(0, 1);
  lcd.print("T:");
  lcd.print(currentTemp, 1);
  lcd.print("C #");
  lcd.print(motionCount);
}

void handleMotionEnd() {
  if (motionActive) {
    unsigned long duration = millis() - motionStartTime;
    motionActive = false;
    displayMode = 0; // Return to normal mode
    
    Serial.println("");
    Serial.println(" Motion Ended");
    Serial.print("Duration: ");
    Serial.print(duration / 1000.0, 1);
    Serial.println(" seconds");
    Serial.println("Returning to normal monitoring...");
    Serial.println("================================");
  }
}

// ===== TEMPERATURE READING =====
void readTemperature() {
  int sensorValue = analogRead(TMP36_PIN);
  float voltage = sensorValue * (5.0 / 1023.0);
  currentTemp = (voltage - 0.5) * 100.0;
  tempF = (currentTemp * 9.0/5.0) + 32.0;
}

// ===== DISPLAY MANAGEMENT =====
void updateDisplay() {
  // Don't interrupt motion display immediately
  if (displayMode == 1 && (millis() - motionStartTime < 2000)) {
    return; // Keep showing motion alert
  }
  
  // Switch back to normal mode after motion alert
  if (displayMode == 1 && !motionActive) {
    displayMode = 0;
  }
  
  lcd.clear();
  
  // Line 1: Temperature + Motion Status
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(currentTemp, 1);
  lcd.print("C ");
  lcd.print((int)tempF);
  lcd.print("F");
  
  // Show motion indicator
  if (motionActive) {
    lcd.setCursor(15, 0);
    lcd.print("M"); // Motion indicator
  }
  
  // Line 2: Motion count + Status
  lcd.setCursor(0, 1);
  lcd.print("Motion: ");
  lcd.print(motionCount);
  
  // Status indicator
  if (motionActive) {
    lcd.setCursor(11, 1);
    lcd.print("ACTIV");
  } else if (motionCount > 0) {
    lcd.setCursor(11, 1);
    lcd.print("QUIET");
  } else {
    lcd.setCursor(11, 1);
    lcd.print("READY");
  }
}

// ===== SERIAL STATUS UPDATES =====
void sendSerialStatus() {
  Serial.println("=== SYSTEM STATUS ===");
  Serial.print("Temperature: ");
  Serial.print(currentTemp, 1);
  Serial.print("C (");
  Serial.print(tempF, 1);
  Serial.println("F)");
  
  Serial.print("Motion Count: ");
  Serial.println(motionCount);
  
  Serial.print("Motion Status: ");
  if (motionActive) {
    Serial.println("ACTIVE (motion detected)");
    Serial.print("Motion duration: ");
    Serial.print((millis() - motionStartTime) / 1000.0, 1);
    Serial.println(" seconds");
  } else if (motionCount > 0) {
    Serial.println("QUIET (no current motion)");
    Serial.print("Last motion: ");
    Serial.print((millis() - lastMotionTime) / 1000);
    Serial.println(" seconds ago");
  } else {
    Serial.println("READY (no motion detected yet)");
  }
  
  Serial.print("System Uptime: ");
  Serial.print(millis() / 1000);
  Serial.println(" seconds");
  Serial.println("===================");
}