## System Overview
Developing firmware for a custom DIY F1 sim-racing steering wheel utilizing a dual-MCU architecture to separate HID input polling from dynamic logic and telemetry.

## Current Workspace Focus
*   **Target MCU:** ATmega328P-AU (16MHz).
*   **Role:** Dedicated SimHub controller. Acts as an input preprocessor, dynamic logic engine, and telemetry UI driver. 
*   **Secondary MCU (Context Only):** ATmega32U4 acts as the USB HID device, running deterministic input polling.

## Hardware Logic & Inter-MCU Communication (CRITICAL)
The ATmega328P sits between complex analog inputs and the 32U4 HID controller.

*   **Simple Rotary Switches (ROT2, ROT3, ROT4):** 3x 12-position switches read as analog inputs via resistor ladders.
    *   *Logic:* The 328P converts these to discrete states and passes them to the 32U4 via the shift register interface.
    *   *State:* Constant toggle type (1 of the 12 positions is always held active).
    *   *Note:* Switches are make-before-break.

*   **Multi-Function Rotary (ROT1) & Encoder Pair:** ROT1 (12-position) acts as a bank/mode selector for a single Rotary Encoder (CW, CCW, SW push button). This creates 12 distinct groups of 3 inputs.
    *   *SimHub Routing (Positions 8, 9, 10):* The encoder actions for these 3 positions (9 unique inputs) are handled strictly by the 328P and sent over Serial to trigger internal SimHub custom plugin functions.
    *   *32U4 Routing (Remaining 9 positions):* The encoder actions for the other 9 positions (27 unique inputs) are routed to the 32U4 via the shift register interface.
    *   *State:* Output to the 32U4 must be formatted as momentary "hold/pulse" inputs, NOT constant toggles.

*   **Clutch System:** Dual paddles using analog Hall-effect sensors. The 328P along with the SimHub Custom plugin handles analog reading, calibration, bite-point logic, and outputs as a PWM signal to the 32U4.

*   **Inter-MCU Comm (328P → 32U4):** Processed inputs are sent to the 32U4 via a **shift register interface** (328p->74HC595->74HC165->32u4). 
    *   *Constraint:* Hardware UART on the 328P is strictly reserved for SimHub. Do NOT use hardware serial for inter-MCU communication.

## LED & Telemetry System
*   **Hardware:** WS2812B LED chain (single data line D3), it's solely controlled from SimHub.

## Directives for AI Agent
*   Provide C/C++ code optimized for the ATmega328P's limited SRAM.
*   Do not compile the firmware, leave that step to the developer.
*   **Concurrency:** All routines (serial polling, analog reading, shift-register outputs) must be strictly non-blocking.
*   **Known Pitfalls to Avoid:**
    *   SimHub serial bandwidth limitations (keep telemetry parsing efficient).
    *   WS2812 timing sensitivity conflicting with SimHub serial interrupts.
*   **SimHub Plugin:** The SimHub plugin has to go hand in hand with this firmware, we have to make necessary adjustments to the plugin as we go on with the development.
    * Versioning of the plugin and firmware must be kept in sync. 
    * Versioning format: (x+1).x.x - for major change bump the first number.
    * Versioning format: x.(x+1).x - for minor change bump the second number.
    * Versioning format: x.x.(x+1) - for patch bump the third number.
    * After the changes you need to compile the plugin using the compile_plugin.ps1 script.