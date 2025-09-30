/*
 TEMPERATURE SENSOR TEST
 */
#include <LiquidCrystal.h>

// LCD setup (keep your working LCD connections)
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// TMP36 Temperature Sensor   // in the kit its DHT11 [both temp + humidity sensor]
const int TMP36_PIN = A0;

void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);
  
  Serial.println("=== TMP36 Temperature Sensor Test ===");
  Serial.println("");
  
  // LCD welcome message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TMP36 Sensor");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  
  delay(2000);
  
  Serial.println("TMP36 sensor ready");
  Serial.println("Reading temperature every 2 seconds");
  Serial.println("");
}

void loop() {
  // Read the analog value from TMP36
  int sensorValue = analogRead(TMP36_PIN);
  
  // Convert analog reading to voltage
  float voltage = sensorValue * (5.0 / 1023.0);
  
  // Convert voltage to temperature in Celsius
  // TMP36 formula: Temperature = (voltage - 0.5) * 100
  float temperatureC = (voltage - 0.5) * 100.0;
  
  // Convert to Fahrenheit
  float temperatureF = (temperatureC * 9.0/5.0) + 32.0;
  
  // Display detailed information on Serial Monitor
  Serial.println("=== TMP36 Reading ===");
  Serial.print("Raw ADC Value: ");
  Serial.println(sensorValue);
  Serial.print("Voltage: ");
  Serial.print(voltage, 3);
  Serial.println(" V");
  Serial.print("Temperature: ");
  Serial.print(temperatureC, 1);
  Serial.println(" °C");
  Serial.print("Temperature: ");
  Serial.print(temperatureF, 1);
  Serial.println(" °F");
  Serial.println("");
  
  // Update LCD display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(temperatureC, 1);
  lcd.print("C");
  
  lcd.setCursor(0, 1);
  lcd.print("      ");
  lcd.print(temperatureF, 1);
  lcd.print("F");
  
  // Add a simple status indicator
  static int readingCount = 0;
  readingCount++;
  
  // Show reading number in corner of LCD
  lcd.setCursor(14, 0);
  lcd.print(readingCount % 10); // Show last digit of count
  
  Serial.print("Reading #");
  Serial.println(readingCount);
  Serial.println("---");
  
  delay(2000); // Update every 2 seconds
}
