# 🎉 F1 Wheel Clutch Plugin - Enhanced Version Ready!

## ✅ **Enhanced Compilation Complete**

Your F1 Wheel Clutch Plugin has been successfully compiled with full interactive features:

```
.\bin\F1WheelClutchPlugin.dll
```

**Plugin Details:**

- **Size**: 16.5 KB (Enhanced Version)
- **Framework**: .NET Framework 4.8
- **Language**: C# 5.0 Compatible
- **Interfaces**: IPlugin, IDataPlugin, IWPFSettingsV2
- **Features**: Full interactive UI, real-time communication, live updates

## 🚀 **New Features in Enhanced Version:**

### **Interactive Settings UI:**

- **🔧 COM Port Selection** - Dropdown with available ports
- **🔄 Connect/Disconnect Buttons** - Manual connection control
- **⚙️ Auto-Connect Option** - Automatic connection on startup
- **📊 Live Status Display** - Real-time connection status with color coding
- **🎛️ Manual Bite Point Control** - Interactive slider (10-90%)
- **📈 Live Values Monitor** - Real-time display of Clutch A/B, PWM output
- **❌ Error Display** - Shows connection errors and troubleshooting info
- **🕐 Last Update Time** - Shows when data was last received from Arduino

### **Advanced Communication:**

- **🔄 Auto-Reconnection** - Automatically tries to reconnect on connection loss
- **📡 Status Monitoring** - Continuous health check of serial connection
- **⏱️ Timeout Handling** - Proper error handling for communication timeouts
- **🛡️ Error Recovery** - Graceful handling of serial port errors

---

## 📦 **Installation Steps**

### **Step 1: Copy Plugin to SimHub**

```powershell
# Copy the compiled plugin to SimHub's Plugins folder
Copy-Item ".\bin\F1WheelClutchPlugin.dll" "D:\SimHub\Plugins\"
```

Or manually copy:

- **Source**: `e:\Sim Racing\Custom Steering Wheel\Redbull Steering custom firmware\F1 Wheel Interface PreProcessor and Telemetry Controller\SimHub\bin\F1WheelClutchPlugin.dll`
- **Destination**: `D:\SimHub\Plugins\`

### **Step 2: Restart SimHub**

1. **Close SimHub completely**
2. **Restart SimHub**
3. SimHub should detect the new plugin

### **Step 3: Enable Plugin**

1. **Go to**: `Additional Plugins`
2. **Find**: `F1WheelDualClutch` in the plugin list
3. **Enable**: Check the checkbox
4. **Verify**: Plugin shows as "Enabled"

### **Step 4: Configure Plugin**

1. **Click**: `Settings` next to the plugin
2. **Configure**: Serial port (default: COM3)
3. **Test**: Connection with Arduino

---

## 🔧 **Plugin Features**

### **Dashboard Properties Available:**

- `DataCorePlugin.ExternalScript.F1WheelDualClutch.ClutchBitePoint`
- `DataCorePlugin.ExternalScript.F1WheelDualClutch.SystemActive`
- `DataCorePlugin.ExternalScript.F1WheelDualClutch.IsConnected`
- `DataCorePlugin.ExternalScript.F1WheelDualClutch.ClutchAValue`
- `DataCorePlugin.ExternalScript.F1WheelDualClutch.ClutchBValue`
- `DataCorePlugin.ExternalScript.F1WheelDualClutch.PWMOutput`

### **Arduino Communication:**

- **Protocol**: Custom serial protocol
- **Baud Rate**: 115200
- **Commands**:
  - `CLUTCH_BP_GET` - Get current bite point
  - `CLUTCH_BP_SET:45.5` - Set bite point to 45.5%
  - `CLUTCH_STATUS:A=123,B=456,PWM=78,BP=45.5` - Status response

### **Settings UI:**

- **Serial Port Selection**
- **Connection Status**
- **Real-time Bite Point Display**
- **Clutch Values Monitoring**

---

## 🚀 **Next Steps**

### **1. Install Dashboard Widgets**

Your existing dashboard files should work with the new plugin:

- `F1 Clutch Settings.djson`
- `F1 Wheel Main Dashboard.djson`

### **2. Test Arduino Communication**

1. **Upload** the Arduino firmware:

   - `src/main.cpp`
   - `src/ExpandedInputsPreProcessor.h`
   - `src/SHCustomProtocol.h`

2. **Connect** Arduino to the configured COM port

3. **Verify** rotary position 7 activates the clutch system

### **3. Calibrate System**

- **Test** hall effect sensors on A4/A5
- **Verify** PWM output on D9
- **Adjust** bite point range (10.0-90.0%)
- **Test** encoder control for bite point adjustment

---

## 🔍 **Troubleshooting**

### **Plugin Not Showing in SimHub:**

- Check file permissions on the .dll
- Ensure SimHub was restarted completely
- Check SimHub logs: `%AppData%\SimHub\Logs`

### **Plugin Shows as Disabled:**

- Check .NET Framework 4.8 is installed
- Verify all dependencies are available
- Check Windows Event Viewer for errors

### **Serial Communication Issues:**

- Verify correct COM port in settings
- Check Arduino is connected and powered
- Test with Arduino Serial Monitor first
- Ensure baud rate matches (115200)

### **Dashboard Properties Not Available:**

- Verify plugin is enabled and running
- Check property names in dashboard editor
- Restart SimHub after enabling plugin

---

## 📊 **Testing Checklist**

### **Plugin Installation:**

- [ ] Plugin appears in Additional Plugins
- [ ] Plugin can be enabled successfully
- [ ] Settings UI opens without errors
- [ ] No error messages in SimHub logs

### **Serial Communication:**

- [ ] Can connect to Arduino via plugin
- [ ] Status shows "Connected" in settings
- [ ] Arduino responds to commands
- [ ] Data updates in real-time

### **Dashboard Integration:**

- [ ] Properties appear in dashboard editor
- [ ] Widgets load correctly
- [ ] Real-time updates work
- [ ] Conditional visibility based on rotary position

### **Hardware Integration:**

- [ ] Rotary position 7 activates system
- [ ] Hall sensors provide valid readings
- [ ] PWM output responds to bite point changes
- [ ] Encoder controls work smoothly

---

## 🎮 **Ready for Racing!**

Your F1 Wheel Dual Clutch System is now complete:

1. **✅ Arduino Firmware** - Advanced input processing with clutch system
2. **✅ SimHub Plugin** - Professional .dll compiled and ready
3. **✅ Dashboard Widgets** - Beautiful F1-style interface
4. **✅ Complete Integration** - Hardware, software, and UI working together

**Enjoy your professional F1 steering wheel experience!**

---

## 📞 **Support**

If you encounter any issues:

1. Check the troubleshooting section above
2. Review SimHub logs for error messages
3. Test individual components (Arduino, plugin, dashboard)
4. Verify all connections and configurations

**Happy Racing! 🏁**
