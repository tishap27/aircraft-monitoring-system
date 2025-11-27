// LED pin declarations
const int LEFT_RED_PIN = 31;
const int LEFT_GREEN_PIN = 37; //
const int RIGHT_RED_PIN = 33;
const int RIGHT_GREEN_PIN = 32;

// Dummy flag for example, replace with your nightMode variable
bool nightMode = false;

void setup() {
  pinMode(LEFT_RED_PIN, OUTPUT);
  pinMode(LEFT_GREEN_PIN, OUTPUT);
  pinMode(RIGHT_RED_PIN, OUTPUT);
  pinMode(RIGHT_GREEN_PIN, OUTPUT);

  // Start LEDs off
  turnAllOff();
}

void loop() {
  // Toggle night mode for testing every 5s
  nightMode = !nightMode;

  if (nightMode) {
    // Navigation lights ON (left red, right green)
    digitalWrite(LEFT_RED_PIN, HIGH);
    digitalWrite(LEFT_GREEN_PIN, LOW);
    digitalWrite(RIGHT_RED_PIN, LOW);
    digitalWrite(RIGHT_GREEN_PIN, HIGH);
  } else {
    // Navigation lights OFF
    turnAllOff();
  }

  delay(5000);
}

void turnAllOff() {
  digitalWrite(LEFT_RED_PIN, LOW);
  digitalWrite(LEFT_GREEN_PIN, LOW);
  digitalWrite(RIGHT_RED_PIN, LOW);
  digitalWrite(RIGHT_GREEN_PIN, LOW);
}
