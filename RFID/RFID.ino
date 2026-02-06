// ============================================================================
// RC522 RFID MODULE - COMPLETE TEST & DEBUG CODE
// ============================================================================
// This code will help you verify if your RC522 is working correctly
// Open Serial Monitor at 9600 baud to see detailed debug information
// ============================================================================

#include <SPI.h>
#include <MFRC522.h>

// ===== RC522 PIN DEFINITIONS =====
#define RST_PIN 49    // Reset pin
#define SS_PIN 53     // SDA/SS pin

MFRC522 rfid(SS_PIN, RST_PIN);

// Variables for testing
unsigned long lastCheck = 0;
int successfulReads = 0;
int failedReads = 0;
bool moduleInitialized = false;

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(9600);
  
  // Wait for serial to initialize
  delay(1000);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════════════════╗");
  Serial.println("║     RC522 RFID MODULE - TEST & DEBUG PROGRAM      ║");
  Serial.println("╚════════════════════════════════════════════════════╝");
  Serial.println();
  
  // Test 1: SPI Initialization
  Serial.println("TEST 1: Initializing SPI...");
  SPI.begin();
  Serial.println(" SPI initialized successfully");
  Serial.println();
  
  delay(500);
  
  // Test 2: RFID Module Initialization
  Serial.println("TEST 2: Initializing RC522 module...");
  rfid.PCD_Init();
  delay(100);
  
  // Test 3: Check if module responds
  Serial.println("TEST 3: Checking RC522 module communication...");
  byte version = rfid.PCD_ReadRegister(rfid.VersionReg);
  
  Serial.print("RC522 Version Register: 0x");
  Serial.println(version, HEX);
  
  if (version == 0x00 || version == 0xFF) {
    Serial.println(" ERROR: Cannot communicate with RC522!");
    Serial.println();
    Serial.println("TROUBLESHOOTING:");
    Serial.println("1. Check all wiring connections");
    Serial.println("2. Verify 3.3V power is connected (NOT 5V!)");
    Serial.println("3. Check resistors are properly connected");
    Serial.println("4. Make sure SPI pins are correct:");
    Serial.println("   - SDA  → Pin 53");
    Serial.println("   - SCK  → Pin 52");
    Serial.println("   - MOSI → Pin 51");
    Serial.println("   - MISO → Pin 50");
    Serial.println("   - RST  → Pin 49");
    Serial.println("   - GND  → GND");
    Serial.println("   - 3.3V → 3.3V");
    Serial.println();
    moduleInitialized = false;
  } else {
    Serial.println(" RC522 communication OK!");
    Serial.print(" Chip version: ");
    
    // Identify chip version
    switch(version) {
      case 0x88: Serial.println("(clone)"); break;
      case 0x90: Serial.println("v0.0"); break;
      case 0x91: Serial.println("v1.0"); break;
      case 0x92: Serial.println("v2.0"); break;
      default: Serial.println("(unknown)"); break;
    }
    
    moduleInitialized = true;
  }
  
  Serial.println();
  delay(500);
  
  // Test 4: Display full module info
  if (moduleInitialized) {
    Serial.println("TEST 4: Displaying RC522 detailed information...");
    rfid.PCD_DumpVersionToSerial();
    Serial.println();
  }
  
  // Test 5: Self-test
  Serial.println("TEST 5: Running RC522 self-test...");
  bool selfTestResult = rfid.PCD_PerformSelfTest();
  
  // Re-initialize after self-test
  rfid.PCD_Init();
  delay(100);
  
  if (selfTestResult) {
    Serial.println("Self-test PASSED");
  } else {
    Serial.println("Self-test returned false (this is sometimes normal)");
  }
  Serial.println();
  
  // Ready message
  Serial.println("════════════════════════════════════════════════════");
  if (moduleInitialized) {
    Serial.println(" RC522 MODULE IS READY! ");
    Serial.println();
    Serial.println("NOW TESTING CARD DETECTION...");
    Serial.println("Place your RFID card/tag near the reader");
    Serial.println("════════════════════════════════════════════════════");
  } else {
    Serial.println(" RC522 MODULE FAILED TO INITIALIZE ");
    Serial.println("Please fix wiring and reset Arduino");
    Serial.println("════════════════════════════════════════════════════");
  }
  Serial.println();
  
  lastCheck = millis();
}

// ============================================================================
// MAIN LOOP - CONTINUOUS CARD DETECTION
// ============================================================================
void loop() {
  if (!moduleInitialized) {
    // Module not working, just wait
    delay(1000);
    Serial.println("Module not initialized. Check wiring and reset.");
    return;
  }
  
  // Check for new cards every 500ms
  if (millis() - lastCheck > 500) {
    lastCheck = millis();
    
    Serial.print(".");  // Show we're checking
    
    // Look for new cards
    if (rfid.PICC_IsNewCardPresent()) {
      Serial.println();
      Serial.println("═══════════════════════════════════════════");
      Serial.println(" CARD DETECTED! Attempting to read...");
      
      if (rfid.PICC_ReadCardSerial()) {
        successfulReads++;
        
        Serial.println(" CARD READ SUCCESSFUL! ");
        Serial.println();
        
        // Display UID
        Serial.print(" Card UID: ");
        for (byte i = 0; i < rfid.uid.size; i++) {
          if (rfid.uid.uidByte[i] < 0x10) Serial.print("0");
          Serial.print(rfid.uid.uidByte[i], HEX);
          if (i < rfid.uid.size - 1) Serial.print(":");
        }
        Serial.println();
        
        // Display UID in decimal
        Serial.print(" Card UID (DEC): ");
        for (byte i = 0; i < rfid.uid.size; i++) {
          Serial.print(rfid.uid.uidByte[i]);
          if (i < rfid.uid.size - 1) Serial.print(":");
        }
        Serial.println();
        
        // Display card type
        Serial.print(" Card Type: ");
        MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
        Serial.println(rfid.PICC_GetTypeName(piccType));
        
        // Display SAK
        Serial.print(" SAK: 0x");
        Serial.println(rfid.uid.sak, HEX);
        
        Serial.println();
        Serial.print(" Total Successful Reads: ");
        Serial.println(successfulReads);
        Serial.print(" Total Failed Reads: ");
        Serial.println(failedReads);
        Serial.println("═══════════════════════════════════════════");
        Serial.println();
        
        // Halt card and stop encryption
        rfid.PICC_HaltA();
        rfid.PCD_StopCrypto1();
        
        delay(1000);  // Wait before next read
        
      } else {
        failedReads++;
        Serial.println();
        Serial.println(" Failed to read card serial");
        Serial.print("Failed reads: ");
        Serial.println(failedReads);
      }
    }
  }
  
  delay(50);  // Small delay to prevent overwhelming the module
}

// ============================================================================
// Additional debug function (called when card detected)
// ============================================================================
void dumpCardInfo() {
  Serial.println("─────────────────────────────────────────");
  Serial.println("DETAILED CARD INFORMATION:");
  rfid.PICC_DumpToSerial(&(rfid.uid));
  Serial.println("─────────────────────────────────────────");
}