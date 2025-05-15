# Rabbit Arduino Project

## Overview

This project reads sensor data, displays it, and controls actuators using:

- DHT sensor (Temperature & Humidity)
- Real-Time Clock (RTC - DS3231)
- Servo Motor
- LiquidCrystal I2C Display
- NAU7802 Load Cell Amplifier
- Watchdog Timer (AVR)

## Libraries Used

Make sure these libraries are installed via the Arduino Library Manager or included in the `/lib/` folder:

- `Wire.h`
- `RTClib.h` from Adafruit
- `DHT.h` from Adafruit
- `Servo.h`
- `LiquidCrystal_I2C.h`
- `Adafruit_NAU7802.h`
- `avr/wdt.h` (built-in)

## File Structure

See the folder layout above for organization.

## Getting Started

1. Clone this repo.
2. Open `rabbit.ino` in the Arduino IDE.
3. Ensure required libraries are installed or located in `lib/`.
4. Upload to your microcontroller (e.g., Arduino Uno).
