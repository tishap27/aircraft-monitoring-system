// Pin declarations
const int RED1_PIN = 31;
const int GREEN1_PIN = 32;   //32
const int BLUE1_PIN = 33;     //33

const int RED2_PIN = 35;    //35
const int GREEN2_PIN = 34;
const int BLUE2_PIN = 36;

// Helper function to set LED color (common cathode)
void setLEDColor(int redPin, int greenPin, int bluePin, bool redOn, bool greenOn, bool blueOn) {
  digitalWrite(redPin, redOn ? HIGH : LOW);
  digitalWrite(greenPin, greenOn ? HIGH : LOW);
  digitalWrite(bluePin, blueOn ? HIGH : LOW);
}

void setup() {
  // Initialize pins as outputs
  pinMode(RED1_PIN, OUTPUT);
  pinMode(GREEN1_PIN, OUTPUT);
  pinMode(BLUE1_PIN, OUTPUT);

  pinMode(RED2_PIN, OUTPUT);
  pinMode(GREEN2_PIN, OUTPUT);
  pinMode(BLUE2_PIN, OUTPUT);

  // Start with all LEDs off
  setLEDColor(RED1_PIN, GREEN1_PIN, BLUE1_PIN, false, false, false);
  setLEDColor(RED2_PIN, GREEN2_PIN, BLUE2_PIN, false, false, false);
}

void loop() {
  // LED1 Red, LED2 Green (like nav lights)
  setLEDColor(RED1_PIN, GREEN1_PIN, BLUE1_PIN, true, false, false);
  setLEDColor(RED2_PIN, GREEN2_PIN, BLUE2_PIN, false, true, false);
  delay(2000);

  // LED1 Blue, LED2 Blue (like beacon/strobe)
  setLEDColor(RED1_PIN, GREEN1_PIN, BLUE1_PIN, false, false, true);
  setLEDColor(RED2_PIN, GREEN2_PIN, BLUE2_PIN, false, false, true);
  delay(2000);

  // LED1 Green, LED2 Red (reverse nav colors)
  setLEDColor(RED1_PIN, GREEN1_PIN, BLUE1_PIN, false, true, false);
  setLEDColor(RED2_PIN, GREEN2_PIN, BLUE2_PIN, true, false, false);
  delay(2000);

  // All off
  setLEDColor(RED1_PIN, GREEN1_PIN, BLUE1_PIN, false, false, false);
  setLEDColor(RED2_PIN, GREEN2_PIN, BLUE2_PIN, false, false, false);
  delay(2000);
}
