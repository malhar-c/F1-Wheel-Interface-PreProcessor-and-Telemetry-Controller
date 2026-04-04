#pragma once
#include <Arduino.h>

// Dual Clutch Hall Effect Sensor Reader on A4, A5
// SS49E linear Hall sensors: output rests at Vcc/2 (~608 ADC on 5V supply).
// Calibration maps [calRest, calFull] -> 0-1023 so the output starts at zero
// when the lever is released. Works for both magnet orientations (calFull can
// be higher or lower than calRest — map() handles reversed ranges).
class SHDualClutchSensor
{
private:
  const uint8_t CLUTCH_A_PIN = A4;
  const uint8_t CLUTCH_B_PIN = A5;

  uint16_t clutchAValue = 0;
  uint16_t clutchBValue = 0;
  uint16_t lastClutchAValue = 0xFFFF;
  uint16_t lastClutchBValue = 0xFFFF;

  unsigned long lastReadTime = 0;
  const unsigned long READ_INTERVAL = 10; // Read every 10ms

  // Simple moving average filter
  static const int FILTER_SIZE = 4;
  uint16_t clutchAFilter[FILTER_SIZE] = {0};
  uint16_t clutchBFilter[FILTER_SIZE] = {0};
  int filterIndex = 0;

  // Per-channel calibration endpoints.
  // calRest  = raw ADC when lever is fully RELEASED (no magnet influence).
  // calFull  = raw ADC when lever is fully PRESSED.
  // Default 0/1023 = passthrough (no remapping) until real values are measured.
  uint16_t calRestA = 0;
  uint16_t calFullA = 1023;
  uint16_t calRestB = 0;
  uint16_t calFullB = 1023;

  // Map raw ADC to 0-1023 using the calibrated endpoints.
  // constrain clamps values outside the measured travel range.
  uint16_t applyCalibration(uint16_t raw, uint16_t calRest, uint16_t calFull)
  {
    if (calRest == calFull)
      return 0; // safety: avoid degenerate mapping
    long mapped = map((long)raw, (long)calRest, (long)calFull, 0L, 1023L);
    return (uint16_t)constrain(mapped, 0, 1023);
  }

public:
  // Set calibration at runtime (called from protocol callback or setup).
  void setCalibration(uint16_t restA, uint16_t fullA, uint16_t restB, uint16_t fullB)
  {
    calRestA = restA;
    calFullA = fullA;
    calRestB = restB;
    calFullB = fullB;
  }

  void begin()
  {
    pinMode(CLUTCH_A_PIN, INPUT);
    pinMode(CLUTCH_B_PIN, INPUT);
    // Default cal = 0/1023 passthrough. main.cpp setup() calls setCalibration()
    // with the CLUTCH_x_CAL_REST/FULL constants from hardwareSettings.h.
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

    // Apply per-channel calibration: raw ADC -> 0-1023 useful range.
    clutchAValue = applyCalibration(sumA / FILTER_SIZE, calRestA, calFullA);
    clutchBValue = applyCalibration(sumB / FILTER_SIZE, calRestB, calFullB);

    // Check if values changed significantly (more than 5 units post-calibration)
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
