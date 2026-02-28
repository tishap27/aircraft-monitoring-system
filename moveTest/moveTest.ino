// 
// LCD 
// GP11 (LCD_DIN/MOSI)  -> GPIO 23
// GP10 (LCD_CLK/SCLK)  -> GPIO 18
// GP9  (LCD_CS)        -> GPIO 5
// GP8  (LCD_DC)        -> GPIO 2
// GP12 (LCD_RST)       -> GPIO 4
// GP13 (LCD_BL)        -> GPIO 15
// 3V3OUT               -> 3.3V
// GND                  -> GND

// buttons
// GP15 (Button A)      -> GPIO 27
// GP17 (Button B)      -> GPIO 26
// GP19 (Button X)      -> GPIO 25
// GP21 (Button Y)      -> GPIO 14

// Joystick
// GP2  (Joy UP)        -> GPIO 13
// GP18 (Joy DOWN)      -> GPIO 19
// GP16 (Joy LEFT)      -> GPIO 22
// GP20 (Joy RIGHT)     -> GPIO 21
// GP3  (Joy PRESS)     -> GPIO 12


#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// pins
#define BTN_A     27
#define BTN_B     26
#define BTN_X     25
#define BTN_Y     14

#define JOY_UP    13
#define JOY_DOWN  19
#define JOY_LEFT  22
#define JOY_RIGHT 21
#define JOY_PRESS 12

// position
int playerX = 120;
int playerY = 120;
int speed = 8;        // how many pixels it moves per step

void setup() {
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);

  tft.init();
  tft.setRotation(2);
  tft.invertDisplay(true);
  tft.fillScreen(TFT_BLACK);

  pinMode(BTN_A,     INPUT_PULLUP);
  pinMode(BTN_B,     INPUT_PULLUP);
  pinMode(BTN_X,     INPUT_PULLUP);
  pinMode(BTN_Y,     INPUT_PULLUP);
  pinMode(JOY_UP,    INPUT_PULLUP);
  pinMode(JOY_DOWN,  INPUT_PULLUP);
  pinMode(JOY_LEFT,  INPUT_PULLUP);
  pinMode(JOY_RIGHT, INPUT_PULLUP);
  pinMode(JOY_PRESS, INPUT_PULLUP);

  // Draw initial asterisk
  drawPlayer(playerX, playerY, TFT_WHITE);
}

void drawPlayer(int x, int y, uint16_t color) {
  tft.setTextSize(3);
  tft.setTextColor(color);
  tft.setCursor(x, y);
  tft.print("*");
}

void loop() {
  int newX = playerX;
  int newY = playerY;

  if (digitalRead(JOY_UP)    == LOW) newX -= speed;
  if (digitalRead(JOY_DOWN)  == LOW) newX += speed;
  if (digitalRead(JOY_LEFT)  == LOW) newY += speed;
  if (digitalRead(JOY_RIGHT) == LOW) newY -= speed;

  // Keep player inside screen boundaries
  newX = constrain(newX, 0, 220);
  newY = constrain(newY, 0, 215);

  // Only redraw if position changed
  if (newX != playerX || newY != playerY) {
    // Erase old position
    drawPlayer(playerX, playerY, TFT_BLACK);

    // Update position
    playerX = newX;
    playerY = newY;

    // Draw new position
    drawPlayer(playerX, playerY, TFT_WHITE);
  }

  delay(50); // controls movement speed
}
