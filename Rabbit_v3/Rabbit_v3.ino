#include <Wire.h>
#include <RTClib.h>
#include <DHT.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_NAU7802.h>
#include <avr/wdt.h>  // <-- Add this for watchdog

// --- DHT22 ---
#define DHTPIN 11
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
float temp = 0;

// --- Relay ---
#define RELAY_PIN 13
bool fanState = false;

// --- Servo ---
Servo myServo;
bool servoOpen = false;

// Feeding settings
const float minFoodWeight = 35.0;       // Minimum weight to avoid feeding
const float targetFoodWeight = 50.0;   // Stop feeding when weight is enough
bool fedAt8am = false;
bool fedAt5pm = false;

// --- RTC & LCD ---
RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);

//Buzzer
#define BUZZER_PIN 10

// --- Load Cell ---
Adafruit_NAU7802 nau;
int32_t zero_offset = 0;
float calibration_factor = 2.294;
float weight_grams = 0;

void setup() {
  // --- Start Watchdog (8s timeout) ---
  wdt_enable(WDTO_8S); // Auto reset if loop doesn't finish in 8s

  Serial.begin(115200);

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // RTC
  if (!rtc.begin()) {
    Serial.println("RTC failed");
    while (1);
  }

  // DHT
  dht.begin();

  // Relay
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Default OFF

  //Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Default OFF
  
  // Servo
  myServo.attach(9);
  myServo.write(120); // Closed

  // Load Cell
  if (!nau.begin()) {
    Serial.println("Failed to find NAU7802");
    while (1);
  }

  nau.setLDO(NAU7802_3V0);
  nau.setGain(NAU7802_GAIN_128);
  nau.setRate(NAU7802_RATE_80SPS);

  // Discard old readings to stabilize load cell
  for (uint8_t i = 0; i < 10; i++) {
    while (!nau.available()) delay(1); // Wait until data is available
    nau.read(); // Read data and discard
  }

  // Perform calibration
  while (!nau.calibrate(NAU7802_CALMOD_INTERNAL)) delay(100); // Internal calibration
  while (!nau.calibrate(NAU7802_CALMOD_OFFSET)) delay(100); // Offset calibration

  // Calculate zero offset by reading 10 samples
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    while (!nau.available()) delay(1); // Wait for data
    sum += nau.read(); // Add the reading
  }

  // Average the readings to get the zero offset
  zero_offset = sum / 10;

  // Get the initial weight reading after calibration
  while (!nau.available()) delay(1); // Ensure data is available
  int32_t raw = nau.read(); // Read raw data
  int32_t adjusted = raw - zero_offset; // Adjust by subtracting zero offset
  weight_grams = (adjusted / 1000.0) * calibration_factor; // Convert to weight in grams

  // Initial feeding check based on weight
  if (weight_grams < minFoodWeight) {
    myServo.write(0); // Open servo to start feeding
    servoOpen = true;
    Serial.println("Initial feeding on boot..."); // Log initial feeding
    delay(1000); // Wait for food to drop
  }

}

void loop() {
  // --- Watchdog Reset ---
  wdt_reset(); // Prevents auto-reset unless freeze occurs

  DateTime now = rtc.now();
  int currentHour = now.hour();
  int currentMinute = now.minute();

  //Initial feeding
  if (servoOpen && weight_grams >= targetFoodWeight) {
    myServo.write(120); // Close
    servoOpen = false;
    Serial.println("Feeding complete.");
  }
  // --- Safe DHT Read ---
  float newTemp = dht.readTemperature();
  if (!isnan(newTemp) && newTemp < 80.0 && newTemp > -40.0) {
    temp = newTemp;
  }

  // --- Update Weight ---
  if (nau.available()) {
    int32_t raw = nau.read();
    int32_t adjusted = raw - zero_offset;
    weight_grams = (adjusted / 1000.0) * calibration_factor;
  }

  // --- Fan Control ---
  if (temp >= 31.0 && !fanState) {
    digitalWrite(RELAY_PIN, HIGH);
    fanState = true;
    delay(50);  // Optional debounce
  } else if (temp <= 29.0 && fanState) {
    digitalWrite(RELAY_PIN, LOW);
    fanState = false;
    delay(50);  // Optional debounce
  }

  // --- Reset Feeding Flags at Midnight ---
  if (currentHour == 0 && currentMinute == 0) {
    fedAt8am = false;
    fedAt5pm = false;
  }

  // --- Feeding Logic ---
  if (currentHour == 8 && currentMinute < 3 && !fedAt8am && !servoOpen) {
    if (weight_grams < minFoodWeight) {
      myServo.write(0); //Open
      servoOpen = true;
      Serial.println("Feeding at 8AM...");
    } else {
      Serial.println("8AM: Food sufficient, skipping feed.");
      //Trigger Buzzer
      digitalWrite(BUZZER_PIN, HIGH);
      delay(2000);
      digitalWrite(BUZZER_PIN, LOW);
    }
    fedAt8am = true;
  }

  if (currentHour == 17 && currentMinute < 3 && !fedAt5pm && !servoOpen) {
    if (weight_grams < minFoodWeight) {
      myServo.write(0); //Open
      servoOpen = true;
      Serial.println("Feeding at 5PM...");
    } else {
      Serial.println("5PM: Food sufficient, skipping feed.");
      //Trigger Buzzer
      digitalWrite(BUZZER_PIN, HIGH);
      delay(2000);
      digitalWrite(BUZZER_PIN, LOW);
    }
    fedAt5pm = true;
  }

  // --- Stop Feeding if Enough Weight ---
  if (servoOpen && weight_grams >= targetFoodWeight) {
    myServo.write(120); // Close
    servoOpen = false;
    Serial.println("Feeding complete.");
  }

  // --- LCD Update ---
  lcd.setCursor(0, 0);
  lcd.print(now.hour() < 10 ? "0" : ""); lcd.print(now.hour());
  lcd.print(":");
  lcd.print(now.minute() < 10 ? "0" : ""); lcd.print(now.minute());
  lcd.print(" FAN:");
  lcd.print(fanState ? "ON " : "OFF");

  // Clear second line before updating
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);

  int tempInt = (int)temp;
  int tempDec = abs((int)((temp - tempInt) * 10));
  int weightInt = (int)weight_grams;

  char line[17];
  snprintf(line, sizeof(line), "W:%3dg T:%2d.%1dC", weightInt, tempInt, tempDec);
  lcd.print(line);

  delay(200);
}
