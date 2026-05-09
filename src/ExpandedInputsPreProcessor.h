#pragma once
#include <Arduino.h>

// Rotary switch analog input pins
#define ROTARY_A0_PIN A0  // ROT1 — mode selector for encoder routing
#define ROTARY_A1_PIN A1  // ROT2 — simple 12-position rotary
#define ROTARY_A2_PIN A2  // ROT3 — simple 12-position rotary
#define ROTARY_A3_PIN A3  // ROT4 — simple 12-position rotary

// Rotary encoder direct GPIO pins
#define ENCODER_CLK_PIN 2
#define ENCODER_DT_PIN  8
#define ENCODER_SW_PIN  4

// 74HC595 shift register chain (9 chips, 72 bits)
// Outputs feed into 74HC165 inputs on the Pro Micro (MMJoy2) HID side.
#define SR595_DATA_PIN  10  // DS    — serial data
#define SR595_CLOCK_PIN 11  // SH_CP — shift clock
#define SR595_LATCH_PIN 12  // ST_CP — storage/latch clock
#define SR595_OE_PIN     5  // /OE   — output enable (active LOW)
                            // 10kΩ pull-up to VCC on PCB — outputs disabled during power-on / ICSP
#define SR_CHIP_COUNT    9  // 9 × 74HC595 = 72 bits

// Shift register bit layout (72 bits):
//
//   Bits  0-35 : ROT1 encoder outputs — ALL 12 positions × 3 (SW=0, CCW=1, CW=2).
//                Formula: position N (1-based) → SW=bit(N-1)*3, CCW=bit(N-1)*3+1, CW=bit(N-1)*3+2
//                SimHub-dedicated positions never set their bits (events go to serial only).
//                Layout is FIXED regardless of which positions are currently SimHub-dedicated,
//                so the Pro Micro button mapping never changes when SHP: config is updated.
//
//   Bits 36-47 : ROT2 one-hot (bit 36+pos-1 HIGH for active position 1-12)
//   Bits 48-59 : ROT3 one-hot (bit 48+pos-1)
//   Bits 60-71 : ROT4 one-hot (bit 60+pos-1)
//   Bits 72-   : 2 spare bits in byte 8 (unused)
//
// Chip wiring: byte[8] shifted first → chip 9 (farthest from MCU DS pin)
//              byte[0] shifted last  → chip 1 (closest to MCU DS pin)
//
// Byte contents:
//   byte[0] bits  0- 7 : ROT1 pos1 SW/CCW/CW, pos2 SW/CCW/CW, pos3 SW/CCW
//   byte[1] bits  8-15 : ROT1 pos3 CW, pos4 SW/CCW/CW, pos5 SW/CCW/CW, pos6 SW
//   byte[2] bits 16-23 : ROT1 pos6 CCW/CW, pos7 SW/CCW/CW, pos8 SW/CCW/CW, pos9 SW
//   byte[3] bits 24-31 : ROT1 pos9 CCW/CW, pos10 SW/CCW/CW, pos11 SW/CCW/CW, pos12 SW
//   byte[4] bits 32-39 : ROT1 pos12 CCW/CW, ROT2 pos1-6
//   byte[5] bits 40-47 : ROT2 pos7-12, ROT3 pos1-2
//   byte[6] bits 48-55 : ROT3 pos3-10
//   byte[7] bits 56-63 : ROT3 pos11-12, ROT4 pos1-6
//   byte[8] bits 64-71 : ROT4 pos7-12, 2 spare
#define ROT1_SR_BASE  0
#define ROT2_SR_BASE 36
#define ROT3_SR_BASE 48
#define ROT4_SR_BASE 60

// Duration (ms) that CCW/CW encoder event bits are held HIGH in the shift register
#define ENCODER_SR_PULSE_MS 30

// Rotary encoder half-step state machine (exact copy from SimHub's SHRotaryEncoder.h)
#define R_START       0x0
#define DIR_CW        0x10
#define DIR_CCW       0x20
#define HS_R_CCW_BEGIN  0x1
#define HS_R_CW_BEGIN   0x2
#define HS_R_START_M    0x3
#define HS_R_CW_BEGIN_M  0x4
#define HS_R_CCW_BEGIN_M 0x5

static const unsigned char expandedInputsHalfStepsTable[6][4] = {
    // input: 00               01 (CLK hi)       10 (DT hi)        11 (idle)
    {HS_R_START_M,            HS_R_CW_BEGIN,    HS_R_CCW_BEGIN,   R_START},
    {HS_R_START_M | DIR_CCW,  R_START,          HS_R_CCW_BEGIN,   R_START},
    {HS_R_START_M | DIR_CW,   HS_R_CW_BEGIN,    R_START,          R_START},
    {HS_R_START_M,            HS_R_CCW_BEGIN_M, HS_R_CW_BEGIN_M,  R_START},
    {HS_R_START_M,            HS_R_START_M,     HS_R_CW_BEGIN_M,  R_START | DIR_CW},
    {HS_R_START_M,            HS_R_CCW_BEGIN_M, HS_R_START_M,     R_START | DIR_CCW},
};

// ADC thresholds for 12-position resistor-ladder rotary switches.
// All four rotaries use the same 2.7kΩ ladder network (R1–R14).
// Position 1-12 maps to indices 0-11. Each value is the midpoint
// between adjacent measured ADC levels (step ≈85, noise ≤3 counts).
const int ROTARY_THRESHOLDS[12] = {
    126,  // pos  1 — ADC ~84
    211,  // pos  2 — ADC ~169
    296,  // pos  3 — ADC ~254
    380,  // pos  4 — ADC ~339
    465,  // pos  5 — ADC ~423
    549,  // pos  6 — ADC ~507
    634,  // pos  7 — ADC ~592
    719,  // pos  8 — ADC ~677
    805,  // pos  9 — ADC ~762
    891,  // pos 10 — ADC ~848
    979,  // pos 11 — ADC ~935
   1023,  // pos 12 — ADC ~1023
};

class ExpandedInputsPreProcessor
{
private:
    // SimHub-dedicated encoder positions (configurable via SHP: protocol token).
    // Default {8,9,10} — events for these 3 positions are sent to SimHub via serial;
    // events for all other 9 positions are routed to the Pro Micro via 74HC595.
    uint8_t _simhubPositions[3] = {8, 9, 10};

    // ROT1 state (mode selector for encoder routing)
    int lastRotaryPosition = -1;
    unsigned long lastRotaryRead = 0;
    const unsigned long ROTARY_READ_INTERVAL = 10; // ms

    // ROT2/3/4 state (simple rotaries — one-hot SR output)
    int lastRotary2Position = -1;
    int lastRotary3Position = -1;
    int lastRotary4Position = -1;
    unsigned long lastRotary2Read = 0;
    unsigned long lastRotary3Read = 0;
    unsigned long lastRotary4Read = 0;

    // Rotary encoder state machine
    uint8_t encoderLastState = R_START;
    int encoderCounter = 0;
    unsigned long lastEncoderEventTime = 0;
    const unsigned long ENCODER_DEBOUNCE_DELAY = 2; // ms — guard against chatter

    // SW button debounce state
    bool lastSWRawState      = false;
    bool lastSWReportedState = false;
    int  lastSWButtonId      = -1;   // stores baseButtonId (≥100 = SimHub, else = SR bit for SW)
    unsigned long lastSWChangeTime = 0;
    const unsigned long SW_DEBOUNCE_DELAY = 50; // ms

    // 74HC595 output state buffer (72 bits across 9 bytes)
    uint8_t _srState[SR_CHIP_COUNT] = {};

    // Active CCW/CW encoder SR pulse tracker (only one pulse active at a time)
    int8_t _activePulseBit = -1;
    unsigned long _pulseStartTime = 0;

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
        pinMode(ENCODER_DT_PIN,  INPUT_PULLUP);
        pinMode(ENCODER_SW_PIN,  INPUT_PULLUP);
        lastSWRawState      = !digitalRead(ENCODER_SW_PIN);
        lastSWReportedState = lastSWRawState;
        lastSWChangeTime    = millis();
        encoderLastState    = R_START;

        // Safe 74HC595 startup:
        // Pre-load D5 HIGH *before* switching to OUTPUT — ATmega PORT register is 0x00 after
        // reset, so calling pinMode(OUTPUT) first would briefly drive /OE LOW, enabling the
        // 74HC595 with undefined state. digitalWrite() on an INPUT pin writes PORT without
        // touching DDR, so the pin comes up HIGH the instant pinMode(OUTPUT) runs.
        // 1. Pre-load /OE HIGH → pinMode → D5 immediately OUTPUT HIGH, no glitch
        // 2. Shift 0xFF through all chips (_srState=0 → ~0x00=0xFF) while /OE still HIGH
        // 3. Enable /OE — Pro Micro sees all inputs HIGH (= inactive) from the very first cycle
        digitalWrite(SR595_OE_PIN, HIGH); // pre-load PORT HIGH before switching to output
        pinMode(SR595_OE_PIN,    OUTPUT); // D5 → OUTPUT HIGH immediately, /OE never goes LOW
        pinMode(SR595_DATA_PIN,  OUTPUT);
        pinMode(SR595_CLOCK_PIN, OUTPUT);
        pinMode(SR595_LATCH_PIN, OUTPUT);
        writeAllTo595(); // _srState=0 → ~0x00=0xFF → all outputs HIGH = inactive (matches pull-ups)
        digitalWrite(SR595_OE_PIN, LOW);
    }

    void readAll(void (*onButtonChange)(int, byte))
    {
        // ROT1: mode selector — position only, encoder events drive SR bits
        int rotaryPosition = readRotaryPosition();

        // ROT2/3/4: simple rotaries — one-hot SR output on position change
        readRotary2Position();
        readRotary3Position();
        readRotary4Position();

        // Expire active CCW/CW pulse if ENCODER_SR_PULSE_MS has elapsed
        updateSrPulse();

        // Route encoder and SW to SimHub (serial) or Pro Micro (SR) based on ROT1 position
        bool clkState = digitalRead(ENCODER_CLK_PIN);
        bool dtState  = digitalRead(ENCODER_DT_PIN);
        bool swState  = !digitalRead(ENCODER_SW_PIN); // inverted — INPUT_PULLUP
        routeEncoderBasedOnRotary(rotaryPosition, swState, clkState, dtState, onButtonChange);
    }

    int getRotaryPosition()  { return lastRotaryPosition;  }
    int getRotary2Position() { return lastRotary2Position; }
    int getRotary3Position() { return lastRotary3Position; }
    int getRotary4Position() { return lastRotary4Position; }

private:
    // Decode ADC reading on any rotary pin to position 1-12.
    int decodeAnalogPosition(uint8_t pin)
    {
        int adcValue = analogRead(pin);
        for (int pos = 0; pos < 12; pos++)
        {
            if (adcValue <= ROTARY_THRESHOLDS[pos])
                return pos + 1;
        }
        return 12; // fallback if ADC reads above highest threshold
    }

    // ROT1 — 12-position mode selector (A0).
    // Position is tracked here; the SR bits for ROT1 are driven by encoder events, not by position change.
    int readRotaryPosition()
    {
        if (lastRotaryPosition > 0 && millis() - lastRotaryRead < ROTARY_READ_INTERVAL)
            return lastRotaryPosition;
        lastRotaryRead    = millis();
        lastRotaryPosition = decodeAnalogPosition(ROTARY_A0_PIN);
        return lastRotaryPosition;
    }

    // ROT2 — simple 12-position rotary (A1). One-hot SR output on change.
    void readRotary2Position()
    {
        if (lastRotary2Position > 0 && millis() - lastRotary2Read < ROTARY_READ_INTERVAL)
            return;
        lastRotary2Read = millis();
        int newPos = decodeAnalogPosition(ROTARY_A1_PIN);
        if (newPos != lastRotary2Position)
        {
            if (lastRotary2Position > 0)
                setSrBit(ROT2_SR_BASE + lastRotary2Position - 1, false);
            lastRotary2Position = newPos;
            setSrBit(ROT2_SR_BASE + lastRotary2Position - 1, true);
            writeAllTo595();
        }
    }

    // ROT3 — simple 12-position rotary (A2). One-hot SR output on change.
    void readRotary3Position()
    {
        if (lastRotary3Position > 0 && millis() - lastRotary3Read < ROTARY_READ_INTERVAL)
            return;
        lastRotary3Read = millis();
        int newPos = decodeAnalogPosition(ROTARY_A2_PIN);
        if (newPos != lastRotary3Position)
        {
            if (lastRotary3Position > 0)
                setSrBit(ROT3_SR_BASE + lastRotary3Position - 1, false);
            lastRotary3Position = newPos;
            setSrBit(ROT3_SR_BASE + lastRotary3Position - 1, true);
            writeAllTo595();
        }
    }

    // ROT4 — simple 12-position rotary (A3). One-hot SR output on change.
    void readRotary4Position()
    {
        if (lastRotary4Position > 0 && millis() - lastRotary4Read < ROTARY_READ_INTERVAL)
            return;
        lastRotary4Read = millis();
        int newPos = decodeAnalogPosition(ROTARY_A3_PIN);
        if (newPos != lastRotary4Position)
        {
            if (lastRotary4Position > 0)
                setSrBit(ROT4_SR_BASE + lastRotary4Position - 1, false);
            lastRotary4Position = newPos;
            setSrBit(ROT4_SR_BASE + lastRotary4Position - 1, true);
            writeAllTo595();
        }
    }

    // Route encoder and SW outputs based on current ROT1 position.
    // SimHub positions → callback (button IDs ≥ 100) → forwarded to SimHub via serial.
    // 32U4 positions   → SR bits directly (button IDs 0-35 = SR bit indices for SW/CCW/CW).
    void routeEncoderBasedOnRotary(int rotaryPos, bool sw, bool clk, bool dt, void (*onButtonChange)(int, byte))
    {
        // Compute base ID: default (pos-1)*3 is the SR bit base for SW/CCW/CW of that position.
        // Overwritten to ≥100 for SimHub-dedicated positions.
        int baseButtonId = (rotaryPos - 1) * 3;
        for (uint8_t i = 0; i < 3; i++)
        {
            if (rotaryPos == _simhubPositions[i])
            {
                baseButtonId = 100 + i * 3; // slot 0→100-102, slot 1→103-105, slot 2→106-108
                break;
            }
        }
        bool isSimHub = (baseButtonId >= 100);

        // --- SW button debounce ---
        if (sw != lastSWRawState)
        {
            lastSWRawState   = sw;
            lastSWChangeTime = millis();
        }

        if ((millis() - lastSWChangeTime) >= SW_DEBOUNCE_DELAY && lastSWReportedState != lastSWRawState)
        {
            if (isSimHub)
            {
                onButtonChange(baseButtonId, lastSWRawState ? 1 : 0);
            }
            else
            {
                // baseButtonId == (pos-1)*3 == SR bit for SW of this position
                setSrBit((uint8_t)baseButtonId, lastSWRawState);
                writeAllTo595();
            }
            lastSWReportedState = lastSWRawState;
            lastSWButtonId      = baseButtonId;
        }

        // --- SW held while ROT1 selector moves: migrate hold to new button/bit ---
        if (lastSWReportedState && lastSWButtonId != -1 && lastSWButtonId != baseButtonId)
        {
            // Release old
            if (lastSWButtonId >= 100)
                onButtonChange(lastSWButtonId, 0);
            else
                setSrBit((uint8_t)lastSWButtonId, false);

            // Press new
            if (isSimHub)
                onButtonChange(baseButtonId, 1);
            else
                setSrBit((uint8_t)baseButtonId, true);

            writeAllTo595();
            lastSWButtonId = baseButtonId;
        }

        // --- Rotary encoder (CLK and DT from GPIO) ---
        // CCW = baseButtonId+1, CW = baseButtonId+2 (also the SR bit indices for 32U4 routing)
        processRotaryEncoderWithRouting(clk, dt, baseButtonId + 1, baseButtonId + 2, isSimHub, onButtonChange);
    }

    // State machine for the rotary encoder. Routes CCW/CW to SimHub callback or SR pulse.
    void processRotaryEncoderWithRouting(bool clk, bool dt, int ccwButtonId, int cwButtonId, bool isSimHub, void (*onButtonChange)(int, byte))
    {
        uint8_t encoderInput = (dt << 1) | clk;
        uint8_t tableResult  = expandedInputsHalfStepsTable[encoderLastState & 0xf][encoderInput];
        encoderLastState     = tableResult;

        uint8_t direction = tableResult & 0x30;
        if (direction != 0)
        {
            unsigned long now = millis();
            if (now - lastEncoderEventTime >= ENCODER_DEBOUNCE_DELAY)
            {
                if (direction == DIR_CCW)
                {
                    encoderCounter--;
                    if (isSimHub)
                    {
                        onButtonChange(ccwButtonId, 1);
                        onButtonChange(ccwButtonId, 0);
                    }
                    else
                    {
                        triggerSrPulse((uint8_t)ccwButtonId); // ccwButtonId IS the SR CCW bit
                    }
                }
                else if (direction == DIR_CW)
                {
                    encoderCounter++;
                    if (isSimHub)
                    {
                        onButtonChange(cwButtonId, 1);
                        onButtonChange(cwButtonId, 0);
                    }
                    else
                    {
                        triggerSrPulse((uint8_t)cwButtonId); // cwButtonId IS the SR CW bit
                    }
                }
                lastEncoderEventTime = now;
            }
        }
    }

    // Set or clear a single bit in the SR state buffer.
    void setSrBit(uint8_t bitIndex, bool value)
    {
        uint8_t byteIdx = bitIndex / 8;
        uint8_t bitPos  = bitIndex % 8;
        if (byteIdx >= SR_CHIP_COUNT) return;
        if (value)
            _srState[byteIdx] |=  (uint8_t)(1 << bitPos);
        else
            _srState[byteIdx] &= ~(uint8_t)(1 << bitPos);
    }

    // Begin a 30 ms SR pulse on bitIndex for a CCW/CW encoder event.
    // Cancels any still-active pulse from a previous event.
    void triggerSrPulse(uint8_t bitIndex)
    {
        if (_activePulseBit >= 0)
            setSrBit((uint8_t)_activePulseBit, false); // cancel old pulse immediately
        _activePulseBit = (int8_t)bitIndex;
        _pulseStartTime = millis();
        setSrBit(bitIndex, true);
        writeAllTo595();
    }

    // Called every readAll() cycle: expire active CCW/CW pulse when time has elapsed.
    void updateSrPulse()
    {
        if (_activePulseBit >= 0 && (millis() - _pulseStartTime) >= ENCODER_SR_PULSE_MS)
        {
            setSrBit((uint8_t)_activePulseBit, false);
            _activePulseBit = -1;
            writeAllTo595();
        }
    }

    // Shift all 9 SR bytes to the 74HC595 chain and latch outputs.
    // byte[SR_CHIP_COUNT-1] is shifted first → ends up in chip 9 (farthest from MCU).
    // byte[0] is shifted last               → stays in chip 1 (closest to MCU DS pin).
    //
    // Output polarity: active-LOW. 74HC165 inputs are INPUT_PULLUP — idle = HIGH = not pressed.
    // _srState uses active-HIGH internally (1 = active); bytes are bitwise-inverted before
    // shifting so the 74HC595 pulls a line LOW to assert "pressed".
    // Idle state: _srState=0x00 → ~0x00=0xFF → all outputs HIGH → matches pull-ups → clean boot.
    // MMJoy2 button assignments must be configured as active-LOW (inverted) to match.
    void writeAllTo595()
    {
        digitalWrite(SR595_LATCH_PIN, LOW);
        for (int8_t i = SR_CHIP_COUNT - 1; i >= 0; i--)
            shiftOut(SR595_DATA_PIN, SR595_CLOCK_PIN, MSBFIRST, ~_srState[i]);
        digitalWrite(SR595_LATCH_PIN, HIGH);
        digitalWrite(SR595_LATCH_PIN, LOW);
    }
};
