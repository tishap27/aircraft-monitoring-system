// ============================================================================
// IR REMOTE RECEIVER TEST CODE - MEGA 1
// ============================================================================
// Pin connections:
// Y (Signal) -> Pin 15
// R (VCC)    -> 5V
// G (GND)    -> GND
// ============================================================================

#include <IRremote.h>

const int IR_RECEIVE_PIN = 15;

void setup() {
  Serial.begin(9600);
 
  Serial.println("║    IR REMOTE RECEIVER TEST        ║");

  
  // Initialize IR receiver
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  
  Serial.println("IR Receiver initialized on Pin 15");
  Serial.println("\n Point your remote at the receiver");
  Serial.println("Press any button to test\n");
}

void loop() {
  // Check if IR signal received
  if (IrReceiver.decode()) {
 
    Serial.println("IR SIGNAL RECEIVED!");
 
    
    // Print protocol (using library's built-in function)
    Serial.print("Protocol: ");
    Serial.println(getProtocolString(IrReceiver.decodedIRData.protocol));
    
    // Print raw data
    Serial.print("Raw Data: 0x");
    Serial.println(IrReceiver.decodedIRData.decodedRawData, HEX);
    
    // Print command (button code)
    Serial.print("Command: 0x");
    Serial.println(IrReceiver.decodedIRData.command, HEX);
    
    // Print address
    Serial.print("Address: 0x");
    Serial.println(IrReceiver.decodedIRData.address, HEX);
    
    // Decode specific buttons (common remote codes)
    decodeButton(IrReceiver.decodedIRData.command);
    
    Serial.println("═══════════════════════════════════\n");
    
    // Resume receiving
    IrReceiver.resume();
  }
}

// ============================================================================
// DECODE BUTTON NAMES (Common IR Remote Codes)
// ============================================================================
void decodeButton(uint8_t command) {
  Serial.print("Button: ");
  
  // Most common IR remote button codes (NEC protocol)
  switch(command) {
    // Number buttons (0-9)
    case 0x16: Serial.println("0"); break;
    case 0x0C: Serial.println("1"); break;
    case 0x18: Serial.println("2"); break;
    case 0x5E: Serial.println("3"); break;
    case 0x08: Serial.println("4"); break;
    case 0x1C: Serial.println("5"); break;
    case 0x5A: Serial.println("6"); break;
    case 0x42: Serial.println("7"); break;
    case 0x52: Serial.println("8"); break;
    case 0x4A: Serial.println("9"); break;
    
    // Control buttons
    case 0x45: Serial.println("POWER"); break;
    case 0x46: Serial.println("MODE"); break;
    case 0x47: Serial.println("MUTE"); break;
    case 0x44: Serial.println("PLAY/PAUSE"); break;
    case 0x40: Serial.println("FORWARD"); break;
    case 0x43: Serial.println("BACKWARD"); break;
    case 0x07: Serial.println("EQ / VOL-"); break;
    case 0x15: Serial.println("ST/REPT / VOL+"); break;
    case 0x09: Serial.println("U/SD / CH+"); break;
    case 0x19: Serial.println("CH-"); break;
    
    // Special buttons
    case 0x0A: Serial.println("STAR (*)"); break;
    case 0x0E: Serial.println("HASH (#)"); break;
    
    // Common arrow/navigation codes (varies by remote)
    case 0x02: Serial.println("UP"); break;
    case 0x98: Serial.println("DOWN"); break;
    case 0x34: Serial.println("LEFT"); break;
    case 0xCC: Serial.println("RIGHT"); break;
    case 0xE0: Serial.println("OK/ENTER"); break;
    
    default:
      Serial.print("UNKNOWN (0x");
      Serial.print(command, HEX);
      Serial.println(")");
      Serial.println("\nWrite down this code!");
      Serial.println("You can map it to a function");
      break;
  }
}