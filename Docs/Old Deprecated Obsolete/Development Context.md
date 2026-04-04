Here’s a **well-structured development context prompt** that clearly outlines your **intention**, **motivation**, and **technical references** for your SimHub-based F1 steering wheel project. You can reuse this for documentation, sharing with collaborators, or even prompting other AI tools or firmware generation environments:

---

### 🛠️ **Development Context for Custom F1 Steering Wheel Integration with SimHub and Arduino**

#### 🎯 **Project Goal**

To build a **functional 1:1 replica** of the **Red Bull RB19 Formula 1 steering wheel** for sim racing, fully integrated with **SimHub** for telemetry-driven LED feedback and **game HID input** using custom logic and modular microcontroller architecture.

---

#### 💡 **Motivation**

The default SimHub Arduino support is limiting when handling complex logic (like dual-clutch behavior, rotary position mapping, telemetry control, and mode-based configuration like STRAT switches). This project aims to:

* Separate telemetry and LED display logic from HID input
* Add intelligence (macro-like behavior) to hardware like 12-position rotary switches
* Use shift registers and analog resistor ladders to simplify wiring and pin management
* Enable features found in actual F1 wheels like:

  * Dual-stage clutch bite point control
  * STRAT rotary (multi-parameter game config)
  * Adaptive input routing based on mode selectors

---

#### ⚙️ **Technical Strategy**

| Component               | Purpose                                                       | Controlled By       |
| ----------------------- | ------------------------------------------------------------- | ------------------- |
| **Arduino Pro Micro**   | HID/Game controller (buttons, encoders, axes)                 | MMJoy2 Firmware     |
| **Arduino Nano**        | SimHub controller (telemetry, WS2812, logic routing)          | Custom Sketch       |
| **Shift Registers**     | I/O expansion (165 for inputs, optional 595 for Nano outputs) | Both                |
| **Rotary Switches**     | Input via analog resistor ladder (processed by Nano)          | Routed to Pro Micro |
| **Hall Sensors / Pots** | Read dual clutch levers, output single PWM/analog axis        | Nano → Pro Micro    |
| **WS2812B LEDs**        | Addressable shift lights, flags, indicators                   | SimHub + Nano       |

---

#### 📚 **Reference Resources**

1. 🔧 **SimHub Custom Arduino Protocol**

   * [SimHub GitHub Wiki - Custom Hardware](https://github.com/SHWotever/SimHub/wiki/Custom-Arduino-hardware-support)
   * Details message structure, multiple USB board support, read/loop/idle methods, and serial telemetry handling.

2. 🧠 **MMJoy2 HID Firmware**

   * Used on Pro Micro for low-latency, high-flexibility HID device emulation
   * Supports button matrices, encoders, axes, shift register inputs, etc.

3. 📦 **Shift Registers**

   * **SN74HC165N** – 8-bit parallel-in/serial-out for reading digital states (buttons, converted rotary outputs)
   * Chainable with clock and latch sharing, unique data out pins

4. 🛠️ **Arduino IDE + PlatformIO**

   * Required for manual firmware development on Nano (SimHub does not auto-flash custom logic)
   * Uses SimHub libraries with `SHCustomProtocol` messaging

5. 🧪 **SimHub Hardware Tools**

   * Device definition interface (with multiple USB support)
   * Custom telemetry triggers with NCalc syntax
   * Dashboards, shift light configuration, flag mapping

---

#### 🔩 **System Highlights**

* Dual-board architecture for task separation and modular testing
* Analog rotary switch decoding with digital output transformation
* Macro-like game configuration logic (STRAT rotary switch)
* Expandable button input handling via shift registers
* Real-time control of telemetry-driven elements via WS2812
* Manual sketch flashing and version-controlled codebase

---

