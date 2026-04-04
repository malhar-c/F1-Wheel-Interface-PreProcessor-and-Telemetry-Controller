# F1 Wheel Hardware Configuration Plugin - Installation & Usage Guide

## Overview

This is a **simplified, hardware-focused plugin** for configuring your F1 steering wheel's dual clutch system and other hardware settings. It integrates with SimHub's existing Arduino communication system.

## Key Features

- **Dual Clutch Bite Point Configuration** - Primary focus for your clutch system
- **Multi-Tab Interface** - Clean organization with expandable tabs
- **Hardware Status Monitoring** - Live values from Arduino
- **Future Expansion Ready** - Tabs prepared for button mapping and other wheel settings
- **No Game Data Processing** - Pure hardware configuration interface

## Installation

### Step 1: Copy Plugin File

```powershell
# Copy the compiled plugin to SimHub's plugins folder
Copy-Item ".\bin\F1WheelClutchPlugin.dll" "D:\SimHub\Plugins\F1WheelHardwareConfig.dll"
```

### Step 2: Restart SimHub

1. Close SimHub completely
2. Restart SimHub
3. The plugin will be automatically loaded

### Step 3: Enable the Plugin

1. Go to **Settings** → **Additional Plugins**
2. Find **"F1 Wheel Hardware Configuration"**
3. Enable the plugin
4. You'll see **"F1 Wheel Config"** in the left menu

## Plugin Interface

### Tab 1: Dual Clutch

**Primary focus - your clutch bite point system**

- **Connection Status** - Shows if Arduino is connected
- **Enable/Disable Clutch System** - Toggle clutch functionality
- **Bite Point Slider** - Adjust clutch bite point (10-90%)
- **Test Button** - Test with 75% bite point
- **Reset Button** - Reset to 50% default
- **Live Values** - Real-time clutch pedal and PWM output readings

### Tab 2: Wheel Settings

**Future expansion for other hardware**

- Placeholder for rotary switch settings
- Display brightness controls
- LED configurations
- Other hardware-specific settings

### Tab 3: Button Mapping

**Future expansion for custom button functions**

- Assign functions to wheel buttons
- Configure rotary encoder actions
- Set up custom SimHub properties
- Map to SimHub commands

### Tab 4: Diagnostics

**Hardware status and debugging**

- Real-time Arduino connection status
- Live data from wheel hardware
- System timestamps
- Debug information

## Arduino Communication

The plugin integrates with SimHub's existing Arduino system:

### Expected Arduino Properties

```cpp
// Your Arduino should send data in this format:
ClutchA=xxx      // Left clutch pedal value (0-1023)
ClutchB=xxx      // Right clutch pedal value (0-1023)
PWMOutput=xxx    // Current PWM output (0-255)
RotaryPosition=x // Rotary switch position
```

### Commands Sent to Arduino

```cpp
// Plugin sends these commands:
CLUTCH_BP:xx.x   // Set clutch bite point percentage
CLUTCH_RESET:1   // Reset clutch settings
```

## SimHub Dashboard Properties

The plugin exposes these properties for use in dashboards:

```
[F1WheelHardwareConfig.ClutchBitePoint]     // Current bite point (10-90)
[F1WheelHardwareConfig.ClutchSystemEnabled] // System enabled (true/false)
[F1WheelHardwareConfig.ArduinoConnected]    // Connection status
[F1WheelHardwareConfig.ClutchAValue]        // Left clutch raw value
[F1WheelHardwareConfig.ClutchBValue]        // Right clutch raw value
[F1WheelHardwareConfig.PWMOutput]           // Current PWM output
[F1WheelHardwareConfig.RotaryPosition]      // Rotary switch position
```

## Usage Workflow

### Initial Setup

1. **Install Plugin** - Follow installation steps above
2. **Configure Arduino** - Ensure your Arduino sends the expected data format
3. **Test Connection** - Check Diagnostics tab for Arduino connection
4. **Set Bite Point** - Use the Dual Clutch tab to configure your preferred bite point

### Daily Use

1. **Open F1 Wheel Config** - Click in SimHub's left menu
2. **Check Connection** - Verify Arduino is connected (green status)
3. **Adjust Bite Point** - Use slider in Dual Clutch tab
4. **Monitor Values** - Watch live clutch pedal values
5. **Test if Needed** - Use Test button to verify settings

### Future Expansion

As you add more wheel features:

1. **Wheel Settings Tab** - Will contain hardware-specific configurations
2. **Button Mapping Tab** - Will allow custom button assignments
3. **Additional Properties** - New SimHub properties for dashboard use

## Troubleshooting

### Plugin Not Visible

- Check that the .dll file is in `D:\SimHub\Plugins\`
- Restart SimHub completely
- Enable in Additional Plugins settings

### Arduino Not Connected

- Verify Arduino is properly connected via USB
- Check SimHub's Arduino settings (separate from this plugin)
- Ensure Arduino is sending data in expected format

### Bite Point Not Responding

- Check Arduino connection status
- Verify clutch system is enabled in plugin
- Check Arduino code handles `CLUTCH_BP:xx.x` commands

### Values Not Updating

- Check Diagnostics tab for live data
- Verify Arduino is sending properties with correct names
- Restart plugin by disabling/enabling in settings

## Technical Notes

### Plugin Architecture

- **No Game Data Processing** - Purely for hardware configuration
- **SimHub Integration** - Uses existing Arduino communication system
- **Expandable Design** - Easy to add new tabs and features
- **C# 5.0 Compatible** - Works with SimHub's compiler requirements

### File Structure

```
F1WheelClutchPlugin_Simple.cs  - Source code
bin/F1WheelClutchPlugin.dll    - Compiled plugin (19 KB)
```

This plugin is designed specifically for **hardware configuration**, not game data processing. It provides a clean, professional interface for managing your F1 wheel's dual clutch system with room for future expansion.
