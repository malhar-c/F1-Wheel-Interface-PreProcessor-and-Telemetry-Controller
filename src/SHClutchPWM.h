#pragma once
#include <Arduino.h>

// 10-bit PWM Clutch Control on Pin 9 (Timer 1)
// Range: 0-1023 (10-bit resolution)
// PWM is computed in real-time from dual clutch sensors + bite point — no persistence needed.
#define CLUTCH_PWM_MIN 0
#define CLUTCH_PWM_MAX 1023

class SHClutchPWM
{
private:
  uint16_t clutchValue = 0;
  const uint8_t CLUTCH_PIN = 9; // Timer 1 Pin A (10-bit PWM)

public:
  void begin()
  {
    pinMode(CLUTCH_PIN, OUTPUT);

    // Configure Timer 1 for 10-bit Fast PWM mode
    // WGM13:10 = 0111 (mode 7: 10-bit Fast PWM)
    // COM1A1:0 = 10 (non-inverted PWM on OC1A/pin9)
    // CS12:10 = 001 (no prescaler, F_CPU clock)
    cli();
    TCCR1A = 0;
    TCCR1B = 0;
    TCCR1A |= (1 << WGM11) | (1 << WGM10);
    TCCR1B |= (1 << WGM12);
    TCCR1A |= (1 << COM1A1);
    TCCR1B |= (1 << CS10);
    OCR1A = 0;
    sei();
  }

  void setValue(uint16_t value)
  {
    clutchValue = constrain(value, CLUTCH_PWM_MIN, CLUTCH_PWM_MAX);
    OCR1A = clutchValue;
  }

  uint16_t getValue()
  {
    return clutchValue;
  }
};
