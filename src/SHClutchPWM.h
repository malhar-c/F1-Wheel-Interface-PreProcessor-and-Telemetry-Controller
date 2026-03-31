#pragma once
#include <Arduino.h>
#include <EEPROM.h>

// 10-bit PWM Clutch Control on Pin 9 (Timer 1)
// Range: 0-1023 (10-bit resolution)
// EEPROM storage address for persistence
#define CLUTCH_PWM_EEPROM_ADDR 100
#define CLUTCH_PWM_MIN 0
#define CLUTCH_PWM_MAX 1023
#define CLUTCH_PWM_DEFAULT 512 // Mid-range default
#define CLUTCH_PWM_STEP 10     // Encoder increment/decrement step

class SHClutchPWM
{
private:
  uint16_t clutchValue = CLUTCH_PWM_DEFAULT;
  const uint8_t CLUTCH_PIN = 9;                // Timer 1 Pin A (10-bit PWM)
  const unsigned long EEPROM_SAVE_DELAY = 500; // Save to EEPROM after 500ms of no changes
  unsigned long lastChangeTime = 0;
  bool pendingSave = false;

public:
  void begin()
  {
    pinMode(CLUTCH_PIN, OUTPUT);

    // Load clutch value from EEPROM
    EEPROM.get(CLUTCH_PWM_EEPROM_ADDR, clutchValue);
    if (clutchValue < CLUTCH_PWM_MIN || clutchValue > CLUTCH_PWM_MAX)
    {
      clutchValue = CLUTCH_PWM_DEFAULT;
    }

    // Configure Timer 1 for 10-bit Fast PWM mode
    // WGM13:10 = 0111 (mode 7: 10-bit Fast PWM)
    // COM1A1:0 = 10 (non-inverted PWM on OC1A/pin9)
    // CS12:10 = 001 (no prescaler, F_CPU clock)

    cli(); // Disable interrupts temporarily

    // Clear Timer 1 registers
    TCCR1A = 0;
    TCCR1B = 0;

    // Set WGM13:10 = 0111 (10-bit Fast PWM)
    TCCR1A |= (1 << WGM11) | (1 << WGM10);
    TCCR1B |= (1 << WGM12);

    // Set COM1A1:0 = 10 (non-inverted PWM)
    TCCR1A |= (1 << COM1A1);

    // Set prescaler to 1 (no prescaling for maximum frequency)
    TCCR1B |= (1 << CS10);

    // Load initial PWM value
    OCR1A = clutchValue;

    sei(); // Re-enable interrupts

    FlowSerialDebugPrintLn("Clutch PWM initialized on pin 9 (10-bit). Value: " + String(clutchValue));
  }

  // Adjust clutch value by encoder steps
  void adjustByEncoder(bool isCounterClockwise)
  {
    if (isCounterClockwise)
    {
      clutchValue = max((uint16_t)CLUTCH_PWM_MIN, clutchValue - CLUTCH_PWM_STEP);
    }
    else
    {
      clutchValue = min((uint16_t)CLUTCH_PWM_MAX, clutchValue + CLUTCH_PWM_STEP);
    }

    OCR1A = clutchValue;
    lastChangeTime = millis();
    pendingSave = true;

    FlowSerialDebugPrintLn("Clutch PWM: " + String(clutchValue) + " (" + String((clutchValue * 100) / 1023) + "%)");
  }

  // Set clutch value directly (0-1023)
  void setValue(uint16_t value)
  {
    clutchValue = constrain(value, CLUTCH_PWM_MIN, CLUTCH_PWM_MAX);
    OCR1A = clutchValue;
    lastChangeTime = millis();
    pendingSave = true;
  }

  // Get current clutch value
  uint16_t getValue()
  {
    return clutchValue;
  }

  // Persist clutch value to EEPROM (call in idle/loop)
  void handleEEPROMSave()
  {
    if (pendingSave && (millis() - lastChangeTime) >= EEPROM_SAVE_DELAY)
    {
      EEPROM.put(CLUTCH_PWM_EEPROM_ADDR, clutchValue);
      pendingSave = false;
      FlowSerialDebugPrintLn("Clutch PWM saved to EEPROM: " + String(clutchValue));
    }
  }

  // Reset clutch to default
  void reset()
  {
    clutchValue = CLUTCH_PWM_DEFAULT;
    OCR1A = clutchValue;
    lastChangeTime = millis();
    pendingSave = true;
    FlowSerialDebugPrintLn("Clutch PWM reset to default: " + String(clutchValue));
  }
};
