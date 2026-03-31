#pragma once
#include <Arduino.h>

// Dual Clutch Hall Effect Sensor Reader on A4, A5
// Reads two independent analog hall sensors and provides debounced values
class SHDualClutchSensor
{
private:
  const uint8_t CLUTCH_A_PIN = A4;
  const uint8_t CLUTCH_B_PIN = A5;

  uint16_t clutchAValue = 0;
  uint16_t clutchBValue = 0;
  uint16_t lastClutchAValue = 0xFFFF; // Initialize to impossible value
  uint16_t lastClutchBValue = 0xFFFF;

  unsigned long lastReadTime = 0;
  const unsigned long READ_INTERVAL = 10; // Read every 10ms

  // Simple moving average filter
  static const int FILTER_SIZE = 4;
  uint16_t clutchAFilter[FILTER_SIZE] = {0};
  uint16_t clutchBFilter[FILTER_SIZE] = {0};
  int filterIndex = 0;

public:
  void begin()
  {
    pinMode(CLUTCH_A_PIN, INPUT);
    pinMode(CLUTCH_B_PIN, INPUT);
    FlowSerialDebugPrintLn("Dual Clutch Sensors initialized on A4 (Clutch A) and A5 (Clutch B)");
  }

  // Read both clutch sensors with filtering and debouncing
  void read(void (*onClutchChange)(uint16_t, uint16_t) = nullptr)
  {
    // Rate limit reads
    if (millis() - lastReadTime < READ_INTERVAL)
      return;

    lastReadTime = millis();

    // Read raw analog values (0-1023)
    uint16_t rawA = analogRead(CLUTCH_A_PIN);
    uint16_t rawB = analogRead(CLUTCH_B_PIN);

    // Apply moving average filter
    clutchAFilter[filterIndex] = rawA;
    clutchBFilter[filterIndex] = rawB;
    filterIndex = (filterIndex + 1) % FILTER_SIZE;

    // Calculate filtered average
    uint32_t sumA = 0, sumB = 0;
    for (int i = 0; i < FILTER_SIZE; i++)
    {
      sumA += clutchAFilter[i];
      sumB += clutchBFilter[i];
    }

    clutchAValue = sumA / FILTER_SIZE;
    clutchBValue = sumB / FILTER_SIZE;

    // Check if values changed significantly (more than 5 units)
    if (abs((int)clutchAValue - (int)lastClutchAValue) >= 5 ||
        abs((int)clutchBValue - (int)lastClutchBValue) >= 5)
    {
      lastClutchAValue = clutchAValue;
      lastClutchBValue = clutchBValue;

      if (onClutchChange)
      {
        onClutchChange(clutchAValue, clutchBValue);
      }
    }
  }

  // Get current clutch values
  uint16_t getClutchA() { return clutchAValue; }
  uint16_t getClutchB() { return clutchBValue; }

  // For debugging
  void printValues()
  {
    FlowSerialDebugPrintLn("Clutch A: " + String(clutchAValue) + " | Clutch B: " + String(clutchBValue));
  }
};
