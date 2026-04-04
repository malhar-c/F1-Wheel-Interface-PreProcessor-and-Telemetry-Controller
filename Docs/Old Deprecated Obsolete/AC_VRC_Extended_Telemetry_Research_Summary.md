# Assetto Corsa VRC Extended Telemetry Research Summary

## Research Question

Can we add new, game-specific properties (such as extra VRC Formula Alpha 2024 controls) to SimHub via a custom plugin, specifically by accessing additional game data from Assetto Corsa's UDP, shared memory, or through Content Manager/Custom Shaders Patch (CSP) extended controls?

## Executive Summary

**Short Answer**: **Limited Possibilities** - While technically possible in some scenarios, there are significant limitations to accessing mod-specific data like VRC FA2024 extended controls through SimHub plugins.

## Key Findings

### 1. SimHub Plugin Limitations

**What SimHub Plugins CAN Do:**

- Access standard Assetto Corsa telemetry data already exposed by SimHub's game reader
- Process and transform existing game data using calculated properties
- Interface with hardware (Arduino, serial devices, etc.)
- Create custom UI controls and dashboards
- Use NCALC formulas and JavaScript for complex calculations

**What SimHub Plugins CANNOT Do:**

- Directly access raw UDP streams from games
- Read shared memory that isn't already processed by SimHub's game reader
- Access mod-specific data that isn't exposed through AC's standard telemetry API
- Parse game files or mod data independently

### 2. Assetto Corsa Telemetry Architecture

**Standard AC Telemetry (Available in SimHub):**

- Basic car physics data (speed, RPM, gear, etc.)
- Standard control inputs (steering, throttle, brake)
- Track information and session data
- Basic setup parameters

**Extended/Mod Data (Generally NOT Available):**

- VRC-specific car parameters and controls
- Custom Shaders Patch extended physics
- Mod-specific setup options
- Community mod telemetry extensions

### 3. Possible Workarounds and Solutions

#### Option A: Custom Assetto Corsa Python App (Most Promising)

**How it Works:**

1. Create a custom Python app that runs inside Assetto Corsa
2. The app reads mod-specific data using AC's Python API
3. App sends additional data to SimHub via UDP
4. SimHub receives the data through its UDP connector feature

**Example Workflow:**

```python
# AC Python App (simplified example)
import ac
import socket

# Read VRC-specific data (if exposed by mod)
vrc_data = get_vrc_extended_controls()  # Hypothetical function

# Send to SimHub via UDP
udp_socket.sendto(f"VRC_BRAKE_BALANCE:{vrc_data.brake_balance}", ("127.0.0.1", 9999))
```

**Limitations:**

- Only works if the VRC mod exposes its data through AC's Python API
- Requires the mod developers to provide access to extended controls
- Most mods don't expose their internal data to Python

#### Option B: Custom Shaders Patch Extensions (Limited)

**Possibility:**

- CSP can extend AC's physics and rendering
- Some CSP features might expose additional telemetry
- CSP has configuration files that might allow data export

**Reality:**

- CSP primarily focuses on visual and physics improvements
- No evidence found of CSP exposing VRC-specific controls to external tools
- CSP's extension API is not well-documented for telemetry purposes

#### Option C: Memory Reading/Process Injection (Not Recommended)

**Technical Possibility:**

- Direct memory reading from AC process
- Reverse engineering mod data structures
- Creating external tools to extract data

**Why This is Problematic:**

- Violates most game EULAs
- Extremely fragile and game-version dependent
- Requires deep reverse engineering skills
- May trigger anti-cheat systems

### 4. Existing Examples and Precedents

**SimHub UDPConnector + Custom AC Apps:**

- There are examples of AC Python apps sending additional data to SimHub
- These typically send calculated or processed data, not raw mod data
- Most successful examples deal with standard AC data, not mod-specific information

**Content Manager Integration:**

- Content Manager has extensive AC integration
- Primarily focused on content management and setup
- No evidence of exposing mod-specific telemetry to external tools

## Specific Assessment for VRC Formula Alpha 2024

### What We Know About VRC Mods:

- High-quality physics and car models
- Extensive setup options and parameters
- Professional-grade simulation features

### What We Don't Know:

- Whether VRC exposes extended controls through AC's Python API
- If any VRC-specific data is available to external applications
- Whether the VRC team has provided any telemetry extensions

### Likelihood of Success:

**Low to Moderate** - Unless VRC specifically designed their mod to expose extended telemetry data through AC's Python API, it's unlikely that custom controls and parameters can be accessed externally.

## Recommendations

### 1. For Immediate Implementation

**Stick with SimHub's Calculated Properties:**

- Use NCALC or JavaScript to create custom formulas
- Combine existing telemetry data to approximate desired values
- Create virtual controls based on available data

### 2. For Advanced Solutions

**Contact VRC Directly:**

- Reach out to VRC developers to ask about telemetry API
- Request documentation on any extended controls they expose
- Ask if they have plans for SimHub integration

### 3. For Future Development

**Monitor SimHub Community:**

- Watch for community-developed solutions
- Check if other users have found ways to access VRC data
- Look for updates to SimHub that might add more mod support

### 4. Alternative Approaches

**Focus on Available Data:**

- Maximize use of standard AC telemetry
- Create sophisticated calculated properties
- Develop hardware interfaces (like your current F1 wheel project)

## Conclusion

While the technical infrastructure exists for extending Assetto Corsa telemetry (through Python apps and UDP communication), the practical limitation is that most mods, including VRC, don't expose their extended controls and parameters to external applications.

**The most realistic approach** is to:

1. Use SimHub's existing calculated properties for complex formulas
2. Focus on hardware integration (which your current project excels at)
3. Work within the constraints of standard AC telemetry
4. Consider reaching out to VRC directly for any extended telemetry possibilities

Your current hardware-focused plugin approach is actually the most practical and reliable method for extending SimHub functionality in the context of Assetto Corsa.

---

**Research Date:** January 2025  
**Research Scope:** SimHub plugin development, AC telemetry architecture, VRC mod integration possibilities  
**Confidence Level:** High for architectural limitations, Moderate for specific VRC capabilities (would require direct testing)
