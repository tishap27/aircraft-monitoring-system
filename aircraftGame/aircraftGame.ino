
//  WIRING 
//  LCD Display 
// GP11 (LCD_DIN/MOSI)  -> GPIO 23
// GP10 (LCD_CLK/SCLK)  -> GPIO 18
// GP9  (LCD_CS)        -> GPIO 5
// GP8  (LCD_DC)        -> GPIO 2
// GP12 (LCD_RST)       -> GPIO 4
// GP13 (LCD_BL)        -> GPIO 15
// 3V3OUT               -> 3.3V
// GND                  -> GND

//  Buttons 
// GP15 (Button A)      -> GPIO 27  SHOOT
// GP17 (Button B)      -> GPIO 26  (unused)
// GP19 (Button X)      -> GPIO 25  PAUSE
// GP21 (Button Y)      -> GPIO 14  RESTART
//  Joystick 
// GP2  (Joy UP)        -> GPIO 13
// GP18 (Joy DOWN)      -> GPIO 19
// GP16 (Joy LEFT)      -> GPIO 22
// GP20 (Joy RIGHT)     -> GPIO 21
// GP3  (Joy PRESS)     -> GPIO 12


#include <TFT_eSPI.h>
#include "plane_image.h"

TFT_eSPI tft = TFT_eSPI();


#define BTN_A     27  // shoot
#define BTN_B     26
#define BTN_X     25  // pause
#define BTN_Y     14  // restart

#define JOY_UP    13
#define JOY_DOWN  19
#define JOY_LEFT  22
#define JOY_RIGHT 21
#define JOY_PRESS 12

#define PLANE_W   50
#define PLANE_H   50

// Bullets 
#define MAX_BULLETS 5
struct Bullet {
  int x, y;
  bool active;
};
Bullet bullets[MAX_BULLETS];

//  Game state
int playerX = 95;
int playerY = 95;
int speed = 5;
bool paused = false;
bool lastPauseBtn = HIGH;
bool lastShootBtn = HIGH;

void drawBullet(int x, int y, uint16_t color) {
  tft.fillRect(x, y, 8, 4, color);
}

void shoot() {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) {
      bullets[i].x = playerX + PLANE_W;
      bullets[i].y = playerY + PLANE_H / 2;
      bullets[i].active = true;
      break;
    }
  }
}

void updateBullets() {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].active) {
      drawBullet(bullets[i].x, bullets[i].y, TFT_BLACK);
      bullets[i].x += 10;
      if (bullets[i].x > 240) {
        bullets[i].active = false;
      } else {
        drawBullet(bullets[i].x, bullets[i].y, TFT_YELLOW);
      }
    }
  }
}

void restartGame() {
  for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = false;
  playerX = 95;
  playerY = 95;
  paused = false;
  tft.fillScreen(TFT_BLACK);
  tft.pushImage(playerX, playerY, PLANE_W, PLANE_H, airplane);
}

void showPause() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3);
  tft.setCursor(55, 100);
  tft.print("PAUSED");
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY);
  tft.setCursor(45, 140);
  tft.print("Press X to resume");
}

void setup() {
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);

  tft.init();
  tft.setRotation(1);
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

  for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = false;

  tft.pushImage(playerX, playerY, PLANE_W, PLANE_H, airplane);
}

void loop() {

  // Y - Restart (works even when paused)
  if (digitalRead(BTN_Y) == LOW) {
    restartGame();
    delay(300);
    return;
  }

  // X  Pause toggle
  bool pauseBtn = digitalRead(BTN_X);
  if (pauseBtn == LOW && lastPauseBtn == HIGH) {
    paused = !paused;
    if (paused) {
      showPause();
    } else {
      tft.fillScreen(TFT_BLACK);
      tft.pushImage(playerX, playerY, PLANE_W, PLANE_H, airplane);
    }
    delay(200);
  }
  lastPauseBtn = pauseBtn;

  if (paused) return;

  // A  Shoot
  bool shootBtn = digitalRead(BTN_A);
  if (shootBtn == LOW && lastShootBtn == HIGH) {
    shoot();
  }
  lastShootBtn = shootBtn;

  // Joystick  Move plane
  int newX = playerX;
  int newY = playerY;

  if (digitalRead(JOY_UP)    == LOW) newY -= speed;
  if (digitalRead(JOY_DOWN)  == LOW) newY += speed;
  if (digitalRead(JOY_LEFT)  == LOW) newX -= speed;
  if (digitalRead(JOY_RIGHT) == LOW) newX += speed;

  newX = constrain(newX, 0, 240 - PLANE_W);
  newY = constrain(newY, 0, 240 - PLANE_H);

  if (newX != playerX || newY != playerY) {
    tft.fillRect(playerX, playerY, PLANE_W, PLANE_H, TFT_BLACK);
    playerX = newX;
    playerY = newY;
    tft.pushImage(playerX, playerY, PLANE_W, PLANE_H, airplane);
  }

  updateBullets();

  delay(30);
}
