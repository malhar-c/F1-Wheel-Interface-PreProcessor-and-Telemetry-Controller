# F1 Wheel Firmware — Architecture Reference

**Project:** Red Bull RB19 DIY Steering Wheel  
**Last updated:** May 4, 2026  
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
| A0 | Rotary switch 1 (12-pos) | In | 2.7kΩ resistor ladder, ADC thresholds in `ExpandedInputsPreProcessor.h` |
| A1 | Rotary switch 2 (12-pos) | In | Future: duplicate of A0 |
| A2 | Rotary switch 3 (12-pos) | In | Future: duplicate of A0 |
| A3 | Rotary switch 4 (12-pos) | In | Future: duplicate of A0 |
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

// Hall sensor calibration boot defaults (see calibration section below)
#define CLUTCH_A_CAL_REST  607
#define CLUTCH_A_CAL_FULL  862
#define CLUTCH_B_CAL_REST  609
#define CLUTCH_B_CAL_FULL  868
```

`ENABLED_BUTTONS_COUNT 0` is intentional — it disables SimHub's built-in button polling which would conflict with D12.

The `CLUTCH_x_CAL_*` defines are the **boot defaults** applied at `setup()` before SimHub connects. Once connected, the plugin sends live cal values via the protocol expression and these are overridden in RAM. No EEPROM involved.

---

### `ExpandedInputsPreProcessor.h`

Handles all physical input reading and preprocessing. Instantiated as `expandedInputs` in `hardwareSettings.h`.

**Responsibilities:**
- Read 12-position rotary switches on A0–A3 (ADC + threshold table). Currently A0 implemented; A1–A3 are future expansion.
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
- Default positions (1–7, 11–12): `baseId = (pos - 1) * 3`, IDs 0–35 → routed to Pro Micro via 74HC595
- SimHub positions (default 8–10, configurable): offset to IDs 100–108 → forwarded to SimHub only

Which 3 positions are SimHub-dedicated is runtime-configurable via the `SHP:` protocol token (see `SHCustomProtocol.h`). Defaults to `{8, 9, 10}` if no `SHP:` token is ever received — backward-compatible with the old hardcoded behaviour. Configured via the **Rotary Config** tab in the SimHub plugin.

---

### `SHClutchPWM.h`

Thin wrapper around Timer 1 for 10-bit Fast PWM on D9.

- `begin()` — configures Timer 1 (WGM mode 7, COM1A1, CS10, OCR1A=0)
- `setValue(uint16_t)` — constrains to 0–1023, writes OCR1A directly
- `getValue()` — returns current value

**No EEPROM.** PWM is computed in real-time every cycle from sensor readings and bite point. Nothing to persist here.

---

### `SHDualClutchSensor.h`

Reads Hall effect sensors on A4/A5 with a 4-sample moving average filter, then applies per-channel calibration before passing values upstream.

**SS49E behaviour:** Output rests at Vcc/2 (~607–610 ADC on 5V). Raw values never reach 0 or 1023. Without calibration the clutch axis is permanently offset and has reduced range.

**Calibration:** `applyCalibration(raw, calRest, calFull)` uses Arduino's `map()` to stretch `[calRest, calFull]` → `[0, 1023]`. `constrain()` clamps anything outside the measured travel.  
Works for both magnet orientations — `calFull` can be less than `calRest` if pressing moves the ADC downward.

- `setCalibration(restA, fullA, restB, fullB)` — sets cal values in RAM, called from two places:
  1. `main.cpp` `setup()` — applies `CLUTCH_x_CAL_*` defines as boot defaults
  2. `onCalibrationReceived()` callback — fires every time SimHub sends `RA/FA/RB/FB` fields

**No EEPROM.** Cal values live in RAM only.

---

### `SHCustomProtocol.h`

SimHub custom protocol handler. Manages the bi-directional data channel between SimHub and the firmware.

**Receiving from SimHub (via `read()`):**

Full line is read at once with `FlowSerialReadStringUntil('\n')` then parsed by token:

```
BP:xx.x;MODE:x;RA:nnn;FA:nnn;RB:nnn;FB:nnn;SHP:p1,p2,p3
```

| Token | Meaning | Required |
|---|---|---|
| `BP` | Clutch bite point (0.0–100.0) | Yes |
| `MODE` | 1 = clutch adjustment mode active | Yes |
| `RA` | Calibration REST value, sensor A | Optional |
| `FA` | Calibration FULL value, sensor A | Optional |
| `RB` | Calibration REST value, sensor B | Optional |
| `FB` | Calibration FULL value, sensor B | Optional |
| `SHP` | Three SimHub rotary position slots, comma-separated (e.g. `8,9,10`) | Optional |

`RA/FA/RB/FB` are optional and backward-compatible — if absent the calibration callback is not fired. When present, `onCalibrationReceived()` in `main.cpp` calls `shDualClutchSensor.setCalibration()`.

`SHP` is optional and backward-compatible — if absent, SimHub positions default to `{8, 9, 10}`. When present, all three values must be distinct and in range 1–12, otherwise the token is silently ignored. Parsed values are applied to `ExpandedInputsPreProcessor` each debounce cycle via `main.cpp`.

**Sending to SimHub (via `idle()`):**  
When `clutchAdjustMode == true`, sends every 100ms:
```
CLT:A:xxx;B:yyy
```
via `FlowSerialDebugPrintLn` (packet type 0x07).

**Rotary position:**
```
ROT1:n
```
Three send paths (see "Rotary Position Delivery" section for full rationale):

1. `read()` — sent immediately when SimHub sends its first 'P' command after connecting/reconnecting. This is the reliable boot trigger.
2. `idle()` — sent every 5 seconds as a periodic heartbeat, regardless of position changes or clutch mode.
3. `main.cpp` debounce — sent immediately on any position change.

The plugin receives these via `PluginManager.OnArduinoMessage` event (not via `LoggingLastMessage`).

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
// Boot: apply compile-time cal defaults before SimHub connects
shDualClutchSensor.setCalibration(CLUTCH_A_CAL_REST, CLUTCH_A_CAL_FULL,
                                   CLUTCH_B_CAL_REST, CLUTCH_B_CAL_FULL);

// Runtime: SimHub protocol sends live cal values each cycle
void onCalibrationReceived(uint16_t restA, uint16_t fullA, uint16_t restB, uint16_t fullB)
{
    shDualClutchSensor.setCalibration(restA, fullA, restB, fullB);
}

// Sensor callback: calibrated values flow straight to PWM
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

Compiled to `F1WheelHardwareConfig.dll` using `csc.exe` (.NET Framework 4.x, C# 5.0). Current version: **v3.4.0**.

**What it does:**
- Subscribes to `PluginManager.OnArduinoMessage` event in `Init()` — receives Arduino debug messages (packet 0x07) directly with `SerialDash.DeviceDetails` attached. This is more reliable than polling `LoggingLastMessage` which is a shared, busy channel overwritten by SimHub's own internal log messages every ~2 seconds.
- Parses `CLT:A:xxx;B:yyy` from the event to populate `ClutchAValue` / `ClutchBValue` (only when adjust mode active)
- Parses `ROT1:n` from the event to populate `Rotary1Position`
- Determines `ArduinoConnected` from: `DeviceDetails.InUse` (live SerialDash state) OR `lastMessageReceived < 10 seconds ago`
- Sends the full protocol string to Arduino via the SimHub device custom protocol expression
- Persists bite point and calibration values across SimHub restarts via `ReadCommonSettings` / `SaveCommonSettings`

**Why `OnArduinoMessage` instead of `LoggingLastMessage`:**  
`DataCorePlugin.LoggingLastMessage` is overwritten by every SimHub log entry — the AC process scanner alone fires every ~2 seconds with "Searching for AC process" / "AC process not found". A single `ROT1:n` message sent by the Arduino would be replaced before the plugin's `DataUpdate` tick ran, losing the data permanently (since ROT1 is event-driven, not streamed). `OnArduinoMessage` delivers the message directly to the handler on SimHub's serial-read thread, with no race condition.

**Connection detection:**  
`DeviceDetails.InUse` (from SerialDash internals) was found to always be `false` for this device type — it appears to track something other than physical connection. Connection is therefore determined entirely by `_lastMessageReceived < 10 seconds`. The 5-second heartbeat from the firmware keeps this timestamp fresh during idle periods.

**Known limitation:**  
Disconnect is detected after up to 10 seconds of silence (one missed heartbeat). This is a design trade-off: the heartbeat (5-second `ROT1:n`) adds ~0.01% serial bandwidth but is necessary for reliable connection tracking. See "Rotary Position Delivery" section for alternatives if this becomes a problem.

**SimHub properties exposed:**
| Property | Type | Source |
|---|---|---|
| `F1WheelHardwareConfigPlugin.ClutchBitePoint` | double | Plugin setting (persisted) |
| `F1WheelHardwareConfigPlugin.ClutchAValue` | int | Parsed from Arduino debug log |
| `F1WheelHardwareConfigPlugin.ClutchBValue` | int | Parsed from Arduino debug log |
| `F1WheelHardwareConfigPlugin.PWMOutput` | int | Computed locally in plugin |
| `F1WheelHardwareConfigPlugin.ClutchAdjustmentMode` | bool | Plugin state |
| `F1WheelHardwareConfigPlugin.ArduinoConnected` | bool | SimHub device connection status |
| `F1WheelHardwareConfigPlugin.Rotary1Position` | int | Rotary switch 1 position (1–12), 0 when disconnected |
| `F1WheelHardwareConfigPlugin.CalRestA` | int | Calibration REST endpoint, sensor A (persisted) |
| `F1WheelHardwareConfigPlugin.CalFullA` | int | Calibration FULL endpoint, sensor A (persisted) |
| `F1WheelHardwareConfigPlugin.CalRestB` | int | Calibration REST endpoint, sensor B (persisted) |
| `F1WheelHardwareConfigPlugin.CalFullB` | int | Calibration FULL endpoint, sensor B (persisted) |
| `F1WheelHardwareConfigPlugin.SimHubPos1` | int | Rotary position slot 1 → buttons 100-102 (persisted, default 8) |
| `F1WheelHardwareConfigPlugin.SimHubPos2` | int | Rotary position slot 2 → buttons 103-105 (persisted, default 9) |
| `F1WheelHardwareConfigPlugin.SimHubPos3` | int | Rotary position slot 3 → buttons 106-108 (persisted, default 10) |

**Persistence:**  
`F1WheelHardwareConfigSettings` is serialized by SimHub. Holds bite point, all 4 cal endpoints, and the 3 SimHub rotary position slots. Loaded in `Init()`, saved via `SaveSettings()` (called from `End()`, `SetCalibration()`, and `SetSimHubPositions()`). Arduino EEPROM is NOT used for any of this.

**Compiler note:**  
C# 5.0 only — no auto-property initializers, no `$""` interpolation, no inline `out` declarations. Use `string.Format()` and separate variable declarations.

**Compile command (from `SimHub Integration/`):**
```powershell
.\compile_plugin.ps1
# Output: .\bin\F1WheelHardwareConfig.dll
# Install: copy to D:\SimHub\Plugins\
```

`compile_plugin.ps1` embeds `assets\wheel rotary switches section.png` as a manifest resource (`F1WheelClutchPlugin.assets.wheel_rotary_switches_section.png`) via the `/resource:` flag — the `.csproj` is not used for building.

---

## SimHub Device Custom Protocol Expression

Paste this **once** into SimHub → Hardware → [Your Device] → Custom Protocol. Never needs editing again — all values are live plugin properties re-evaluated every protocol cycle.

```
'BP:' + format([F1WheelHardwareConfigPlugin.ClutchBitePoint], '0.0') + ';MODE:' + if([F1WheelHardwareConfigPlugin.ClutchAdjustmentMode], '1', '0') + ';RA:' + [F1WheelHardwareConfigPlugin.CalRestA] + ';FA:' + [F1WheelHardwareConfigPlugin.CalFullA] + ';RB:' + [F1WheelHardwareConfigPlugin.CalRestB] + ';FB:' + [F1WheelHardwareConfigPlugin.CalFullB] + ';SHP:' + [F1WheelHardwareConfigPlugin.SimHubPos1] + ',' + [F1WheelHardwareConfigPlugin.SimHubPos2] + ',' + [F1WheelHardwareConfigPlugin.SimHubPos3]
```

The `SHP:` token at the end carries the three configurable SimHub rotary position slots. Changing them in the **Rotary Config** plugin tab takes effect on the next protocol cycle — no reflash needed. The expression can always be copied fresh from the **Calibration** tab in the plugin UI.

**Fallback behaviour:** if the old expression (without `SHP:`) is still pasted in SimHub, the firmware silently uses the default positions `{8, 9, 10}`. No errors, no broken behaviour.

---

## Rotary Position Delivery — Design Notes

This section documents a hard-won design decision. Do not simplify without reading this first.

### Why it's complex

`ROT1:n` is an event-driven message — the Arduino only sends it when the position changes. The plugin needs to know the current position as soon as SimHub connects, even if the rotary hasn't moved since the Arduino booted. Several approaches were tried and failed:

**Failed: `LoggingLastMessage` polling**  
`DataCorePlugin.LoggingLastMessage` is SimHub's general-purpose log channel. It is overwritten every ~2 seconds by SimHub's own internal messages (e.g., "Searching for AC process"). A single `ROT1:5` is replaced before the plugin's `DataUpdate` tick can read it. Attempts to send ROT1 multiple times (burst, boot window) were also unreliable for the same reason.

**Failed: `PluginManager.GetAllDevices()`**  
This returns `DeviceInstance` objects from the *Devices* tab (monitors, VoCore, etc.). The Arduino lives under the *Arduino* tab and is managed by `SerialDash.dll`, a completely different subsystem.

**Failed: `PluginManager.GetPlugin<MultipleSerialDashController>()`**  
`GetPlugin<T>()` has a generic constraint `where T : IPlugin`. `MultipleSerialDashController` does not implement `IPlugin` and is not registered with the plugin system.

### What actually works

**`PluginManager.OnArduinoMessage` event** (discovered via reflection on `SimHub.Plugins.dll`):
```
PluginManager.OnArduinoMessage
delegate: (SerialDash.DeviceDetails details, string message) -> void
```
This event fires on SimHub's serial-read thread whenever an Arduino sends a debug message (packet 0x07, sent by `FlowSerialDebugPrintLn`). `DeviceDetails.UniqueId` identifies the source device. Messages cannot be overwritten — each fires exactly once to all subscribers.

**`read()` trigger for boot/reconnect:**  
`SHCustomProtocol::read()` is called only when SimHub sends a 'P' command (custom protocol). The first 'P' command after connecting is the exact moment SimHub is guaranteed to be ready to receive debug messages. Sending `ROT1:n` directly from `read()` on first call (`_lastReadMs == 0`) or after a 3-second gap (reconnect) is more reliable than any flag or idle()-based approach.

**5-second heartbeat from `idle()`:**  
The plugin detects disconnect via a 10-second timeout on `_lastMessageReceived`. Without a periodic send, an idle wheel (no position changes, not in adjust mode) triggers a false disconnect after 10 seconds. The heartbeat prevents this. At 7 bytes every 5 seconds, it adds ~0.01% load on the 115200 baud link — negligible.

### If you need to remove the heartbeat in future

If serial bandwidth becomes a concern (e.g., expanding to 4 rotary switches with frequent updates):

1. Explore whether `DeviceDetails.InUse` can be made reliable — if it reflects live connection state, the timeout mechanism becomes unnecessary.
2. Alternatively, use `PluginManager.GetPluginInterface<T>()` (no generic constraint) to try accessing `MultipleSerialDashController` directly and read `ConnectedDevices`. This was not investigated fully.
3. The heartbeat interval can be increased (e.g., to 30 seconds) with a matching plugin timeout increase, reducing bandwidth with a longer disconnect detection window.

### Current parameters

| Parameter | Value | Where |
|---|---|---|
| `read()` reconnect gap threshold | 3 seconds | `SHCustomProtocol.h::read()` |
| Heartbeat interval | 5 seconds | `SHCustomProtocol.h::idle()` |
| Plugin disconnect timeout | 10 seconds | `F1WheelClutchPlugin_Simple.cs::DataUpdate()` |
| `SerialDash.dll` added to compile refs | Yes | `compile_plugin.ps1` |

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
                               [idle() → CLT:A:xxx;B:yyy]  (every 100ms, only when adjust mode active)
                               [sendRotaryPosition() → ROT1:n]  (on boot and on change only)
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
[shDualClutchSensor.read()]  -- 4-sample moving average
       |
       v
[applyCalibration(raw, calRest, calFull)]  -- map() to 0-1023
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
[Rotary switches on A0–A3]
       |  ADC read every 10ms (A0 active now; A1–A3 future)
       v
[readRotaryPosition() for each channel]
       |  on change only
       v
[writeRotaryTo595(position)]
       |  shiftOut position as 4-bit nibble
       v
[74HC595 QA–QD]  -->  [74HC165 inputs on Pro Micro]  -->  MMJoy2 reads as buttons
```

---

## Hall Sensor Calibration

**Why it's needed:** SS49E outputs Vcc/2 (~607–610 ADC) at rest. Without calibration the lever never reads 0 and has reduced effective range. Calibration stretches the actual travel range to the full 0–1023 scale.

**How it works (two-stage):**

1. **Boot default** — `CLUTCH_x_CAL_*` defines in `hardwareSettings.h` applied at `setup()` before SimHub connects. These are the values measured during initial calibration and baked in so the clutch works correctly from the first moment even before SimHub sends anything.

2. **Runtime override** — Every protocol message from SimHub contains `RA/FA/RB/FB` fields. The Arduino overwrites the cal values in RAM each cycle. So updating calibration in the plugin UI takes effect immediately without reflashing.

**To recalibrate:**
1. Go to SimHub → F1 Wheel Config → **Calibration** tab
2. Leave levers fully released → click **Capture** next to REST for each
3. Pull each lever fully pressed → click **Capture** next to FULL
4. Click **Save Calibration** — values are live immediately
5. Optionally update the `#defines` in `hardwareSettings.h` and reflash so the boot default matches (avoids a brief wrong-calibration window before SimHub connects)

**Measured values (current hardware):**

| | REST | FULL |
|---|---|---|
| Sensor A | 607 | 862 |
| Sensor B | 609 | 868 |

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
| Hall sensor calibration (boot defaults + runtime via protocol) | Working |
| WS2812B shift lights (SimHub-driven) | Working |
| 12-pos rotary ADC decode (all 12 positions) | Working |
| Encoder routing per rotary position | Working |
| Rotary positions 1-7, 11-12 suppressed from SimHub (74HC595 path only) | Working — `expandedButtonChanged` filter in `main.cpp` |
| SimHub rotary positions configurable from plugin UI | Working — Rotary Config tab, persisted, sent as `SHP:` token; defaults to {8,9,10} if token absent |
| Rotary1Position in SimHub Diagnostics tab (on boot + on change) | Working — via `OnArduinoMessage` event + `read()` trigger + 5s heartbeat |
| Diagnostics tab rotary visual panel | Working — Canvas+Viewbox overlay of wheel image; ROT1 live, ROT2–4 show N/A; all show DISCONNECTED when Arduino offline |
| ArduinoConnected detection (connect/disconnect) | Working — 10s timeout on `_lastMessageReceived`; disconnect latency ≤10s |
| 74HC595 pin assignment + /OE safe boot | Implemented — Flashed but test pending |
| 74HC595 rotary position output to Pro Micro | Implemented — Flashed but test pending |
| Pro Micro (MMJoy2) reading 74HC165 from 595 | Pending — PCB not built yet |
| PCB design | In progress |
| Remaining rotary positions (buttons via 595 chain) | Pending |
