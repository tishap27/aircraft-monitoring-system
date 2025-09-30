/*
 PIR MOTION TEST
*/
const int PIR_PIN = 8;
volatile bool motionTriggered = false;
int motionCount = 0;

void motionISR() {
  motionTriggered = true;
}

void setup() {
  Serial.begin(9600);
  pinMode(PIR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIR_PIN), motionISR, RISING);
  
  Serial.println("=== PIR DEBUG TEST ===");
  Serial.println("Checking PIR pin readings...");
  Serial.println("");
}

void loop() {
  // Method 1: Check interrupt (
  if (motionTriggered) {
    motionCount++;
    Serial.print("INTERRUPT MOTION #");
    Serial.println(motionCount);
    motionTriggered = false;
  }
  
  // Method 2: Direct pin reading (bypass interrupt)
  int pirState = digitalRead(PIR_PIN);
  static int lastPirState = LOW;
  static int directMotionCount = 0;
  
  if (pirState == HIGH && lastPirState == LOW) {
    directMotionCount++;
    Serial.print(" DIRECT READ MOTION #");
    Serial.println(directMotionCount);
  }
  lastPirState = pirState;
  
  // Method 3: Show raw pin values continuously
  static unsigned long lastDebugOutput = 0;
  if (millis() - lastDebugOutput > 2000) {
    lastDebugOutput = millis();
    
    Serial.println("--- Debug Info ---");
    Serial.print("PIR Pin 8 State: ");
    Serial.print(pirState);
    Serial.println(pirState == HIGH ? " (HIGH - Motion)" : " (LOW - No Motion)");
    Serial.print("Interrupt Count: ");
    Serial.println(motionCount);
    Serial.print("Direct Read Count: ");
    Serial.println(directMotionCount);
    Serial.print("Uptime: ");
    Serial.print(millis() / 1000);
    Serial.println(" seconds");
    Serial.println("------------------");
  }
  
  delay(100);
}
