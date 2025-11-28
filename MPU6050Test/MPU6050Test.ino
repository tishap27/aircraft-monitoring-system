// ============================================================================
// MPU6050 DIAGNOSTIC TEST WITH TIMEOUT PROTECTION
// ============================================================================

#include <Wire.h>

void setup() {
  Serial.begin(9600);
  
  delay(2000);
  
  Serial.println("===========================================");
  Serial.println("MPU6050 DIAGNOSTIC TEST - TIMEOUT PROTECTED");
  Serial.println("===========================================");
  Serial.println();
  
  // Test I2C bus initialization
  Serial.println("Step 1: Testing I2C bus initialization...");
  Wire.begin();
  Wire.setClock(100000);  // Set to standard 100kHz
  Serial.println("I2C initialized at 100kHz");
  Serial.println();
  
  // Test if I2C pins are responding
  Serial.println("Step 2: Testing I2C pin configuration...");
  Serial.println("Arduino Mega I2C pins:");
  Serial.println("  SDA = Pin 20");
  Serial.println("  SCL = Pin 21");
  Serial.println();
  
  // Scan with timeout protection
  Serial.println("Step 3: Scanning I2C bus with timeout...");
  scanI2CWithTimeout();
  Serial.println();
  
  // Direct register test
  Serial.println("Step 4: Direct communication test...");
  directMPUTest();
  Serial.println();
  
  Serial.println("===========================================");
  Serial.println("DIAGNOSTIC COMPLETE");
  Serial.println("===========================================");
  Serial.println();
  printTroubleshootingGuide();
}

void loop() {
  delay(5000);
  Serial.println("Still running... Press reset to restart diagnostic");
}

// ============================================================================
// SCAN I2C WITH TIMEOUT
// ============================================================================

void scanI2CWithTimeout() {
  byte error, address;
  int deviceCount = 0;
  unsigned long timeout;
  
  Serial.println("Scanning addresses 0x01 to 0x7F...");
  
  for(address = 1; address < 127; address++) {
    Serial.print("Testing address 0x");
    if (address < 16) Serial.print("0");
    Serial.print(address, HEX);
    Serial.print("...");
    
    timeout = millis();
    Wire.beginTransmission(address);
    
    // Wait for transmission with timeout
    while(Wire.available() == 0 && (millis() - timeout) < 100) {
      // Timeout protection
    }
    
    if ((millis() - timeout) >= 100) {
      Serial.println(" TIMEOUT");
      continue;
    }
    
    error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print(" FOUND");
      
      if (address == 0x68) {
        Serial.print(" <- MPU6050 DEFAULT ADDRESS");
      } else if (address == 0x69) {
        Serial.print(" <- MPU6050 ALTERNATE ADDRESS");
      }
      Serial.println();
      deviceCount++;
    }
    else if (error == 2) {
      Serial.println(" No device");
    }
    else {
      Serial.print(" Error code: ");
      Serial.println(error);
    }
    
    delay(10);  // Small delay between scans
  }
  
  Serial.println();
  Serial.print("Scan complete. Devices found: ");
  Serial.println(deviceCount);
  
  if (deviceCount == 0) {
    Serial.println();
    Serial.println("ERROR: NO I2C DEVICES DETECTED");
    Serial.println("This indicates a wiring or hardware problem");
  }
}

// ============================================================================
// DIRECT MPU6050 TEST
// ============================================================================

void directMPUTest() {
  Serial.println("Attempting direct communication with 0x68...");
  
  Wire.beginTransmission(0x68);
  byte error = Wire.endTransmission();
  
  Serial.print("Transmission result: ");
  
  switch(error) {
    case 0:
      Serial.println("SUCCESS - Device is connected");
      
      // Try to wake up MPU6050
      Serial.println("Attempting to wake MPU6050...");
      Wire.beginTransmission(0x68);
      Wire.write(0x6B);  // PWR_MGMT_1 register
      Wire.write(0x00);  // Wake up
      error = Wire.endTransmission();
      
      if (error == 0) {
        Serial.println("Wake command sent successfully");
        delay(100);
        
        // Read WHO_AM_I
        Serial.println("Reading WHO_AM_I register...");
        Wire.beginTransmission(0x68);
        Wire.write(0x75);
        Wire.endTransmission(false);
        Wire.requestFrom(0x68, 1);
        
        if (Wire.available()) {
          byte whoami = Wire.read();
          Serial.print("WHO_AM_I = 0x");
          Serial.println(whoami, HEX);
          
          if (whoami == 0x68) {
            Serial.println("SUCCESS: Valid MPU6050 detected!");
          } else {
            Serial.println("WARNING: Unexpected WHO_AM_I value");
          }
        } else {
          Serial.println("ERROR: No data received");
        }
      } else {
        Serial.println("ERROR: Failed to send wake command");
      }
      break;
      
    case 1:
      Serial.println("FAIL - Data too long");
      break;
      
    case 2:
      Serial.println("FAIL - NACK on address");
      Serial.println("Device is NOT connected or wrong address");
      break;
      
    case 3:
      Serial.println("FAIL - NACK on data");
      break;
      
    case 4:
      Serial.println("FAIL - Other I2C error");
      break;
      
    default:
      Serial.print("FAIL - Unknown error: ");
      Serial.println(error);
      break;
  }
}

// ============================================================================
// TROUBLESHOOTING GUIDE
// ============================================================================

void printTroubleshootingGuide() {
  Serial.println("TROUBLESHOOTING GUIDE:");
  Serial.println("===========================================");
  Serial.println();
  
  Serial.println("If scan timed out or no devices found:");
  Serial.println();
  
  Serial.println("1. CHECK WIRING:");
  Serial.println("   MPU6050 VCC  -> Arduino 5V or 3.3V");
  Serial.println("   MPU6050 GND  -> Arduino GND");
  Serial.println("   MPU6050 SDA  -> Arduino Pin 20");
  Serial.println("   MPU6050 SCL  -> Arduino Pin 21");
  Serial.println();
  
  Serial.println("2. CHECK POWER:");
  Serial.println("   - Try 3.3V instead of 5V");
  Serial.println("   - Some modules need 5V, some need 3.3V");
  Serial.println("   - Check if power LED on module is lit");
  Serial.println();
  
  Serial.println("3. CHECK CONNECTIONS:");
  Serial.println("   - Ensure wires are firmly inserted");
  Serial.println("   - Try different jumper wires");
  Serial.println("   - Check for broken wires");
  Serial.println("   - Look for cold solder joints on module");
  Serial.println();
  
  Serial.println("4. CHECK MODULE:");
  Serial.println("   - Verify it is an MPU6050 (check chip marking)");
  Serial.println("   - Module may be damaged");
  Serial.println("   - Try a different MPU6050 module if available");
  Serial.println();
  
  Serial.println("5. CHECK PULL-UP RESISTORS:");
  Serial.println("   - MPU6050 modules should have built-in pull-ups");
  Serial.println("   - If not, add 4.7k resistors from SDA/SCL to VCC");
  Serial.println();
  
  Serial.println("6. CHECK AD0 PIN:");
  Serial.println("   - If AD0 is connected to VCC, address is 0x69");
  Serial.println("   - If AD0 is GND or floating, address is 0x68");
  Serial.println();
}