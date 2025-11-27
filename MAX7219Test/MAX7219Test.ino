// ============================================================================
// MAX7219 DIAGNOSTIC TEST - FIX "ALL RED" ISSUE
// ============================================================================

#include <MD_MAX72xx.h>
#include <SPI.h>

// Try different pin configurations
#define MAX7219_CLK_PIN  29
#define MAX7219_CS_PIN   27
#define MAX7219_DIN_PIN  25

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 1

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, MAX7219_DIN_PIN, MAX7219_CLK_PIN, MAX7219_CS_PIN, MAX_DEVICES);

int brightness = 8;  // Start at medium brightness

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(9600);
  delay(2000);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  MAX7219 DIAGNOSTIC TEST               ║");
  Serial.println("║  CLK=47, CS=46, DIN=39                ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  Serial.println("Initializing MAX7219...");
  
  // Initialize MAX7219
  mx.begin();
  
  // CRITICAL: Set brightness FIRST
  mx.control(MD_MAX72XX::INTENSITY, brightness);
  
  // Clear display completely
  mx.clear();
  delay(500);
  
  Serial.println(" MAX7219 initialized");
  Serial.println(" Display cleared\n");
  
  Serial.println("Commands:");
  Serial.println("  1 = Single LED Test");
  Serial.println("  2 = Row Fill (0-7)");
  Serial.println("  3 = Column Fill (0-7)");
  Serial.println("  4 = Diagonal Pattern");
  Serial.println("  5 = Checkerboard");
  Serial.println("  6 = Clear Display");
  Serial.println("  B = Brightness Up");
  Serial.println("  b = Brightness Down");
  Serial.println("  ? = Debug Info\n");
  
  // Start with simple test
  Serial.println("Starting: Single LED Test (top-left corner)\n");
  testSingleLED();
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  if (Serial.available()) {
    char input = Serial.read();
    
    if (input == '1') {
      Serial.println("► Single LED Test");
      testSingleLED();
    }
    else if (input == '2') {
      Serial.println("► Row Fill Test");
      testRowFill();
    }
    else if (input == '3') {
      Serial.println("► Column Fill Test");
      testColumnFill();
    }
    else if (input == '4') {
      Serial.println("► Diagonal Pattern");
      testDiagonal();
    }
    else if (input == '5') {
      Serial.println("► Checkerboard Pattern");
      testCheckerboard();
    }
    else if (input == '6') {
      Serial.println("► Clearing display");
      mx.clear();
      Serial.println("Display cleared\n");
    }
    else if (input == 'B') {
      if (brightness < 15) brightness++;
      mx.control(MD_MAX72XX::INTENSITY, brightness);
      Serial.print("► Brightness: ");
      Serial.print(brightness);
      Serial.println("/15\n");
    }
    else if (input == 'b') {
      if (brightness > 0) brightness--;
      mx.control(MD_MAX72XX::INTENSITY, brightness);
      Serial.print("► Brightness: ");
      Serial.print(brightness);
      Serial.println("/15\n");
    }
    else if (input == '?') {
      printDebugInfo();
    }
  }
}

// ============================================================================
// TEST 1: Single LED
// ============================================================================
void testSingleLED() {
  mx.clear();
  Serial.println("  Lighting LED at position (0,0)");
  mx.setPoint(0, 0, true);
  Serial.println("  If you see 1 LED light up in TOP-LEFT corner, DIN is correct!\n");
  delay(2000);
}

// ============================================================================
// TEST 2: Fill Rows (0-7, left to right)
// ============================================================================
void testRowFill() {
  for (int row = 0; row < 8; row++) {
    mx.clear();
    
    // Light up entire row
    for (int col = 0; col < 8; col++) {
      mx.setPoint(row, col, true);
    }
    
    Serial.print("  Row ");
    Serial.print(row);
    Serial.println(" (press any key to stop early)");
    delay(1000);
    
    if (Serial.available()) {
      Serial.read();
      break;
    }
  }
  mx.clear();
  Serial.println();
}

// ============================================================================
// TEST 3: Fill Columns (0-7, top to bottom)
// ============================================================================
void testColumnFill() {
  for (int col = 0; col < 8; col++) {
    mx.clear();
    
    for (int row = 0; row < 8; row++) {
      mx.setPoint(row, col, true);
    }
    
    Serial.print("  Column ");
    Serial.print(col);
    Serial.println(" (press any key to stop early)");
    delay(1000);
    
    if (Serial.available()) {
      Serial.read();
      break;
    }
  }
  mx.clear();
  Serial.println();
}

// ============================================================================
// TEST 4: Diagonal Pattern
// ============================================================================
void testDiagonal() {
  mx.clear();
  for (int i = 0; i < 8; i++) {
    mx.setPoint(i, i, true);
  }
  Serial.println("  Diagonal line from TOP-LEFT to BOTTOM-RIGHT\n");
  delay(3000);
  mx.clear();
}

// ============================================================================
// TEST 5: Checkerboard
// ============================================================================
void testCheckerboard() {
  mx.clear();
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      if ((row + col) % 2 == 0) {
        mx.setPoint(row, col, true);
      }
    }
  }
  Serial.println("  Checkerboard pattern displayed\n");
  delay(3000);
  mx.clear();
}

// ============================================================================
// DEBUG INFO
// ============================================================================
void printDebugInfo() {
  Serial.println("\n╔════ DEBUG INFO ════╗");
  Serial.println("Current Settings:");
  Serial.print("  Brightness: ");
  Serial.print(brightness);
  Serial.println("/15");
  Serial.println("  CLK Pin: 47");
  Serial.println("  CS Pin:  46");
  Serial.println("  DIN Pin: 39");
  Serial.println("  Modules: 1");
  Serial.println("  Type: FC16_HW");
  
  Serial.println("\nTroubleshooting:");
  Serial.println("  - If all LEDs are RED: Check DIN/DOUT connections");
  Serial.println("  - If NO LEDs light up: Check VCC/GND");
  Serial.println("  - If random pattern: Try different pin");
  Serial.println("  - Use test 1-5 to diagnose\n");
  Serial.println("╚═══════════════════╝\n");
}