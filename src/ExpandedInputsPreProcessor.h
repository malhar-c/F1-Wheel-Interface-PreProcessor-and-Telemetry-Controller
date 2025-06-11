#pragma once
#include <Arduino.h>

// Pin definitions for 74HC165 (adjust as needed)
#define SHIFT_LOAD_PIN 7         // PL (parallel load, active low)
#define SHIFT_CLOCK_ENABLE_PIN 4 // CE (clock enable, active low)
#define SHIFT_DATA_PIN 5         // Q7 (serial data out)
#define SHIFT_CLOCK_PIN 6        // CP (clock)

// Rotary switch on A0 for input routing
#define ROTARY_A0_PIN A0

#define EXPANDED_BUTTONS_COUNT 8                         // Number of buttons per shift register
const unsigned long EXPANDED_BUTTON_DEBOUNCE_DELAY = 50; // 50ms debounce delay

// Rotary encoder definitions
#define R_START 0x0
#define DIR_CW 0x10
#define DIR_CCW 0x20

// State definitions for Half-Step encoder logic
#define HS_CW_INT 0x1  // Half-step Clockwise Intermediate
#define HS_CCW_INT 0x2 // Half-step Counter-Clockwise Intermediate

// Half-step table for rotary encoder
static const unsigned char expandedInputsHalfStepsTable[3][4] = {
    // Current State is R_START (0)
    // Input (DT CLK):     00              01 (CLK)        10 (DT)         11
    /* R_START (0) */ {R_START, HS_CW_INT, HS_CCW_INT, R_START},

    // Current State is HS_CW_INT (1) - (From R_START, CLK went high, input was 01)
    // Input (DT CLK):     00 (CLK low)    01 (no change)  10 (DT high)    11 (DT high)
    /* HS_CW_INT (1) */ {R_START | DIR_CW, HS_CW_INT, R_START, R_START | DIR_CW},

    // Current State is HS_CCW_INT (2) - (From R_START, DT went high, input was 10)
    // Input (DT CLK):     00 (DT low)     01 (CLK high)   10 (no change)  11 (CLK high)
    /* HS_CCW_INT (2) */ {R_START | DIR_CCW, R_START, HS_CCW_INT, R_START | DIR_CCW}};

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
  uint8_t lastRawState = 0; // Stores the last raw state of the shift register
  byte lastReportedButtonState[EXPANDED_BUTTONS_COUNT] = {0};
  unsigned long lastDebounceTime[EXPANDED_BUTTONS_COUNT] = {0};
  // Rotary switch state tracking
  int lastRotaryPosition = -1;
  int lastRotaryBaseButtonId = -1;
  unsigned long lastRotaryRead = 0;
  const unsigned long ROTARY_READ_INTERVAL = 10; // Read rotary every 10ms
  // Rotary encoder state tracking (D1=CLK, D2=DT from shift register)
  uint8_t encoderLastState = 0;
  int encoderCounter = 0;

  // Encoder debouncing
  unsigned long lastEncoderEventTime = 0;
  const unsigned long ENCODER_DEBOUNCE_DELAY = 5; // 5ms debounce for encoder events

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
  } // Reads all buttons and calls the callback if state changes
  void readAll(void (*onButtonChange)(int, byte))
  {
    uint8_t currentRawState = readShiftRegister(); // Debug: Print raw shift register state changes (COMMENTED OUT TO REDUCE SERIAL TRAFFIC)
    // static uint8_t lastDebugRawState = 0xFF; // Initialize to impossible value
    // if (currentRawState != lastDebugRawState)
    // {
    //   FlowSerialDebugPrintLn("Shift Register Raw: 0b" + String(currentRawState, BIN) + " (0x" + String(currentRawState, HEX) + ")");
    //   lastDebugRawState = currentRawState;
    // }

    // Read rotary switch position on A0
    int rotaryPosition = readRotaryPosition();

    // Extract D0, D1, D2 states from shift register
    bool d0_state = !((currentRawState >> 1) & 0x01); // D0 from bit 1, inverted
    bool d1_state = !((currentRawState >> 2) & 0x01); // D1 from bit 2, inverted
    bool d2_state = !((currentRawState >> 3) & 0x01); // D2 from bit 3, inverted    // Debug: Print extracted D0, D1, D2 states (COMMENTED OUT TO REDUCE SERIAL TRAFFIC)
    // static bool lastD0Debug = false, lastD1Debug = false, lastD2Debug = false;
    // if (d0_state != lastD0Debug || d1_state != lastD1Debug || d2_state != lastD2Debug)
    // {
    //   FlowSerialDebugPrintLn("Extracted - D0: " + String(d0_state) + " D1(CLK): " + String(d1_state) + " D2(DT): " + String(d2_state));
    //   lastD0Debug = d0_state;
    //   lastD1Debug = d1_state;
    //   lastD2Debug = d2_state;
    // }

    // Route D0, D1, D2 based on rotary position
    routeInputsBasedOnRotary(rotaryPosition, d0_state, d1_state, d2_state, onButtonChange);

    // Process remaining buttons (D3-D7) normally
    for (int i = 4; i < EXPANDED_BUTTONS_COUNT; i++) // Skip D0,D1,D2 (processed above)
    {
      bool currentButtonPhysicalState = (currentRawState >> i) & 0x01;

      if (currentButtonPhysicalState != ((lastRawState >> i) & 0x01))
      {
        lastDebounceTime[i] = millis();
      }

      if ((millis() - lastDebounceTime[i]) > EXPANDED_BUTTON_DEBOUNCE_DELAY)
      {
        if (currentButtonPhysicalState != lastReportedButtonState[i])
        {
          byte invertedState = !currentButtonPhysicalState;
          onButtonChange(i + 100, invertedState);
          lastReportedButtonState[i] = currentButtonPhysicalState;
        }
      }
    }
    lastRawState = currentRawState;
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
  // Read rotary switch position from A0 using resistor ladder
  int readRotaryPosition()
  {

    // Only read rotary every ROTARY_READ_INTERVAL ms for stability
    if (millis() - lastRotaryRead < ROTARY_READ_INTERVAL)
    {
      return lastRotaryPosition;
    }
    lastRotaryRead = millis();
    int adcValue = analogRead(ROTARY_A0_PIN);
    // FlowSerialDebugPrintLn("Rotary A0 ADC value: " + String(adcValue)); // DEBUG

    // Find closest threshold match and return position 1-12
    for (int pos = 0; pos < 12; pos++)
    {
      if (adcValue <= ROTARY_THRESHOLDS[pos])
      {
        int actualPosition = pos + 1; // Convert from 0-11 to 1-12
        if (lastRotaryPosition != actualPosition)
        {
          // FlowSerialDebugPrintLn("Rotary A0 Position: " + String(actualPosition) + " (ADC: " + String(adcValue) + ")");
          lastRotaryPosition = actualPosition;
        }
        return actualPosition;
      }
    }

    // Fallback: if no threshold matched, assume position 12
    if (lastRotaryPosition != 12)
    {
      // FlowSerialDebugPrintLn("Rotary A0 Position: 12 (ADC: " + String(adcValue) + " - fallback)");
      lastRotaryPosition = 12;
    }
    return 12;
  } // Route D0, D1, D2 inputs based on A0 rotary position
  void routeInputsBasedOnRotary(int rotaryPos, bool d0, bool d1, bool d2, void (*onButtonChange)(int, byte))
  {
    static bool lastD0 = false;
    static int lastMappedRotaryPos = -1;

    // Check if we're in SimHub routing mode (positions 8, 9, 10)
    if (rotaryPos == 8 || rotaryPos == 9 || rotaryPos == 10)
    {
      // Calculate base button ID for this rotary position
      int baseButtonId = 100 + (rotaryPos - 8) * 3; // Pos 8=100, Pos 9=103, Pos 10=106

      // If rotary position changed, update debug info
      if (lastMappedRotaryPos != rotaryPos)
      {
        FlowSerialDebugPrintLn("SimHub Mode - Rotary " + String(rotaryPos) + " -> Buttons " + String(baseButtonId) + "-" + String(baseButtonId + 2));
        lastMappedRotaryPos = rotaryPos;
      }

      // Send D0 as button (baseButtonId + 0)
      if (d0 != lastD0)
      {
        onButtonChange(baseButtonId, d0 ? 1 : 0);
        lastD0 = d0;
      }

      // Process rotary encoder with position-specific button IDs
      // CCW = baseButtonId + 1, CW = baseButtonId + 2
      processRotaryEncoderWithRouting(d1, d2, baseButtonId + 1, baseButtonId + 2, onButtonChange);
    }
    else
    {
      // Non-SimHub positions - for now, ignore (later route to 74HC595)
      if (lastMappedRotaryPos != -2) // -2 indicates "non-SimHub mode"
      {
        FlowSerialDebugPrintLn("Non-SimHub Mode - Rotary " + String(rotaryPos) + " (D0,D1,D2 ignored for now)");
        lastMappedRotaryPos = -2;
      }

      // TODO: Later implement 74HC595 output routing here
      // For now, just track state changes without sending to SimHub
      lastD0 = d0;
      // D1, D2 used for encoder - no need to track separately
    }
  } // Process rotary encoder signals from D1 (CLK) and D2 (DT) with position-based routing
  void processRotaryEncoderWithRouting(bool clk, bool dt, int ccwButtonId, int cwButtonId, void (*onButtonChange)(int, byte))
  {
    // Create encoder input state: (DT << 1) | CLK
    uint8_t encoderInput = (dt << 1) | clk;

    // Debug: Print state table progression
    uint8_t oldState = encoderLastState;
    uint8_t tableResult = expandedInputsHalfStepsTable[encoderLastState & 0xf][encoderInput];
    encoderLastState = tableResult;

    if (oldState != encoderLastState)
    {
      // FlowSerialDebugPrintLn("Encoder: " + String(oldState) + "->" + String(encoderLastState & 0xF) + " dir:0x" + String(tableResult & 0x30, HEX));
    }

    // Extract direction flags from the raw table result
    uint8_t direction = (tableResult & 0x30);
    if (direction != 0)
    {
      // FlowSerialDebugPrintLn("Direction: " + String(direction == DIR_CW ? "CW" : "CCW"));

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
          // FlowSerialDebugPrintLn("Encoder CCW - Counter: " + String(encoderCounter) + " -> Button " + String(ccwButtonId));
        }
        else if (direction == DIR_CW)
        {
          encoderCounter++;
          // Send encoder event as press/release cycle to position-specific CW button ID
          onButtonChange(cwButtonId, 1); // Press
          onButtonChange(cwButtonId, 0); // Release
          // FlowSerialDebugPrintLn("Encoder CW - Counter: " + String(encoderCounter) + " -> Button " + String(cwButtonId));
        }
        lastEncoderEventTime = currentTime;
      }
    }
  }

  // Process rotary encoder signals from D1 (CLK) and D2 (DT) - legacy function for non-routed usage
  void processRotaryEncoder(bool clk, bool dt, void (*onButtonChange)(int, byte))
  { // Debug: Print raw encoder states (COMMENTED OUT TO REDUCE SERIAL TRAFFIC)
    // static bool lastClk = false, lastDt = false;
    // if (clk != lastClk || dt != lastDt)
    // {
    //   FlowSerialDebugPrintLn("Encoder Raw - CLK: " + String(clk) + " DT: " + String(dt));
    //   lastClk = clk;
    //   lastDt = dt;
    // }

    // Create encoder input state: (DT << 1) | CLK
    uint8_t encoderInput = (dt << 1) | clk;

    // Debug: Print state table progression
    uint8_t oldState = encoderLastState;                                                      // Process through state table
    uint8_t tableResult = expandedInputsHalfStepsTable[encoderLastState & 0xf][encoderInput]; // Use the new half-step table
    encoderLastState = tableResult;                                                           // Debug: Print state transitions with minimal detail
    if (oldState != encoderLastState)
    {
      // FlowSerialDebugPrintLn("Encoder: " + String(oldState) + "->" + String(encoderLastState & 0xF) + " dir:0x" + String(tableResult & 0x30, HEX));
    } // Extract direction flags from the raw table result
    uint8_t direction = (tableResult & 0x30); // Debug: Print direction detection (SIMPLIFIED)
    if (direction != 0)
    {
      // FlowSerialDebugPrintLn("Direction: " + String(direction == DIR_CW ? "CW" : "CCW"));

      // Debounce encoder events - prevent rapid firing
      unsigned long currentTime = millis();
      if (currentTime - lastEncoderEventTime >= ENCODER_DEBOUNCE_DELAY)
      {
        if (direction == DIR_CCW)
        {
          encoderCounter--;
          // Send encoder event as press/release cycle: ID=200 for CCW direction
          onButtonChange(200, 1); // Press
          onButtonChange(200, 0); // Release
          // FlowSerialDebugPrintLn("Encoder CCW - Counter: " + String(encoderCounter));
        }
        else if (direction == DIR_CW)
        {
          encoderCounter++;
          // Send encoder event as press/release cycle: ID=201 for CW direction
          onButtonChange(201, 1); // Press
          onButtonChange(201, 0); // Release
          // FlowSerialDebugPrintLn("Encoder CW - Counter: " + String(encoderCounter));
        }
        lastEncoderEventTime = currentTime;
      }
    }
  }
};