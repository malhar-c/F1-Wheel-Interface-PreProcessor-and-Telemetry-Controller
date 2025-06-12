# F1 Wheel Clutch Plugin Development Guide

## 🎯 **SimHub Plugin Development Setup**

### **Prerequisites:**

- **Visual Studio 2019/2022** or **Visual Studio Code** with C# extension
- **.NET Framework 4.7.2 or later** (SimHub requirement)
- **SimHub SDK** (included in SimHub installation)

---

## 🚀 **Method 1: SimHub Built-in Plugin Creator (Recommended)**

### **Step 1: Use SimHub's Plugin Generator**

1. **Open SimHub**
2. **Go to**: "Additional Plugins" → "Develop new plugin"
3. **Create new project**:
   - **Name**: `F1WheelClutch`
   - **Author**: Your name
   - **Description**: F1 Wheel Clutch Bite Point System

### **Step 2: SimHub Auto-Generates Project**

SimHub will create:

```
📁 Plugins/F1WheelClutch/
├── F1WheelClutch.cs           # Main plugin file
├── F1WheelClutch.csproj       # Project file
├── DataPluginDemoSettings.xaml # Settings UI (optional)
└── packages.config            # Dependencies
```

### **Step 3: Replace Generated Code**

Replace the auto-generated `F1WheelClutch.cs` with our enhanced version.

---

## 🛠️ **Method 2: Manual Visual Studio Setup**

### **Step 1: Create New Project**

```
File → New → Project → Class Library (.NET Framework)
Name: F1WheelClutch
Framework: .NET Framework 4.7.2
```

### **Step 2: Add SimHub References**

Add references to SimHub DLLs (found in SimHub installation):

```
SimHub.Plugins.dll
SimHub.Plugins.DataPlugins.dll
GameReaderCommon.dll
```

### **Step 3: Install NuGet Packages**

```xml
<PackageReference Include="System.IO.Ports" Version="6.0.0" />
<PackageReference Include="Newtonsoft.Json" Version="13.0.1" />
```

---

## 📝 **Enhanced Plugin Code**

Here's the complete, production-ready plugin code:

```csharp
using System;
using System.ComponentModel;
using System.IO.Ports;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows;
using SimHub.Plugins;
using GameReaderCommon;
using System.Threading.Tasks;
using System.Linq;

[PluginDescription("F1 Wheel Clutch Bite Point System - Professional clutch control for F1 steering wheels")]
[PluginAuthor("F1 Wheel Builder")]
[PluginName("F1WheelClutch")]
public class F1WheelClutchPlugin : IPlugin, IDataPlugin, IWPFSettingsV2
{
    #region Private Fields
    private double _clutchBitePoint = 50.0;
    private bool _systemActive = false;
    private bool _isConnected = false;
    private SerialPort _serialPort;
    private string _selectedPort = "COM3";
    private bool _autoConnect = true;
    private int _lastClutchA = 0;
    private int _lastClutchB = 0;
    private int _lastPWMOutput = 0;
    private DateTime _lastUpdate = DateTime.Now;
    #endregion

    #region Plugin Settings
    [PluginSetting("Serial Port", typeof(string), "COM3", "Serial port for F1 wheel communication")]
    public string SerialPort
    {
        get => _selectedPort;
        set
        {
            if (_selectedPort != value)
            {
                _selectedPort = value;
                if (_autoConnect) ConnectToArduino();
                OnPropertyChanged();
            }
        }
    }

    [PluginSetting("Auto Connect", typeof(bool), true, "Automatically connect to Arduino on startup")]
    public bool AutoConnect
    {
        get => _autoConnect;
        set
        {
            _autoConnect = value;
            OnPropertyChanged();
        }
    }

    [PluginSetting("Clutch Bite Point", typeof(double), 50.0, "Current clutch bite point percentage (10-90%)")]
    public double ClutchBitePoint
    {
        get => _clutchBitePoint;
        set
        {
            var newValue = Math.Max(10.0, Math.Min(90.0, value));
            if (Math.Abs(_clutchBitePoint - newValue) > 0.01)
            {
                _clutchBitePoint = newValue;
                SendBitePointToArduino(newValue);
                OnPropertyChanged();
            }
        }
    }
    #endregion

    #region IPlugin Implementation
    public void Init(PluginManager pluginManager)
    {
        SimHub.Logging.Current.Info("F1 Wheel Clutch Plugin - Initializing...");

        // Auto-connect if enabled
        if (_autoConnect && !string.IsNullOrEmpty(_selectedPort))
        {
            Task.Delay(1000).ContinueWith(_ => ConnectToArduino());
        }

        SimHub.Logging.Current.Info("F1 Wheel Clutch Plugin - Initialized successfully");
    }

    public void End(PluginManager pluginManager)
    {
        DisconnectFromArduino();
        SimHub.Logging.Current.Info("F1 Wheel Clutch Plugin - Shutdown complete");
    }
    #endregion

    #region IDataPlugin Implementation
    public void DataUpdate(PluginManager pluginManager, ref GameData data)
    {
        // Expose properties for dashboard widgets
        pluginManager.SetPropertyValue("F1Wheel.ClutchBitePoint", this.GetType(), _clutchBitePoint);
        pluginManager.SetPropertyValue("F1Wheel.SystemActive", this.GetType(), _systemActive);
        pluginManager.SetPropertyValue("F1Wheel.IsConnected", this.GetType(), _isConnected);
        pluginManager.SetPropertyValue("F1Wheel.LastClutchA", this.GetType(), _lastClutchA);
        pluginManager.SetPropertyValue("F1Wheel.LastClutchB", this.GetType(), _lastClutchB);
        pluginManager.SetPropertyValue("F1Wheel.LastPWMOutput", this.GetType(), _lastPWMOutput);
        pluginManager.SetPropertyValue("F1Wheel.LastUpdate", this.GetType(), _lastUpdate.ToString("HH:mm:ss"));

        // Check for button 300 (rotary position 7) across all controllers
        _systemActive = CheckButton300Status(pluginManager);
    }

    private bool CheckButton300Status(PluginManager pluginManager)
    {
        try
        {
            // Check multiple controller input sources
            for (int i = 0; i < 8; i++)
            {
                var buttonPressed = pluginManager.GetPropertyValue($"InputStatus.Button300") as bool?;
                if (buttonPressed == true) return true;

                buttonPressed = pluginManager.GetPropertyValue($"InputStatus.Controller{i}.Button300") as bool?;
                if (buttonPressed == true) return true;
            }
        }
        catch (Exception ex)
        {
            SimHub.Logging.Current.Debug($"F1 Wheel: Error checking button status: {ex.Message}");
        }
        return false;
    }
    #endregion

    #region Settings UI (WPF)
    public Control GetWPFSettingsControl(PluginManager pluginManager)
    {
        return new F1WheelSettingsControl(this);
    }
    #endregion

    #region Serial Communication
    private void ConnectToArduino()
    {
        Task.Run(() =>
        {
            try
            {
                DisconnectFromArduino();

                if (string.IsNullOrEmpty(_selectedPort)) return;

                _serialPort = new SerialPort(_selectedPort, 19200, Parity.None, 8, StopBits.One)
                {
                    ReadTimeout = 1000,
                    WriteTimeout = 1000
                };

                _serialPort.DataReceived += SerialPort_DataReceived;
                _serialPort.Open();
                _isConnected = true;

                SimHub.Logging.Current.Info($"F1 Wheel: Connected to {_selectedPort}");

                // Request current bite point
                Task.Delay(500).ContinueWith(_ => RequestBitePointFromArduino());
            }
            catch (Exception ex)
            {
                _isConnected = false;
                SimHub.Logging.Current.Error($"F1 Wheel: Failed to connect - {ex.Message}");
            }
        });
    }

    private void DisconnectFromArduino()
    {
        try
        {
            if (_serialPort?.IsOpen == true)
            {
                _serialPort.DataReceived -= SerialPort_DataReceived;
                _serialPort.Close();
                _serialPort.Dispose();
                _serialPort = null;
            }
            _isConnected = false;
        }
        catch (Exception ex)
        {
            SimHub.Logging.Current.Error($"F1 Wheel: Disconnect error - {ex.Message}");
        }
    }

    private void SerialPort_DataReceived(object sender, SerialDataReceivedEventArgs e)
    {
        try
        {
            var data = _serialPort.ReadLine()?.Trim();
            if (!string.IsNullOrEmpty(data))
            {
                ProcessArduinoResponse(data);
            }
        }
        catch (Exception ex)
        {
            SimHub.Logging.Current.Debug($"F1 Wheel: Serial read error - {ex.Message}");
        }
    }

    private void ProcessArduinoResponse(string response)
    {
        _lastUpdate = DateTime.Now;

        if (response.StartsWith("CLUTCH_BP_VALUE:"))
        {
            var valueStr = response.Substring(16);
            if (double.TryParse(valueStr, out double value))
            {
                _clutchBitePoint = value;
                OnPropertyChanged(nameof(ClutchBitePoint));
                SimHub.Logging.Current.Info($"F1 Wheel: Bite point received - {value:F1}%");
            }
        }
        else if (response.StartsWith("CLUTCH_BP_SET_OK:"))
        {
            SimHub.Logging.Current.Info($"F1 Wheel: Bite point set - {response.Substring(17)}%");
        }
        else if (response.StartsWith("CLUTCH_STATUS:"))
        {
            ParseClutchStatus(response.Substring(14));
        }
        else if (response.StartsWith("Clutch:"))
        {
            ParseClutchData(response);
        }
    }

    private void ParseClutchStatus(string status)
    {
        // Parse: BP=50.0,Active=True,Range=10.0-90.0,Step=0.5
        var parts = status.Split(',');
        foreach (var part in parts)
        {
            var keyValue = part.Split('=');
            if (keyValue.Length == 2)
            {
                switch (keyValue[0].Trim())
                {
                    case "BP":
                        if (double.TryParse(keyValue[1], out double bp))
                            _clutchBitePoint = bp;
                        break;
                    case "Active":
                        bool.TryParse(keyValue[1], out _systemActive);
                        break;
                }
            }
        }
    }

    private void ParseClutchData(string data)
    {
        // Parse: Clutch: A=512 B=256 BP=50.0 PWM=128 CA=400 CB=300
        try
        {
            var parts = data.Split(' ');
            foreach (var part in parts)
            {
                var keyValue = part.Split('=');
                if (keyValue.Length == 2)
                {
                    switch (keyValue[0])
                    {
                        case "A":
                            int.TryParse(keyValue[1], out _lastClutchA);
                            break;
                        case "B":
                            int.TryParse(keyValue[1], out _lastClutchB);
                            break;
                        case "PWM":
                            int.TryParse(keyValue[1], out _lastPWMOutput);
                            break;
                    }
                }
            }
        }
        catch (Exception ex)
        {
            SimHub.Logging.Current.Debug($"F1 Wheel: Error parsing clutch data - {ex.Message}");
        }
    }

    public void SendBitePointToArduino(double bitePoint)
    {
        Task.Run(() =>
        {
            try
            {
                if (_serialPort?.IsOpen == true)
                {
                    var command = $"CLUTCH_BP_SET:{bitePoint:F1}\n";
                    _serialPort.Write(command);
                    SimHub.Logging.Current.Info($"F1 Wheel: Sent bite point - {bitePoint:F1}%");
                }
            }
            catch (Exception ex)
            {
                SimHub.Logging.Current.Error($"F1 Wheel: Failed to send bite point - {ex.Message}");
            }
        });
    }

    public void RequestBitePointFromArduino()
    {
        Task.Run(() =>
        {
            try
            {
                if (_serialPort?.IsOpen == true)
                {
                    _serialPort.Write("CLUTCH_BP_GET\n");
                }
            }
            catch (Exception ex)
            {
                SimHub.Logging.Current.Debug($"F1 Wheel: Failed to request bite point - {ex.Message}");
            }
        });
    }

    public void RequestSystemStatus()
    {
        Task.Run(() =>
        {
            try
            {
                if (_serialPort?.IsOpen == true)
                {
                    _serialPort.Write("CLUTCH_STATUS\n");
                }
            }
            catch (Exception ex)
            {
                SimHub.Logging.Current.Debug($"F1 Wheel: Failed to request status - {ex.Message}");
            }
        });
    }
    #endregion

    #region Property Changed
    public event PropertyChangedEventHandler PropertyChanged;
    protected virtual void OnPropertyChanged([System.Runtime.CompilerServices.CallerMemberName] string propertyName = null)
    {
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }
    #endregion
}

#region Settings UI Control
public class F1WheelSettingsControl : UserControl
{
    private readonly F1WheelClutchPlugin _plugin;

    public F1WheelSettingsControl(F1WheelClutchPlugin plugin)
    {
        _plugin = plugin;
        InitializeComponent();
    }

    private void InitializeComponent()
    {
        var mainPanel = new StackPanel { Margin = new Thickness(10) };

        // Title
        var title = new TextBlock
        {
            Text = "F1 Wheel Clutch Bite Point System",
            FontSize = 16,
            FontWeight = FontWeights.Bold,
            Margin = new Thickness(0, 0, 0, 20),
            Foreground = Brushes.DarkBlue
        };
        mainPanel.Children.Add(title);

        // Connection Status
        var statusPanel = new StackPanel { Orientation = Orientation.Horizontal, Margin = new Thickness(0, 0, 0, 10) };
        statusPanel.Children.Add(new TextBlock { Text = "Status: ", VerticalAlignment = VerticalAlignment.Center });

        var statusText = new TextBlock
        {
            Text = _plugin._isConnected ? "Connected" : "Disconnected",
            Foreground = _plugin._isConnected ? Brushes.Green : Brushes.Red,
            FontWeight = FontWeights.Bold,
            VerticalAlignment = VerticalAlignment.Center
        };
        statusPanel.Children.Add(statusText);
        mainPanel.Children.Add(statusPanel);

        // Serial Port
        var portPanel = new StackPanel { Margin = new Thickness(0, 0, 0, 15) };
        portPanel.Children.Add(new TextBlock { Text = "Serial Port:", FontWeight = FontWeights.SemiBold });

        var portCombo = new ComboBox { Width = 100, Margin = new Thickness(0, 5, 0, 0) };
        portCombo.Items.Add("COM1");
        portCombo.Items.Add("COM2");
        portCombo.Items.Add("COM3");
        portCombo.Items.Add("COM4");
        portCombo.Items.Add("COM5");
        portCombo.Items.Add("COM6");
        portCombo.Items.Add("COM7");
        portCombo.Items.Add("COM8");
        portCombo.Text = _plugin.SerialPort;
        portCombo.SelectionChanged += (s, e) =>
        {
            if (portCombo.SelectedItem != null)
                _plugin.SerialPort = portCombo.SelectedItem.ToString();
        };
        portPanel.Children.Add(portCombo);
        mainPanel.Children.Add(portPanel);

        // Bite Point Control
        var bpPanel = new StackPanel { Margin = new Thickness(0, 0, 0, 15) };
        bpPanel.Children.Add(new TextBlock { Text = "Bite Point Control:", FontWeight = FontWeights.SemiBold });

        var bpValueText = new TextBlock
        {
            Text = $"Current: {_plugin.ClutchBitePoint:F1}%",
            Margin = new Thickness(0, 5, 0, 5),
            FontSize = 14,
            Foreground = Brushes.DarkGreen
        };
        bpPanel.Children.Add(bpValueText);

        var bpSlider = new Slider
        {
            Minimum = 10,
            Maximum = 90,
            Value = _plugin.ClutchBitePoint,
            TickFrequency = 5,
            IsSnapToTickEnabled = false,
            Width = 300,
            Margin = new Thickness(0, 5, 0, 10)
        };
        bpSlider.ValueChanged += (s, e) =>
        {
            _plugin.ClutchBitePoint = bpSlider.Value;
            bpValueText.Text = $"Current: {bpSlider.Value:F1}%";
        };
        bpPanel.Children.Add(bpSlider);

        // Range labels
        var rangePanel = new StackPanel { Orientation = Orientation.Horizontal };
        rangePanel.Children.Add(new TextBlock { Text = "10%", FontSize = 10, Foreground = Brushes.Gray });
        rangePanel.Children.Add(new TextBlock { Text = new string(' ', 50) });
        rangePanel.Children.Add(new TextBlock { Text = "90%", FontSize = 10, Foreground = Brushes.Gray });
        bpPanel.Children.Add(rangePanel);

        mainPanel.Children.Add(bpPanel);

        // Control Buttons
        var buttonPanel = new StackPanel { Orientation = Orientation.Horizontal, Margin = new Thickness(0, 10, 0, 0) };

        var connectBtn = new Button
        {
            Content = "Connect",
            Width = 80,
            Height = 30,
            Margin = new Thickness(0, 0, 10, 0),
            Background = Brushes.LightBlue
        };
        connectBtn.Click += (s, e) => _plugin.ConnectToArduino();

        var getBPBtn = new Button
        {
            Content = "Get Bite Point",
            Width = 100,
            Height = 30,
            Margin = new Thickness(0, 0, 10, 0),
            Background = Brushes.LightGreen
        };
        getBPBtn.Click += (s, e) => _plugin.RequestBitePointFromArduino();

        var statusBtn = new Button
        {
            Content = "Get Status",
            Width = 80,
            Height = 30,
            Background = Brushes.LightYellow
        };
        statusBtn.Click += (s, e) => _plugin.RequestSystemStatus();

        buttonPanel.Children.Add(connectBtn);
        buttonPanel.Children.Add(getBPBtn);
        buttonPanel.Children.Add(statusBtn);
        mainPanel.Children.Add(buttonPanel);

        // Diagnostics
        var diagPanel = new StackPanel { Margin = new Thickness(0, 20, 0, 0) };
        diagPanel.Children.Add(new TextBlock { Text = "Diagnostics:", FontWeight = FontWeights.SemiBold });

        var diagText = new TextBlock
        {
            Text = $"Clutch A: {_plugin._lastClutchA} | Clutch B: {_plugin._lastClutchB} | PWM: {_plugin._lastPWMOutput}",
            FontSize = 10,
            Foreground = Brushes.Gray,
            Margin = new Thickness(0, 5, 0, 0)
        };
        diagPanel.Children.Add(diagText);
        mainPanel.Children.Add(diagPanel);

        Content = new ScrollViewer { Content = mainPanel, VerticalScrollBarVisibility = ScrollBarVisibility.Auto };
    }
}
#endregion
```

---

## 🔧 **Compilation Steps**

### **Using SimHub's Built-in Compiler:**

1. **Paste the code** into SimHub's plugin editor
2. **Click "Build"** → SimHub compiles automatically
3. **Plugin appears** in Additional Plugins list
4. **Enable and configure** your plugin

### **Using Visual Studio:**

1. **Build → Build Solution** (Ctrl+Shift+B)
2. **Copy** `bin/Debug/F1WheelClutch.dll`
3. **Paste** to `SimHub/Plugins/` folder
4. **Restart SimHub**

---

## 🎯 **Plugin Features**

### **Automatic Integration:**

- ✅ **Real-time bite point sync** with Arduino
- ✅ **Dashboard property exposure** for widgets
- ✅ **Manual bite point control** via plugin UI
- ✅ **Connection status monitoring**
- ✅ **Serial communication handling**

### **Dashboard Properties Exposed:**

- `F1Wheel.ClutchBitePoint` - Current bite point percentage
- `F1Wheel.SystemActive` - Whether clutch mode is active
- `F1Wheel.IsConnected` - Arduino connection status
- `F1Wheel.LastClutchA/B` - Raw sensor readings
- `F1Wheel.LastPWMOutput` - PWM output value

Your dashboard widgets will automatically use these properties for real-time updates! 🎉
