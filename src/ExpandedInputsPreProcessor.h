#pragma once
#include <Arduino.h>

// Rotary switch on A0 for input routing / mode selection
#define ROTARY_A0_PIN A0

// Rotary encoder on direct GPIO pins (no shift register)
#define ENCODER_CLK_PIN 2
#define ENCODER_DT_PIN 8
#define ENCODER_SW_PIN 4 // SW button on Pin 4

// 74HC595 shift register output expansion
// Outputs are decoded by 74HC165s on the Pro Micro (MMJoy2) HID side
#define SR595_DATA_PIN 10  // DS  - Serial data
#define SR595_CLOCK_PIN 11 // SH_CP - Shift clock
#define SR595_LATCH_PIN 12 // ST_CP - Storage/latch clock
#define SR595_OE_PIN 5     // /OE  - Output enable (active LOW)
                           // Pull-up to VCC on PCB ensures outputs stay
                           // disabled during power-on and ICSP programming

// Rotary encoder definitions
#define R_START 0x0
#define DIR_CW 0x10
#define DIR_CCW 0x20

// Half-step state machine — exact copy from SimHub's SHRotaryEncoder.h.
// Works for both per-detent and half-detent encoders (EC11 / KY-040 and most cheap types).
#define HS_R_CCW_BEGIN 0x1
#define HS_R_CW_BEGIN 0x2
#define HS_R_START_M 0x3
#define HS_R_CW_BEGIN_M 0x4
#define HS_R_CCW_BEGIN_M 0x5

static const unsigned char expandedInputsHalfStepsTable[6][4] = {
    // input: 00 (both low)        01 (CLK hi only)    10 (DT hi only)     11 (both hi / idle)
    {HS_R_START_M, HS_R_CW_BEGIN, HS_R_CCW_BEGIN, R_START},            // 0: R_START
    {HS_R_START_M | DIR_CCW, R_START, HS_R_CCW_BEGIN, R_START},        // 1: HS_R_CCW_BEGIN
    {HS_R_START_M | DIR_CW, HS_R_CW_BEGIN, R_START, R_START},          // 2: HS_R_CW_BEGIN
    {HS_R_START_M, HS_R_CCW_BEGIN_M, HS_R_CW_BEGIN_M, R_START},        // 3: HS_R_START_M
    {HS_R_START_M, HS_R_START_M, HS_R_CW_BEGIN_M, R_START | DIR_CW},   // 4: HS_R_CW_BEGIN_M
    {HS_R_START_M, HS_R_CCW_BEGIN_M, HS_R_START_M, R_START | DIR_CCW}, // 5: HS_R_CCW_BEGIN_M
};

// ADC thresholds for 12-position rotary switch (1kΩ ladder)
// Positions 1-12 corresponding to array indices 0-11
// Based on actual measured ADC values - all 12 positions working correctly
const int ROTARY_THRESHOLDS[12] = {
    46,  // Position 1: 0     (threshold: midpoint to pos 2)
    138, // Position 2: 92    (threshold: midpoint to pos 3)
    231, // Position 3: 185   (threshold: midpoint to pos 4)
    325, // Position 4: 278   (threshold: midpoint to pos 5)
    418, // Position 5: 372   (threshold: midpoint to pos 6)
    511, // Position 6: 465   (threshold: midpoint to pos 7)
    604, // Position 7: 558   (threshold: midpoint to pos 8)
    697, // Position 8: 651   (threshold: midpoint to pos 9)
    789, // Position 9: 743   (threshold: midpoint to pos 10)
    883, // Position 10: 836  (threshold: midpoint to pos 11)
    976, // Position 11: 930  (threshold: midpoint to pos 12)
    1023 // Position 12: 1023 (max ADC value)
};

class ExpandedInputsPreProcessor
{
private:
  // Which 3 rotary positions are routed to SimHub (buttons 100-108).
  // Index 0 → 100-102, index 1 → 103-105, index 2 → 106-108.
  // Updated at runtime via setSimHubPositions() from SHCustomProtocol.
  uint8_t _simhubPositions[3] = {8, 9, 10};

  // Rotary switch state tracking
  int lastRotaryPosition = -1;
  unsigned long lastRotaryRead = 0;
  const unsigned long ROTARY_READ_INTERVAL = 10; // Read rotary every 10ms

  // Rotary encoder state tracking (reading directly from GPIO)
  uint8_t encoderLastState = R_START;
  int encoderCounter = 0;
  unsigned long lastEncoderEventTime = 0;
  const unsigned long ENCODER_DEBOUNCE_DELAY = 2; // Small guard against electrical chatter

  // SW button state tracking
  bool lastSWRawState = false;
  bool lastSWReportedState = false;
  int lastSWButtonId = -1;
  unsigned long lastSWChangeTime = 0;
  const unsigned long SW_DEBOUNCE_DELAY = 50; // 50ms debounce for SW button

public:
  void setSimHubPositions(uint8_t p1, uint8_t p2, uint8_t p3)
  {
    _simhubPositions[0] = p1;
    _simhubPositions[1] = p2;
    _simhubPositions[2] = p3;
  }

  void begin()
  {
    pinMode(ENCODER_CLK_PIN, INPUT_PULLUP);
    pinMode(ENCODER_DT_PIN, INPUT_PULLUP);
    pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
    lastSWRawState = !digitalRead(ENCODER_SW_PIN);
    lastSWReportedState = lastSWRawState;
    lastSWChangeTime = millis();
    encoderLastState = R_START;

    // 74HC595 safe startup sequence:
    // 1. /OE stays HIGH (disabled) — PCB pull-up handles this before firmware runs.
    //    Drive HIGH explicitly in firmware too for belt-and-suspenders.
    // 2. Shift out all-zeros to clear any undefined power-on state.
    // 3. Latch the clean state.
    // 4. Only then drive /OE LOW to enable outputs — Pro Micro sees a clean 0x00.
    pinMode(SR595_OE_PIN, OUTPUT);
    pinMode(SR595_DATA_PIN, OUTPUT);
    pinMode(SR595_CLOCK_PIN, OUTPUT);
    pinMode(SR595_LATCH_PIN, OUTPUT);
    digitalWrite(SR595_OE_PIN, HIGH); // keep outputs disabled
    digitalWrite(SR595_LATCH_PIN, LOW);
    shiftOut(SR595_DATA_PIN, SR595_CLOCK_PIN, MSBFIRST, 0x00);
    digitalWrite(SR595_LATCH_PIN, HIGH); // latch all-zeros
    digitalWrite(SR595_LATCH_PIN, LOW);
    digitalWrite(SR595_OE_PIN, LOW); // enable outputs — clean known state
  }

  // Read encoder and route outputs based on rotary position
  void readAll(void (*onButtonChange)(int, byte))
  {
    // Read rotary switch position on A0 (determines routing mode)
    int rotaryPosition = readRotaryPosition();

    // Read encoder directly from GPIO pins
    bool clkState = digitalRead(ENCODER_CLK_PIN);
    bool dtState = digitalRead(ENCODER_DT_PIN);
    bool swState = !digitalRead(ENCODER_SW_PIN); // Inverted because of pull-up

    // Route encoder and SW outputs based on rotary position
    routeEncoderBasedOnRotary(rotaryPosition, swState, clkState, dtState, onButtonChange);
  }

  // Get current rotary switch position (for external queries like SimHub telemetry)
  int getRotaryPosition()
  {
    return lastRotaryPosition;
  }

private:
  // Read rotary switch position from A0 using resistor ladder
  int readRotaryPosition()
  {
    // Only read rotary every ROTARY_READ_INTERVAL ms for stability
    if (lastRotaryPosition > 0 && millis() - lastRotaryRead < ROTARY_READ_INTERVAL)
    {
      return lastRotaryPosition;
    }
    lastRotaryRead = millis();
    int adcValue = analogRead(ROTARY_A0_PIN);

    // Find closest threshold match and return position 1-12
    for (int pos = 0; pos < 12; pos++)
    {
      if (adcValue <= ROTARY_THRESHOLDS[pos])
      {
        int actualPosition = pos + 1; // Convert from 0-11 to 1-12
        if (lastRotaryPosition != actualPosition)
        {
          lastRotaryPosition = actualPosition;
          writeRotaryTo595(actualPosition);
        }
        return actualPosition;
      }
    }

    // Fallback: if no threshold matched, assume position 12
    lastRotaryPosition = 12;
    return 12;
  }

  // Route encoder and SW outputs based on rotary position
  void routeEncoderBasedOnRotary(int rotaryPos, bool sw, bool clk, bool dt, void (*onButtonChange)(int, byte))
  {
    // Calculate base button ID for this rotary position (each position gets 3 slots: SW, CCW, CW)
    int baseButtonId = (rotaryPos - 1) * 3;

    // For SimHub positions, use special button ID ranges (100-108) to distinguish from 74HC595 routing.
    // Which positions are SimHub-dedicated is configurable via setSimHubPositions().
    for (uint8_t i = 0; i < 3; i++)
    {
      if (rotaryPos == _simhubPositions[i])
      {
        baseButtonId = 100 + i * 3; // slot 0→100-102, slot 1→103-105, slot 2→106-108
        break;
      }
    }

    // Debounce SW based on raw input transitions.
    if (sw != lastSWRawState)
    {
      lastSWRawState = sw;
      lastSWChangeTime = millis();
    }

    if ((millis() - lastSWChangeTime) >= SW_DEBOUNCE_DELAY && lastSWReportedState != lastSWRawState)
    {
      onButtonChange(baseButtonId, lastSWRawState ? 1 : 0);
      lastSWReportedState = lastSWRawState;
      lastSWButtonId = baseButtonId;
    }

    // If the selector changes while SW is held, move the held state to the new routed button id.
    if (lastSWReportedState && lastSWButtonId != -1 && lastSWButtonId != baseButtonId)
    {
      onButtonChange(lastSWButtonId, 0);
      onButtonChange(baseButtonId, 1);
      lastSWButtonId = baseButtonId;
    }

    // Process rotary encoder (CLK and DT from direct GPIO)
    // CCW = baseButtonId + 1, CW = baseButtonId + 2
    processRotaryEncoderWithRouting(clk, dt, baseButtonId + 1, baseButtonId + 2, onButtonChange);
  }

  // Process rotary encoder signals from GPIO pins with position-based routing
  void processRotaryEncoderWithRouting(bool clk, bool dt, int ccwButtonId, int cwButtonId, void (*onButtonChange)(int, byte))
  {
    // Create encoder input state: (DT << 1) | CLK
    uint8_t encoderInput = (dt << 1) | clk;

    // Use SimHub's half-step state machine — fires once per detent on EC11/KY-040 types.
    uint8_t tableResult = expandedInputsHalfStepsTable[encoderLastState & 0xf][encoderInput];
    encoderLastState = tableResult;

    // Extract direction flags from the table result
    uint8_t direction = (tableResult & 0x30);
    if (direction != 0)
    {
      // Debounce encoder events - prevent rapid firing
      unsigned long currentTime = millis();
      if (currentTime - lastEncoderEventTime >= ENCODER_DEBOUNCE_DELAY)
      {
        if (direction == DIR_CCW)
        {
          encoderCounter--;
          // Send encoder event as press/release cycle to position-specific CCW button ID
          onButtonChange(ccwButtonId, 1); // Press
          onButtonChange(ccwButtonId, 0); // Release
        }
        else if (direction == DIR_CW)
        {
          encoderCounter++;
          // Send encoder event as press/release cycle to position-specific CW button ID
          onButtonChange(cwButtonId, 1); // Press
          onButtonChange(cwButtonId, 0); // Release
        }
        lastEncoderEventTime = currentTime;
      }
    }
  }

  // Write a full byte to the 74HC595. The caller is responsible for the value.
  // Latch is pulsed inside — outputs appear immediately.
  void writeTo595(uint8_t value)
  {
    digitalWrite(SR595_LATCH_PIN, LOW);
    shiftOut(SR595_DATA_PIN, SR595_CLOCK_PIN, MSBFIRST, value);
    digitalWrite(SR595_LATCH_PIN, HIGH);
    digitalWrite(SR595_LATCH_PIN, LOW);
  }

private:
  // Encode rotary switch position (1–12) into the lower nibble of the 595 output byte.
  // Upper nibble is reserved (zero). Pro Micro reads QA–QD via 74HC165 as a 4-bit
  // binary number: position 1 = 0b0001, position 12 = 0b1100.
  void writeRotaryTo595(int position)
  {
    uint8_t nibble = (uint8_t)constrain(position, 1, 12);
    writeTo595(nibble); // lower 4 bits carry position; upper 4 reserved zeros
  }
};