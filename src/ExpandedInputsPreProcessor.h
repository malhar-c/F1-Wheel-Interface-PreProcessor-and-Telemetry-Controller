#pragma once
#include <Arduino.h>

// Pin definitions for 74HC165 (adjust as needed)
#define SHIFT_LOAD_PIN 7         // PL (parallel load, active low)
#define SHIFT_CLOCK_ENABLE_PIN 4 // CE (clock enable, active low)
#define SHIFT_DATA_PIN 5         // Q7 (serial data out)
#define SHIFT_CLOCK_PIN 6        // CP (clock)

#define EXPANDED_BUTTONS_COUNT 8                         // Number of buttons per shift register
const unsigned long EXPANDED_BUTTON_DEBOUNCE_DELAY = 50; // 50ms debounce delay

class ExpandedInputsPreProcessor
{
private:
  uint8_t lastRawState = 0; // Stores the last raw state of the shift register
  byte lastReportedButtonState[EXPANDED_BUTTONS_COUNT] = {0};
  unsigned long lastDebounceTime[EXPANDED_BUTTONS_COUNT] = {0};

public:
  void begin()
  {
    pinMode(SHIFT_LOAD_PIN, OUTPUT);
    pinMode(SHIFT_CLOCK_ENABLE_PIN, OUTPUT);
    pinMode(SHIFT_CLOCK_PIN, OUTPUT);
    pinMode(SHIFT_DATA_PIN, INPUT);

    digitalWrite(SHIFT_LOAD_PIN, HIGH);
    digitalWrite(SHIFT_CLOCK_PIN, LOW);
    digitalWrite(SHIFT_CLOCK_ENABLE_PIN, HIGH); // Disable clock initially
  }
  // Reads all buttons and calls the callback if state changes
  void readAll(void (*onButtonChange)(int, byte))
  {
    uint8_t currentRawState = readShiftRegister();
    // FlowSerialDebugPrintLn("SR Raw: " + String(currentRawState, BIN) + " (Dec: " + String(currentRawState) + ")"); // DEBUG: Print raw shift register data

    for (int i = 0; i < EXPANDED_BUTTONS_COUNT; i++)
    {
      bool currentButtonPhysicalState = (currentRawState >> i) & 0x01; // 0 for LOW, 1 for HIGH from SR

      // Check if the physical state has changed since last read for this specific button
      if (currentButtonPhysicalState != ((lastRawState >> i) & 0x01))
      {
        // FlowSerialDebugPrintLn("Button " + String(i) + " physical change: " + String(currentButtonPhysicalState)); // DEBUG
        lastDebounceTime[i] = millis(); // Reset debounce timer on any physical change
      }
      if ((millis() - lastDebounceTime[i]) > EXPANDED_BUTTON_DEBOUNCE_DELAY)
      {
        // Debounce time has passed, now check if this stable state is different from last reported
        if (currentButtonPhysicalState != lastReportedButtonState[i])
        {
          // FlowSerialDebugPrintLn("Button " + String(i + 100) + " state changed to: " + String(currentButtonPhysicalState)); // DEBUG

          // Invert the button state: 0 (pressed) becomes 1, 1 (released) becomes 0
          // This matches SimHub's expectation: 1 = pressed, 0 = released
          byte invertedState = !currentButtonPhysicalState;

          onButtonChange(i + 100, invertedState);                  // Send inverted state to SimHub
          lastReportedButtonState[i] = currentButtonPhysicalState; // Store the actual physical state
        }
      }
    }
    lastRawState = currentRawState; // Update the last raw state for the next cycle
  }

private:
  uint8_t readShiftRegister()
  {
    // Write pulse to load pin (PL) to load parallel data from switches
    // The digitalWrite itself takes a few microseconds.
    // For 74HC165, the minimum load pulse width is very short (e.g., 20ns).
    // The execution time of digitalWrite(LOW) then digitalWrite(HIGH) is typically sufficient.
    digitalWrite(SHIFT_LOAD_PIN, LOW);
    digitalWrite(SHIFT_LOAD_PIN, HIGH);

    // Enable shift register output (CE LOW)
    digitalWrite(SHIFT_CLOCK_ENABLE_PIN, LOW);
    // FlowSerialDebugPrintLn("Reading Shift Register..."); // DEBUG    // Read the 8 bits from the shift register
    // shiftIn(dataPin, clockPin, bitOrder)
    // Try MSBFIRST to correct the bit mapping
    byte incoming = shiftIn(SHIFT_DATA_PIN, SHIFT_CLOCK_PIN, MSBFIRST);

    // Disable shift register output (CE HIGH)
    digitalWrite(SHIFT_CLOCK_ENABLE_PIN, HIGH);

    return incoming;
  }
};