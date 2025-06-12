# 🚀 F1 Wheel Clutch Plugin Compilation Guide

## 📋 **Quick Start**

Choose your preferred compilation method:

1. **🎯 PowerShell Script (Recommended)** - Automatic, handles everything
2. **⚡ Batch Script** - Simple, manual path configuration
3. **🏗️ SimHub Built-in** - Use SimHub's plugin creator
4. **🔧 Visual Studio** - Full IDE experience

---

## 🎯 **Method 1: PowerShell Script (Recommended)**

### **Step 1: Run PowerShell Script**

```powershell
# Navigate to SimHub folder
cd "e:\Sim Racing\Custom Steering Wheel\Redbull Steering custom firmware\F1 Wheel Interface PreProcessor and Telemetry Controller\SimHub"

# Run compilation script
.\compile_plugin.ps1
```

### **Features:**

- ✅ **Auto-detects SimHub installation**
- ✅ **Finds .NET Framework compiler**
- ✅ **Validates all dependencies**
- ✅ **Detailed error reporting**
- ✅ **Cross-platform compatibility**

### **Advanced Usage:**

```powershell
# Specify custom SimHub path
.\compile_plugin.ps1 -SimHubPath "D:\SimHub"

# Open output folder after compilation
.\compile_plugin.ps1 -OpenOutput

# Custom output directory
.\compile_plugin.ps1 -OutputPath ".\build"
```

---

## ⚡ **Method 2: Batch Script**

### **Step 1: Configure Paths**

Edit `compile_plugin.bat` and update SimHub path if needed:

```batch
set SIMHUB_PATH=C:\Program Files (x86)\SimHub
```

### **Step 2: Run Compilation**

```cmd
compile_plugin.bat
```

---

## 🏗️ **Method 3: SimHub Built-in Plugin Creator**

### **Step 1: Open SimHub Plugin Creator**

1. **Launch SimHub**
2. **Go to**: `Additional Plugins` → `Develop new plugin`
3. **Click**: `Create new plugin project`

### **Step 2: Create Project**

- **Plugin Name**: `F1WheelClutch`
- **Author**: Your name
- **Description**: `F1 Wheel Clutch Bite Point System`

### **Step 3: Replace Generated Code**

SimHub creates a template. Replace the entire content of the generated `.cs` file with our `F1WheelClutchPlugin.cs` content.

### **Step 4: Build in SimHub**

Click `Build plugin` in SimHub's interface.

---

## 🔧 **Method 4: Visual Studio**

### **Step 1: Create New Project**

```
File → New → Project → Class Library (.NET Framework)
Name: F1WheelClutchPlugin
Framework: .NET Framework 4.7.2
```

### **Step 2: Add SimHub References**

Right-click `References` → `Add Reference` → `Browse`:

```
C:\Program Files (x86)\SimHub\SimHub.Plugins.dll
C:\Program Files (x86)\SimHub\GameReaderCommon.dll
C:\Program Files (x86)\SimHub\SimHub.Plugins.DataPlugins.dll
```

### **Step 3: Add System References**

Add these .NET Framework references:

- `System.Windows.Forms`
- `PresentationCore`
- `PresentationFramework`
- `WindowsBase`

### **Step 4: Build Project**

Press `Ctrl+Shift+B` or `Build → Build Solution`

---

## 📦 **Installation After Compilation**

### **Step 1: Locate Compiled DLL**

After successful compilation, find:

```
.\bin\F1WheelClutchPlugin.dll
```

### **Step 2: Copy to SimHub**

Copy the `.dll` file to SimHub's plugins directory:

```
C:\Program Files (x86)\SimHub\Plugins\
```

### **Step 3: Restart SimHub**

1. **Close SimHub completely**
2. **Restart SimHub**
3. **Go to**: `Additional Plugins`
4. **Find**: `F1WheelClutch` in the list
5. **Enable**: Check the checkbox

### **Step 4: Configure Plugin**

1. **Click**: `Settings` next to the plugin
2. **Set**: Serial port (e.g., `COM3`)
3. **Enable**: Auto-connect if desired
4. **Test**: Connection with your Arduino

---

## 🔍 **Troubleshooting**

### **Common Issues:**

#### **1. "SimHub not found"**

- Verify SimHub installation path
- Update script with correct path
- Check if SimHub.exe exists

#### **2. ".NET Framework compiler not found"**

- Install .NET Framework 4.7.2 or later
- Install Visual Studio Build Tools
- Check Windows Features for .NET Framework

#### **3. "Missing references"**

- Verify SimHub DLL files exist
- Check SimHub version compatibility
- Reinstall SimHub if needed

#### **4. "Compilation errors"**

- Check C# syntax in plugin file
- Verify all using statements
- Review error messages carefully

#### **5. "Plugin not showing in SimHub"**

- Check .dll file permissions
- Verify correct Plugins folder
- Restart SimHub completely
- Check SimHub logs for errors

### **Debug Mode:**

Enable debug output in PowerShell:

```powershell
$DebugPreference = "Continue"
.\compile_plugin.ps1
```

---

## 📊 **Verification**

### **After Installation, Verify:**

1. **✅ Plugin Listed**: In `Additional Plugins`
2. **✅ Settings Available**: Click `Settings` button works
3. **✅ Serial Connection**: Can connect to Arduino
4. **✅ Dashboard Properties**: Properties appear in dashboard editor
5. **✅ Real-time Updates**: Bite point changes reflect in dashboard

### **Dashboard Properties to Verify:**

- `DataCorePlugin.ExternalScript.F1WheelClutch.ClutchBitePoint`
- `DataCorePlugin.ExternalScript.F1WheelClutch.SystemActive`
- `DataCorePlugin.ExternalScript.F1WheelClutch.IsConnected`
- `DataCorePlugin.ExternalScript.F1WheelClutch.ClutchAValue`
- `DataCorePlugin.ExternalScript.F1WheelClutch.ClutchBValue`

---

## 🚀 **Next Steps After Compilation**

1. **📱 Install Dashboard**: Load `F1 Clutch Settings.djson`
2. **🔧 Test Hardware**: Verify Arduino communication
3. **🎮 Test in SimHub**: Check rotary position 7 activation
4. **⚙️ Calibrate System**: Adjust bite point range if needed
5. **🏁 Race Ready**: Deploy on your F1 steering wheel!

---

## 💡 **Pro Tips**

- **Use PowerShell script** for easiest compilation
- **Test immediately** after installation
- **Check SimHub logs** if issues occur
- **Backup your settings** before major changes
- **Update documentation** when modifying code

## 📞 **Support**

If compilation fails, check:

1. **Error messages** in compilation output
2. **SimHub logs** in `%AppData%\SimHub\Logs`
3. **Windows Event Viewer** for system errors
4. **Plugin compatibility** with your SimHub version
