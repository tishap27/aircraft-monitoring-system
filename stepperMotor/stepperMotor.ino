// ============================================================================
// STEPPER MOTOR TEST CODE - MEGA-2
// ============================================================================

// Stepper Motor Pins (ULN2003)
const int STEPPER_PIN1 = 31;  // IN1
const int STEPPER_PIN2 = 33;  // IN2
const int STEPPER_PIN3 = 35;  // IN3
const int STEPPER_PIN4 = 37;  // IN4

int stepperSpeed = 5;  // Delay between steps (ms)
int stepperPosition = 0;

void setup() {
  Serial.begin(9600);
  
  // Initialize Stepper Motor Pins
  pinMode(STEPPER_PIN1, OUTPUT);
  pinMode(STEPPER_PIN2, OUTPUT);
  pinMode(STEPPER_PIN3, OUTPUT);
  pinMode(STEPPER_PIN4, OUTPUT);
  
  disableStepperMotor();
  
  Serial.println("═══════════════════════════════════");
  Serial.println("  STEPPER MOTOR TEST - MEGA-2");
  Serial.println("═══════════════════════════════════");
  Serial.println();
  Serial.println("Testing stepper motor rotation...");
  Serial.println("If motor doesn't spin, try different wire colors!");
  Serial.println();
}

void loop() {
  Serial.println("\n>>> ROTATING CLOCKWISE <<<");
  stepperMotor(256, true);  // Full rotation clockwise
  delay(2000);
  
  Serial.println("\n>>> ROTATING COUNTER-CLOCKWISE <<<");
  stepperMotor(256, false);  // Full rotation counter-clockwise
  delay(2000);
  
  Serial.println("\n>>> SMALL ROTATION (45 degrees) <<<");
  stepperMotor(64, true);
  delay(1000);
}

void disableStepperMotor() {
  digitalWrite(STEPPER_PIN1, LOW);
  digitalWrite(STEPPER_PIN2, LOW);
  digitalWrite(STEPPER_PIN3, LOW);
  digitalWrite(STEPPER_PIN4, LOW);
}

void stepperMotor(int steps, bool clockwise) {
  int step_sequence[8][4] = {
    {1, 0, 0, 0},
    {1, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
    {0, 0, 0, 1},
    {1, 0, 0, 1}
  };

  for (int i = 0; i < steps; i++) {
    int step = clockwise ? (i % 8) : (7 - (i % 8));
    
    digitalWrite(STEPPER_PIN1, step_sequence[step][0]);
    digitalWrite(STEPPER_PIN2, step_sequence[step][1]);
    digitalWrite(STEPPER_PIN3, step_sequence[step][2]);
    digitalWrite(STEPPER_PIN4, step_sequence[step][3]);

    if (clockwise) {
      stepperPosition += 1;
      if (stepperPosition > 1024) stepperPosition = 0;
    } else {
      stepperPosition -= 1;
      if (stepperPosition < 0) stepperPosition = 1024;
    }

    Serial.print(".");
    delay(stepperSpeed);
  }

  disableStepperMotor();
  Serial.println(" Done!");
  Serial.print("Position: ");
  Serial.println(stepperPosition);
}