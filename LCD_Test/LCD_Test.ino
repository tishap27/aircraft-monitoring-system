#include <LiquidCrystal.h>

// LCD Pin connections to Arduino UNO
// LiquidCrystal(rs, enable, d4, d5, d6, d7)
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

void setup() {
  // Initialize serial communication
  Serial.begin(9600);
  Serial.println("=== Basic LCD Test ===");
  
  // Initialize the LCD (16 columns, 2 rows)
  lcd.begin(16, 2);
  
  // Clear the screen
  lcd.clear();
  
  // Display test messages
  lcd.setCursor(0, 0);  // First row, first column
  lcd.print("LCD Test!");
  
  lcd.setCursor(0, 1);  // Second row, first column  
  lcd.print("Can you read!?");
  
  Serial.println("LCD initialized successfully!");
  Serial.println("You should see on LCD:");
  Serial.println("Line 1: LCD Test!");
  Serial.println("Line 2: Can you read!?");
  Serial.println("");
  Serial.println(" blink test...");
}

void loop() {
  // Blinking indicator in bottom-right corner
  lcd.setCursor(15, 1);  // Last column, second row
  lcd.print("*");        // Show asterisk
  delay(1000);           // Wait 1 second
  
  lcd.setCursor(15, 1);
  lcd.print(" ");        // Clear asterisk (space)
  delay(1000);           // Wait 1 second
  
  // Print status to serial monitor
  static int counter = 0;
  counter++;
  
  Serial.print("Blink cycle: ");
  Serial.println(counter);
}