# F1 Wheel Firmware — Architecture Reference

**Project:** Red Bull RB19 DIY Steering Wheel  
**Last updated:** April 2026  
**Status:** Active development

---

## System Overview

Dual-MCU architecture. Responsibilities are split deliberately:

| Board | Chip | Firmware | Role |
|---|---|---|---|
| Arduino Nano | ATmega328P | SimHub "J revision" + custom sketch | Telemetry, LEDs, clutch logic, input preprocessing |
| Arduino Pro Micro | ATmega32U4 | MMJoy2 | HID game controller (Windows sees this as a joystick) |

The Nano is the "smart" middle layer. It talks to SimHub over USB serial, drives the WS2812B strip, processes the dual-clutch sensors, reads the rotary switch, and routes inputs to the Pro Micro. The Pro Micro only handles HID — it has no protocol awareness; it just reads physical inputs and presents them to Windows.

---

## Hardware I/O — Nano Pin Map

| Pin | Signal | Direction | Notes |
|---|---|---|---|
| D0 (RX) | USB Serial RX | In | SimHub comms |
| D1 (TX) | USB Serial TX | Out | SimHub comms |
| D2 | Encoder CLK | In | INPUT_PULLUP |
| D3 | WS2812B DIN | Out | 25 LEDs, GRB encoding |
| D4 | Encoder SW | In | INPUT_PULLUP |
| D5 | 74HC595 /OE | Out | Active LOW. **10kΩ pull-up to VCC on PCB** — outputs disabled during power-on and ICSP |
| D6 | — | — | Unused |
| D7 | — | — | Unused |
| D8 | Encoder DT | In | INPUT_PULLUP |
| D9 | Clutch PWM out | Out | Timer 1, 10-bit Fast PWM, OCR1A. Signal goes to Pro Micro axis input |
| D10 | 74HC595 DS | Out | Serial data |
| D11 | 74HC595 SH_CP | Out | Shift clock (also ICSP MOSI — safe because /OE controls output) |
| D12 | 74HC595 ST_CP | Out | Latch clock (also ICSP MISO — same reason) |
| D13 | — | — | Unused (shares onboard LED) |
| A0 | 12-pos rotary switch | In | 1kΩ resistor ladder, ADC thresholds in `ExpandedInputsPreProcessor.h` |
| A1–A3 | — | — | Unused |
| A4 | Clutch sensor A | In | Hall effect, analog INPUT |
| A5 | Clutch sensor B | In | Hall effect, analog INPUT |

### 74HC595 /OE Boot Sequence

The PCB pull-up keeps /OE HIGH before the Nano runs. The firmware reinforces this on startup:

1. Set D5 HIGH (outputs disabled, belt-and-suspenders with pull-up)
2. `shiftOut` 0x00 to clear any undefined power-on state in the shift register
3. Pulse latch — clean all-zeros is now in storage register
4. Set D5 LOW — outputs enabled; Pro Micro sees a clean 0x00

This prevents garbage on the 74HC165 inputs on the Pro Micro side during both normal power-on and ICSP programming (which clocks D11/D12).

---

## Firmware Modules

### `hardwareSettings.h`
SimHub hardware configuration header. Key active settings:

```cpp
#define DEVICE_NAME "Redbull RB19 Steering Interface Pre-Processor"
#define DEVICE_UNIQUE_ID "f35eabd7-6b75-4e14-812d-6c88668e76fb"

#define WS2812B_DATAPIN 3
#define WS2812B_RGBLEDCOUNT 25
// GRB encoding

#define ENABLED_BUTTONS_COUNT 0   // D12 freed for 74HC595 ST_CP
```

`ENABLED_BUTTONS_COUNT 0` is intentional — it disables SimHub's built-in button polling which would conflict with D12.

---

### `ExpandedInputsPreProcessor.h`

Handles all physical input reading and preprocessing. Instantiated as `expandedInputs` in `hardwareSettings.h`.

**Responsibilities:**
- Read 12-position rotary switch on A0 (ADC + threshold table)
- Read rotary encoder on D2/D8/D4
- Route encoder events to different SimHub button IDs depending on rotary position
- Write rotary position to 74HC595 outputs (lower nibble, 4-bit binary)

**Rotary → 595 encoding:**  
Position 1–12 is written as a 4-bit binary value to QA–QD.  
`position 1 = 0b00000001`, `position 12 = 0b00001100`  
Upper nibble is always zero (reserved for future use).

The 595 outputs feed into 74HC165 inputs on the Pro Micro side. MMJoy2 reads those as button states, giving the Pro Micro awareness of which rotary position is active without any serial protocol between the two boards.

**Button ID routing scheme:**
Each rotary position owns 3 button IDs: `[SW, CCW, CW]`
- Normal positions (1–7, 11–12): `baseId = (pos - 1) * 3`, IDs 0–35
- SimHub-reserved positions (8–10): offset to IDs 100–108 to avoid conflicts

---

### `SHClutchPWM.h`

Thin wrapper around Timer 1 for 10-bit Fast PWM on D9.

- `begin()` — configures Timer 1 (WGM mode 7, COM1A1, CS10, OCR1A=0)
- `setValue(uint16_t)` — constrains to 0–1023, writes OCR1A directly
- `getValue()` — returns current value

**No EEPROM.** PWM is computed in real-time every cycle from sensor readings and bite point. Nothing to persist here.

---

### `SHCustomProtocol.h`

SimHub custom protocol handler. Manages the bi-directional data channel between SimHub and the firmware.

**Receiving from SimHub (via `read()`):**
```
BP:xx.x;MODE:x\n
```
- `BP` — clutch bite point (0.0–100.0), stored in `clutchBitePoint`
- `MODE` — 1 = clutch adjustment mode active, 0 = inactive

**Sending to SimHub (via `idle()`):**  
When `clutchAdjustMode == true`, sends every 100ms:
```
CLT:A:xxx;B:yyy
```
via `FlowSerialDebugPrintLn` (packet type 0x07).  
SimHub surfaces this as `DataCorePlugin.LoggingLastMessage` — the plugin reads it from there.

**Clutch PWM formula:**
```
PWM = (A_pct × (100 - BP)/100) + (B_pct × BP/100)
```
Where A_pct and B_pct are sensor values normalized to 0–100%.  
Result is clamped to 0–1023 (10-bit) and written to OCR1A via `shClutchPWM.setValue()`.

---

### `main.cpp`

Top-level sketch. Notable wiring:

```cpp
void onClutchSensorsChanged(uint16_t clutchA, uint16_t clutchB)
{
    shCustomProtocol.setClutchValues(clutchA, clutchB);
    uint16_t combinedPWM = shCustomProtocol.calculateCombinedPWM(clutchA, clutchB);
    shClutchPWM.setValue(combinedPWM);
}
```

`shDualClutchSensor.read()` fires `onClutchSensorsChanged` from `idle()` every loop.

---

## SimHub Plugin — `F1WheelClutchPlugin_Simple.cs`

Compiled to `F1WheelHardwareConfig.dll` using `csc.exe` (.NET Framework 4.x, C# 5.0).

**What it does:**
- Reads `DataCorePlugin.LoggingLastMessage` each `DataUpdate` tick
- Parses `CLT:A:xxx;B:yyy` to populate `ClutchAValue` / `ClutchBValue` SimHub properties
- Sends `BP:xx.x;MODE:x` to the Arduino when bite point changes or mode toggles
- Persists `_clutchBitePoint` across SimHub restarts via `ReadCommonSettings` / `SaveCommonSettings`

**SimHub properties exposed:**
| Property | Type | Source |
|---|---|---|
| `F1WheelHardwareConfig.ClutchBitePoint` | double | Plugin setting (persisted) |
| `F1WheelHardwareConfig.ClutchAValue` | int | Parsed from Arduino debug log |
| `F1WheelHardwareConfig.ClutchBValue` | int | Parsed from Arduino debug log |
| `F1WheelHardwareConfig.PWMOutput` | int | Computed locally in plugin |
| `F1WheelHardwareConfig.ClutchAdjustMode` | bool | Plugin state |
| `F1WheelHardwareConfig.ArduinoConnected` | bool | SimHub device connection status |

**Bite point persistence:**  
`F1WheelHardwareConfigSettings` class is serialized by SimHub. Loaded in `Init()`, saved in `End()`. Arduino EEPROM is NOT used for bite point.

**Compiler note:**  
C# 5.0 only — no auto-property initializers, no `$""` interpolation, no inline `out` declarations. Use `string.Format()` and separate variable declarations.

**Compile command (from `SimHub Integration/`):**
```powershell
.\compile_plugin.ps1
# Output: .\bin\F1WheelHardwareConfig.dll
# Install: copy to D:\SimHub\Plugins\
```

---

## Data Flow Diagrams

### Clutch Bite Point — Adjustment Cycle

```
[Encoder on wheel]
       |
       | button events (CCW/CW, rotary pos 7 = adjust mode)
       v
[expandedInputs.readAll()]  -->  SimHub button properties
                                         |
                                         | plugin detects mode change
                                         v
                               [Plugin sends BP:xx.x;MODE:1]
                                         |
                                         v
                               [Arduino SHCustomProtocol.read()]
                                         |
                                    clutchBitePoint updated
                                         |
                               [idle() → CLT:A:xxx;B:yyy]  (every 100ms)
                                         |
                                         v
                               [DataCorePlugin.LoggingLastMessage]
                                         |
                                         v
                               [Plugin ReadArduinoData()]
                                         |
                                ClutchAValue / ClutchBValue properties
```

### Clutch PWM — Real-Time Path

```
[Hall sensors A4, A5]
       |
       v
[shDualClutchSensor.read()]
       |
       v
[onClutchSensorsChanged(A, B)]
       |
       v
[calculateCombinedPWM(A, B)]  <-- clutchBitePoint from protocol
       |
       v
[shClutchPWM.setValue(pwm)]
       |
       v
[OCR1A = pwm]  -->  D9 PWM signal  -->  [Pro Micro axis input]
```

### Rotary Switch → Pro Micro

```
[Rotary switch on A0]
       |  ADC read every 10ms
       v
[readRotaryPosition()]
       |  on change only
       v
[writeRotaryTo595(position)]
       |  shiftOut position as 4-bit nibble
       v
[74HC595 QA–QD]  -->  [74HC165 inputs on Pro Micro]  -->  MMJoy2 reads as buttons
```

---

## Build & Flash

**Environment:** PlatformIO (see `platformio.ini`)

```powershell
# From workspace root:
pio run          # compile
pio run -t upload  # compile + flash
```

**Plugin:**
```powershell
cd "SimHub Integration"
.\compile_plugin.ps1
Copy-Item ".\bin\F1WheelHardwareConfig.dll" "D:\SimHub\Plugins\"
# Restart SimHub after copying
```

---

## Status Tracker

| Item | Status |
|---|---|
| Dual clutch PWM (real-time, sensor-driven) | Working |
| Bite point adjustment via encoder | Working |
| Bite point persistence (SimHub plugin settings) | Working |
| ClutchA/B live values in SimHub properties | Working |
| WS2812B shift lights (SimHub-driven) | Working |
| 12-pos rotary ADC decode (all 12 positions) | Working |
| Encoder routing per rotary position | Working |
| 74HC595 pin assignment + /OE safe boot | Implemented, not yet flashed |
| 74HC595 rotary position output to Pro Micro | Implemented, not yet flashed |
| Pro Micro (MMJoy2) reading 74HC165 from 595 | Pending — PCB not built yet |
| PCB design | In progress |
| Remaining rotary positions (buttons via 595 chain) | Pending |
