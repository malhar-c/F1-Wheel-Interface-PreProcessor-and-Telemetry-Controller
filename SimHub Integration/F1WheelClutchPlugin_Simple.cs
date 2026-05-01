/*
 * F1 Wheel Hardware Control Plugin for SimHub - Multi-Tab Configuration Interface
 * 
 * Purpose: Simple interface for configuring F1 steering wheel hardware settings
 * Features:
 * - Dual clutch bite point configuration
 * - Multi-tab interface for wheel settings
 * - Integration with SimHub's Arduino communication
 * - Future expansion for button mapping and other wheel controls
 * - No game data processing - pure hardware configuration
 */

using System;
using System.ComponentModel;
using System.Windows.Controls;
using System.Windows;
using System.Windows.Media;
using System.Windows.Threading;
using System.Windows.Controls.Primitives;
using SimHub.Plugins;
using SimHub.Plugins.UI;
using GameReaderCommon;
using SerialDash;
using System.Collections.Generic;
using System.Linq;

public class F1WheelHardwareConfigSettings
{
    public double ClutchBitePoint { get; set; }
    // Hall sensor calibration: raw ADC endpoints for each lever.
    // RestX = lever fully released, FullX = lever fully pressed.
    public int CalRestA { get; set; }
    public int CalFullA { get; set; }
    public int CalRestB { get; set; }
    public int CalFullB { get; set; }

    public F1WheelHardwareConfigSettings()
    {
        ClutchBitePoint = 50.0;
        CalRestA = 0;
        CalFullA = 1023;
        CalRestB = 0;
        CalFullB = 1023;
    }
}

[PluginDescription("F1 Wheel Hardware Configuration - Simple interface for F1 steering wheel settings and controls")]
[PluginAuthor("Malhar Chakraborty")]
[PluginName("F1WheelHardwareConfig")]
public class F1WheelHardwareConfigPlugin : IPlugin, IDataPlugin, IWPFSettingsV2
{
    #region Private Fields    // Clutch system settings
    private double _clutchBitePoint = 50.0;
    private bool _clutchAdjustmentMode = false;
    
    // Wheel hardware status
    private bool _arduinoConnected = false;
    private int _lastClutchA = 0;
    private int _lastClutchB = 0;
    private int _lastRotary1Position = 0;
    private int _lastPWMOutput = 0;
    private DateTime _lastUpdate = DateTime.Now;
    
    // Hall sensor calibration endpoints (raw ADC values)
    private int _calRestA = 0;
    private int _calFullA = 1023;
    private int _calRestB = 0;
    private int _calFullB = 1023;
    
    // Future wheel settings (for upcoming tabs)
    private Dictionary<string, int> _buttonMappings = new Dictionary<string, int>();
    private Dictionary<string, object> _wheelSettings = new Dictionary<string, object>();
    
    // UI management
    private DispatcherTimer _uiUpdateTimer;
    private F1WheelConfigSettingsControl _settingsControl;
    private string _lastArduinoData = "";
      // Device constants - CORRECTED from Arduino source
    private const string DEVICE_ID = "f35eabd7-6b75-4e14-812d-6c88668e76fb";
    private const string DEVICE_NAME = "Redbull RB19 Steering Interface Pre-Processor";
    public const string PLUGIN_VERSION = "v3.2.10";

    // OnArduinoMessage event integration: messages arrive directly with DeviceDetails
    // attached, eliminating the LoggingLastMessage overwrite race entirely.
    private DeviceDetails _ourDevice;                  // cached reference; InUse is live
    private DateTime _lastMessageReceived = DateTime.MinValue;
    private PluginManager.DebugMessageArrivedDelegate _arduinoMsgHandler;
    #endregion

    #region Public Properties for SimHub
    // Clutch Properties
    public double ClutchBitePoint
    {
        get { return _clutchBitePoint; }
        set { _clutchBitePoint = Math.Max(10.0, Math.Min(90.0, value)); }
    }

    public bool ClutchAdjustmentMode
    {
        get { return _clutchAdjustmentMode; }
        set { _clutchAdjustmentMode = value; }
    }
    
    // Hardware Status Properties
    public bool ArduinoConnected 
    { 
        get { return _arduinoConnected; } 
    }
    
    public int ClutchAValue { get { return _lastClutchA; } }
    public int ClutchBValue { get { return _lastClutchB; } }
    public int Rotary1Position { get { return _lastRotary1Position; } }
    public int PWMOutput { get { return _lastPWMOutput; } }

    // Calibration properties
    public int CalRestA { get { return _calRestA; } }
    public int CalFullA { get { return _calFullA; } }
    public int CalRestB { get { return _calRestB; } }
    public int CalFullB { get { return _calFullB; } }

    public void SetCalibration(int restA, int fullA, int restB, int fullB)
    {
        _calRestA = restA;
        _calFullA = fullA;
        _calRestB = restB;
        _calFullB = fullB;
        this.SaveCommonSettings("GeneralSettings", new F1WheelHardwareConfigSettings
        {
            ClutchBitePoint = _clutchBitePoint,
            CalRestA = _calRestA,
            CalFullA = _calFullA,
            CalRestB = _calRestB,
            CalFullB = _calFullB
        });
    }
    // Debug/Info Properties
    public string LastArduinoData { get { return _lastArduinoData; } }
    public string LastUpdateTime { get { return _lastUpdate.ToString("HH:mm:ss.fff"); } }
    public string LastDeviceUniqueId { get { return _ourDevice == null ? "(no Arduino message yet)" : _ourDevice.UniqueId; } }
    public bool DeviceInUse { get { return _ourDevice != null && _ourDevice.InUse; } }
    #endregion

    #region IPlugin Implementation
    public PluginManager PluginManager { get; set; }

    public void Init(PluginManager pluginManager)
    {
        PluginManager = pluginManager;

        // Load persisted settings
        var settings = this.ReadCommonSettings<F1WheelHardwareConfigSettings>("GeneralSettings", () => new F1WheelHardwareConfigSettings());
        _clutchBitePoint = Math.Max(10.0, Math.Min(90.0, settings.ClutchBitePoint));
        _calRestA = settings.CalRestA;
        _calFullA = settings.CalFullA;
        _calRestB = settings.CalRestB;
        _calFullB = settings.CalFullB;
        
        // Expose properties to SimHub for dashboard use        
        this.AttachDelegate("ClutchBitePoint", () => _clutchBitePoint);
        this.AttachDelegate("ClutchAdjustmentMode", () => _clutchAdjustmentMode);
        this.AttachDelegate("ArduinoConnected", () => _arduinoConnected);
        this.AttachDelegate("ClutchAValue", () => _lastClutchA);
        this.AttachDelegate("ClutchBValue", () => _lastClutchB);
        this.AttachDelegate("Rotary1Position", () => _lastRotary1Position);
        this.AttachDelegate("PWMOutput", () => _lastPWMOutput);
        // Calibration properties (exposed so they can be used in SimHub device custom protocol expression)
        this.AttachDelegate("CalRestA", () => _calRestA);
        this.AttachDelegate("CalFullA", () => _calFullA);
        this.AttachDelegate("CalRestB", () => _calRestB);
        this.AttachDelegate("CalFullB", () => _calFullB);
        
        // Expose button-assignable actions with proper plugin context
        // Corrected prefix to F1WheelHardwareConfigPlugin
        // this.AddAction("F1WheelHardwareConfigPlugin.IncreaseBitePoint", (a, b) => {
        //     IncreaseBitePoint();
        // });
        // this.AddAction("F1WheelHardwareConfigPlugin.DecreaseBitePoint", (a, b) => {
        //     DecreaseBitePoint();
        // });
        // this.AddAction("F1WheelHardwareConfigPlugin.ResetBitePoint", (a, b) => {
        //     ResetClutchSettings();
        // });
        // this.AddAction("F1WheelHardwareConfigPlugin.ToggleAdjustmentMode", (a, b) => {
        //     ToggleAdjustmentMode();
        // });
        
        // Also add actions without the plugin prefix (for compatibility)
        this.AddAction("IncreaseBitePoint", (a, b) => {
            IncreaseBitePoint();
        });
        this.AddAction("DecreaseBitePoint", (a, b) => {
            DecreaseBitePoint();
        });
        this.AddAction("ResetBitePoint", (a, b) => {
            ResetClutchSettings();
        });
        this.AddAction("ToggleAdjustmentMode", (a, b) => {
            ToggleAdjustmentMode();
        });
        
        // Subscribe to Arduino debug-message event — direct delivery with DeviceDetails,
        // no LoggingLastMessage polling.
        _arduinoMsgHandler = OnArduinoMessageReceived;
        pluginManager.OnArduinoMessage += _arduinoMsgHandler;

        // Setup UI update timer
        _uiUpdateTimer = new DispatcherTimer();
        _uiUpdateTimer.Interval = TimeSpan.FromMilliseconds(200);
        _uiUpdateTimer.Tick += UiUpdateTimer_Tick;
        _uiUpdateTimer.Start();
    }

    public void End(PluginManager pluginManager)
    {
        // Unsubscribe from Arduino message event
        if (_arduinoMsgHandler != null && pluginManager != null)
        {
            pluginManager.OnArduinoMessage -= _arduinoMsgHandler;
            _arduinoMsgHandler = null;
        }

        // Persist settings before shutdown
        this.SaveCommonSettings("GeneralSettings", new F1WheelHardwareConfigSettings
        {
            ClutchBitePoint = _clutchBitePoint,
            CalRestA = _calRestA,
            CalFullA = _calFullA,
            CalRestB = _calRestB,
            CalFullB = _calFullB
        });

        if (_uiUpdateTimer != null)
        {
            _uiUpdateTimer.Stop();
            _uiUpdateTimer = null;
        }
    }    private void UiUpdateTimer_Tick(object sender, EventArgs e)
    {
        if (_settingsControl != null)
            _settingsControl.UpdateUI();
    }
    #endregion

    #region IDataPlugin Implementation
    public void DataUpdate(PluginManager pluginManager, ref GameData data)
    {
        _lastUpdate = DateTime.Now;

        bool prev = _arduinoConnected;
        // Two confirming signals:
        //   1) DeviceDetails.InUse — live state on the SerialDash-owned object
        //   2) Recent message activity — backstop in case InUse staleness
        bool inUse = _ourDevice != null && _ourDevice.InUse;
        bool recentMessage = (DateTime.Now - _lastMessageReceived).TotalSeconds < 10;
        _arduinoConnected = inUse || recentMessage;

        if (prev && !_arduinoConnected)
        {
            _lastClutchA = 0;
            _lastClutchB = 0;
            _lastRotary1Position = 0;
            _lastPWMOutput = 0;
            _lastArduinoData = "Disconnected";
        }
    }

    // Fired by SimHub on its serial-read thread for every Arduino debug message
    // (FlowSerialDebugPrintLn / packet 0x07). DeviceDetails identifies the source.
    private void OnArduinoMessageReceived(DeviceDetails details, string message)
    {
        if (details == null) return;
        if (!string.Equals(details.UniqueId, DEVICE_ID, StringComparison.OrdinalIgnoreCase))
            return;

        _ourDevice = details;
        _lastMessageReceived = DateTime.Now;
        ParseArduinoMessage(message);
        _lastArduinoData = string.Format("[{0}] {1}",
            details.UniqueId.Length >= 8 ? details.UniqueId.Substring(0, 8) : details.UniqueId,
            message);
    }
    #endregion

    #region IWPFSettingsV2 Implementation
    public Control GetWPFSettingsControl(PluginManager pluginManager)
    {
        _settingsControl = new F1WheelConfigSettingsControl(this);
        return _settingsControl;
    }

    public string LeftMenuTitle 
    { 
        get { return "F1 Wheel Config"; } 
    }

    public ImageSource PictureIcon 
    { 
        get { return null; }    }
    #endregion
    
    #region Arduino Communication (via SimHub)
    private void ParseArduinoMessage(string logMsg)
    {
        if (string.IsNullOrEmpty(logMsg)) return;
        try
        {
            // CLT:A:xxx;B:yyy — streamed every 100ms only when clutch adjust mode is active
            if (logMsg.Contains("CLT:A:"))
            {
                int cltIdx = logMsg.IndexOf("CLT:A:");
                int bIdx = logMsg.IndexOf(";B:", cltIdx);
                if (bIdx > cltIdx)
                {
                    string aStr = logMsg.Substring(cltIdx + 6, bIdx - cltIdx - 6);
                    string bRaw = logMsg.Substring(bIdx + 3);
                    int bEnd = 0;
                    while (bEnd < bRaw.Length && char.IsDigit(bRaw[bEnd])) bEnd++;
                    int a = _lastClutchA, b = _lastClutchB;
                    if (int.TryParse(aStr, out a) && int.TryParse(bRaw.Substring(0, bEnd), out b))
                    {
                        _lastClutchA = a;
                        _lastClutchB = b;
                        _lastArduinoData = string.Format("A:{0} B:{1}", a, b);
                    }
                }
            }

            // ROT1:n — sent once after connection (first idle debounce) and on every position change
            if (logMsg.Contains("ROT1:"))
            {
                int idx = logMsg.IndexOf("ROT1:");
                string raw = logMsg.Substring(idx + 5);
                int end = 0;
                while (end < raw.Length && char.IsDigit(raw[end])) end++;
                if (end > 0)
                {
                    int pos;
                    if (int.TryParse(raw.Substring(0, end), out pos) && pos >= 1 && pos <= 12)
                    {
                        _lastRotary1Position = pos;
                        _lastArduinoData = string.Format("Rotary 1: {0}", pos);
                    }
                }
            }
        }
        catch
        {
            _lastArduinoData = "Parse error";
        }
    }
    #endregion

    #region Wheel Configuration Methods
    public void SetClutchBitePoint(double value)
    {
        ClutchBitePoint = value;
    }

    public void ResetClutchSettings()
    {
        if (!_clutchAdjustmentMode) return;
        ClutchBitePoint = 50.0;
        if (_settingsControl != null)
            Application.Current.Dispatcher.Invoke(() => _settingsControl.UpdateUI());
    }// Button-assignable methods for SimHub
    public void IncreaseBitePoint()
    {
        if (!_clutchAdjustmentMode) return;
        
        double newValue = Math.Min(90.0, _clutchBitePoint + 0.5);
        SetClutchBitePoint(newValue);
        if (_settingsControl != null)
        {
            Application.Current.Dispatcher.Invoke(() => _settingsControl.UpdateUI());
        }
    }

    public void DecreaseBitePoint()
    {
        if (!_clutchAdjustmentMode) return;
        
        double newValue = Math.Max(10.0, _clutchBitePoint - 0.5);
        SetClutchBitePoint(newValue);
        if (_settingsControl != null)
        {
            Application.Current.Dispatcher.Invoke(() => _settingsControl.UpdateUI());
        }
    }

    public void ToggleAdjustmentMode()
    {
        _clutchAdjustmentMode = !_clutchAdjustmentMode;
        if (_settingsControl != null)
        {
            Application.Current.Dispatcher.Invoke(() => _settingsControl.UpdateUI());
        }
    }

    // Future methods for other wheel settings
    public void SetButtonMapping(string buttonName, int function)
    {
        _buttonMappings[buttonName] = function;
        // TODO: Send to Arduino when needed
    }

    public void SetWheelSetting(string settingName, object value)
    {
        _wheelSettings[settingName] = value;
        // TODO: Send to Arduino when needed
    }

    public Dictionary<string, object> GetCurrentSettings()
    {        return new Dictionary<string, object>
        {
            {"ClutchBitePoint", _clutchBitePoint},
            {"ClutchAdjustmentMode", _clutchAdjustmentMode},
            {"ArduinoConnected", _arduinoConnected},
            {"LastUpdate", _lastUpdate},
            {"ButtonMappings", _buttonMappings},
            {"WheelSettings", _wheelSettings}
        };
    }
    #endregion
}

#region Main Settings Control with Tabs
public class F1WheelConfigSettingsControl : UserControl
{
    private F1WheelHardwareConfigPlugin _plugin;
    private TabControl _tabControl;
    private ClutchConfigTab _clutchTab;
    private CalibrationTab _calibrationTab;
    private WheelConfigTab _wheelTab;
    private ButtonMappingTab _buttonTab;
    private DiagnosticsTab _diagnosticsTab;    public F1WheelConfigSettingsControl(F1WheelHardwareConfigPlugin plugin)
    {
        if (plugin == null)
            throw new ArgumentNullException("plugin");
        _plugin = plugin;
        CreateTabs();
    }

    private void CreateTabs()
    {
        _tabControl = new TabControl();
        
        // Clutch Configuration Tab (Primary)
        var clutchTabItem = new TabItem();
        clutchTabItem.Header = "Dual Clutch";
        _clutchTab = new ClutchConfigTab(_plugin);
        clutchTabItem.Content = _clutchTab;
        _tabControl.Items.Add(clutchTabItem);

        // Sensor Calibration Tab
        var calTabItem = new TabItem();
        calTabItem.Header = "Calibration";
        _calibrationTab = new CalibrationTab(_plugin);
        calTabItem.Content = _calibrationTab;
        _tabControl.Items.Add(calTabItem);
        
        // Wheel Settings & Button Mapping tabs omitted (not needed for current workflow)
        
        // Diagnostics Tab
        var diagnosticsTabItem = new TabItem();
        diagnosticsTabItem.Header = "Diagnostics";
        _diagnosticsTab = new DiagnosticsTab(_plugin);
        diagnosticsTabItem.Content = _diagnosticsTab;
        _tabControl.Items.Add(diagnosticsTabItem);

        this.Content = _tabControl;
    }    public void UpdateUI()
    {
        if (_clutchTab != null) _clutchTab.UpdateUI();
        if (_calibrationTab != null) _calibrationTab.UpdateUI();
        if (_wheelTab != null) _wheelTab.UpdateUI();
        if (_buttonTab != null) _buttonTab.UpdateUI();
        if (_diagnosticsTab != null) _diagnosticsTab.UpdateUI();
    }
}
#endregion

#region Clutch Configuration Tab
public class ClutchConfigTab : UserControl
{    private F1WheelHardwareConfigPlugin _plugin;
    private Slider _bitePointSlider;
    private TextBlock _bitePointValue;
    private Button _resetButton;
    private TextBlock _statusText;
    private CheckBox _adjustmentModeCheckBox;public ClutchConfigTab(F1WheelHardwareConfigPlugin plugin)
    {
        if (plugin == null)
            throw new ArgumentNullException("plugin");
        _plugin = plugin;
        CreateUI();
    }

    private void CreateUI()
    {
        StackPanel mainPanel = new StackPanel();
        mainPanel.Margin = new Thickness(15);

        // Title
        TextBlock title = new TextBlock();
        title.Text = "Dual Clutch Bite Point Configuration";
        title.FontSize = 16;
        title.FontWeight = FontWeights.Bold;
        title.Margin = new Thickness(0, 0, 0, 15);
        mainPanel.Children.Add(title);        // Connection Status
        _statusText = new TextBlock();
        _statusText.Margin = new Thickness(0, 0, 0, 10);
        mainPanel.Children.Add(_statusText);

        // Bite Point Control Group
        GroupBox bitePointGroup = new GroupBox();
        bitePointGroup.Header = "Bite Point Setting";
        bitePointGroup.Margin = new Thickness(0, 0, 0, 15);

        StackPanel bitePointPanel = new StackPanel();
        
        // Value display
        _bitePointValue = new TextBlock();
        _bitePointValue.FontSize = 14;
        _bitePointValue.FontWeight = FontWeights.Bold;
        _bitePointValue.Margin = new Thickness(0, 0, 0, 10);
        bitePointPanel.Children.Add(_bitePointValue);
        
        // Instantiate slider for bite point adjustments
        _bitePointSlider = new Slider(); // instantiate slider before property assignments
        
        // Slider properties
        _bitePointSlider.Minimum = 10;
        _bitePointSlider.Maximum = 90;
        _bitePointSlider.Value = _plugin != null ? _plugin.ClutchBitePoint : 50.0; // Safe access with null check
        _bitePointSlider.TickFrequency = 0.5;  // 0.5% steps for fine adjustment
        _bitePointSlider.IsSnapToTickEnabled = true;
        _bitePointSlider.TickPlacement = System.Windows.Controls.Primitives.TickPlacement.BottomRight;
        _bitePointSlider.ValueChanged += BitePointSlider_ValueChanged;
        bitePointPanel.Children.Add(_bitePointSlider);        // Add adjustment-mode toggle (visible for testing)
        _adjustmentModeCheckBox = new CheckBox();
        _adjustmentModeCheckBox.Content = "Enable Adjustment Mode";
        _adjustmentModeCheckBox.IsChecked = _plugin != null && _plugin.ClutchAdjustmentMode;
        _adjustmentModeCheckBox.Margin = new Thickness(0, 10, 0, 10);
        _adjustmentModeCheckBox.Checked += AdjustmentModeCheckBox_Changed;
        _adjustmentModeCheckBox.Unchecked += AdjustmentModeCheckBox_Changed;
        bitePointPanel.Children.Add(_adjustmentModeCheckBox);
          // Buttons
        StackPanel buttonPanel = new StackPanel();
        buttonPanel.Orientation = Orientation.Horizontal;
        buttonPanel.Margin = new Thickness(0, 10, 0, 0);
        
        _resetButton = new Button();
        _resetButton.Content = "Reset to Default (50%)";
        _resetButton.Padding = new Thickness(15, 5, 15, 5);
        _resetButton.Click += ResetButton_Click;
        buttonPanel.Children.Add(_resetButton);
        
        bitePointPanel.Children.Add(buttonPanel);
        bitePointGroup.Content = bitePointPanel;
        mainPanel.Children.Add(bitePointGroup);        // Button Configuration Group
        GroupBox buttonConfigGroup = new GroupBox();
        buttonConfigGroup.Header = "Button Assignment";
        buttonConfigGroup.Margin = new Thickness(0, 0, 0, 15);
        
        StackPanel buttonConfigPanel = new StackPanel();
        
        // Instructions
        TextBlock buttonConfigInstructions = new TextBlock();
        buttonConfigInstructions.Text = "Click 'Configure' to assign physical buttons to these actions:";
        buttonConfigInstructions.FontWeight = FontWeights.SemiBold;
        buttonConfigInstructions.Margin = new Thickness(0, 0, 0, 15);
        buttonConfigInstructions.TextWrapping = TextWrapping.Wrap;
        buttonConfigPanel.Children.Add(buttonConfigInstructions);        // ControlsEditor for each action - these create the "Click to configure" buttons
        // Corrected ActionName to F1WheelHardwareConfigPlugin.ActionName
        var increaseBitePointEditor = new ControlsEditor();
        increaseBitePointEditor.ActionName = "F1WheelHardwareConfigPlugin.IncreaseBitePoint";
        increaseBitePointEditor.FriendlyName = "Increase Bite Point (+0.5%)";
        increaseBitePointEditor.Margin = new Thickness(0, 5, 0, 5);
        buttonConfigPanel.Children.Add(increaseBitePointEditor);
        
        var decreaseBitePointEditor = new ControlsEditor();
        decreaseBitePointEditor.ActionName = "F1WheelHardwareConfigPlugin.DecreaseBitePoint";
        decreaseBitePointEditor.FriendlyName = "Decrease Bite Point (-0.5%)";
        decreaseBitePointEditor.Margin = new Thickness(0, 5, 0, 5);
        buttonConfigPanel.Children.Add(decreaseBitePointEditor);
        
        var resetBitePointEditor = new ControlsEditor();
        resetBitePointEditor.ActionName = "F1WheelHardwareConfigPlugin.ResetBitePoint";
        resetBitePointEditor.FriendlyName = "Reset Bite Point (50%)";
        resetBitePointEditor.Margin = new Thickness(0, 5, 0, 5);
        buttonConfigPanel.Children.Add(resetBitePointEditor);
        
        var toggleModeEditor = new ControlsEditor();
        toggleModeEditor.ActionName = "F1WheelHardwareConfigPlugin.ToggleAdjustmentMode";
        toggleModeEditor.FriendlyName = "Toggle Adjustment Mode";
        toggleModeEditor.Margin = new Thickness(0, 5, 0, 5);
        buttonConfigPanel.Children.Add(toggleModeEditor);
        
        buttonConfigGroup.Content = buttonConfigPanel;
        mainPanel.Children.Add(buttonConfigGroup);

        // Information
        TextBlock infoText = new TextBlock();
        infoText.Text = "Bite point controls where the clutch engages. Lower values = earlier engagement.\n" +
                       "Range: 10% - 90%. Test your settings before racing!";
        infoText.FontStyle = FontStyles.Italic;
        infoText.TextWrapping = TextWrapping.Wrap;
        infoText.Foreground = new SolidColorBrush(Colors.Gray);
        mainPanel.Children.Add(infoText);

        this.Content = mainPanel;
        this.Loaded += (s, e) => UpdateUI();
    }    private void BitePointSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        if (_plugin != null)
        {
            _plugin.SetClutchBitePoint(e.NewValue);
            UpdateUI();        }
    }

    private void AdjustmentModeCheckBox_Changed(object sender, RoutedEventArgs e)
    {
        if (_plugin != null && _adjustmentModeCheckBox != null)
        {
            _plugin.ClutchAdjustmentMode = _adjustmentModeCheckBox.IsChecked == true;
            UpdateUI();
        }
    }    private void ResetButton_Click(object sender, RoutedEventArgs e)
    {
        if (_plugin != null)
        {
            // Only allow reset when adjustment mode is enabled
            if (!_plugin.ClutchAdjustmentMode) return;
            
            _plugin.ResetClutchSettings();
            if (_bitePointSlider != null)
            {
                _bitePointSlider.Value = 50.0;
            }
            UpdateUI();
        }
    }    public void UpdateUI()
    {
        if (_plugin != null)
        {
            _bitePointValue.Text = string.Format("Bite Point: {0:F1}%", _plugin.ClutchBitePoint);
            _adjustmentModeCheckBox.IsChecked = _plugin.ClutchAdjustmentMode;
            
            // Update slider position to match current bite point value
            if (_bitePointSlider.Value != _plugin.ClutchBitePoint)
            {
                _bitePointSlider.Value = _plugin.ClutchBitePoint;
            }
            
            // Enable controls only when in adjustment mode
            _bitePointSlider.IsEnabled = _plugin.ClutchAdjustmentMode;
            _resetButton.IsEnabled = _plugin.ClutchAdjustmentMode;
            
            // Update bite point value text color based on mode
            if (_plugin.ClutchAdjustmentMode)
            {
                _bitePointValue.Foreground = new SolidColorBrush(Colors.Orange);
                _bitePointValue.Text += " (ADJUSTMENT MODE Active)";
            }
            else
            {
                _bitePointValue.Foreground = new SolidColorBrush(Colors.Black);
            }
              // Update connection status
            if (_plugin.ArduinoConnected)
            {
                _statusText.Text = "✓ Arduino Connected - Live Configuration Active";
                _statusText.Foreground = new SolidColorBrush(Colors.Green);
            }
            else
            {
                _statusText.Text = "⚠ Arduino Not Connected - Settings will be applied when connected";
                _statusText.Foreground = new SolidColorBrush(Colors.Orange);
            }
        }
    }
}
#endregion

#region Calibration Tab
public class CalibrationTab : UserControl
{
    private F1WheelHardwareConfigPlugin _plugin;

    // Live raw readings
    private TextBlock _liveAText;
    private TextBlock _liveBText;

    // Editable calibration fields
    private TextBox _restABox;
    private TextBox _fullABox;
    private TextBox _restBBox;
    private TextBox _fullBBox;

    // Generated expression text
    private TextBox _expressionBox;

    public CalibrationTab(F1WheelHardwareConfigPlugin plugin)
    {
        _plugin = plugin;
        CreateUI();
    }

    private void CreateUI()
    {
        ScrollViewer scroll = new ScrollViewer();
        scroll.VerticalScrollBarVisibility = ScrollBarVisibility.Auto;

        StackPanel panel = new StackPanel();
        panel.Margin = new Thickness(15);

        // Title
        TextBlock title = new TextBlock();
        title.Text = "Hall Sensor Calibration (SS49E)";
        title.FontSize = 16;
        title.FontWeight = FontWeights.Bold;
        title.Margin = new Thickness(0, 0, 0, 5);
        panel.Children.Add(title);

        // Instructions
        TextBlock instructions = new TextBlock();
        instructions.Text =
            "The SS49E sensor outputs ~Vcc/2 at rest. You must calibrate so the clutch\n" +
            "output is 0 when released and 1023 when fully pressed.\n\n" +
            "Step 1: Leave both levers fully RELEASED. Note the Live values below.\n" +
            "Step 2: Click \"Capture\" next to REST for each lever.\n" +
            "Step 3: Pull each lever fully PRESSED. Click \"Capture\" next to FULL.\n" +
            "Step 4: Click \"Save Calibration\". Update hardwareSettings.h with the values shown,\n" +
            "        then reflash the firmware.";
        instructions.TextWrapping = TextWrapping.Wrap;
        instructions.Foreground = new SolidColorBrush(Colors.DimGray);
        instructions.Margin = new Thickness(0, 0, 0, 15);
        panel.Children.Add(instructions);

        // Live readings
        GroupBox liveGroup = new GroupBox();
        liveGroup.Header = "Live Sensor Reading (raw ADC — valid only with default 0/1023 calibration)";
        liveGroup.Margin = new Thickness(0, 0, 0, 15);
        StackPanel livePanel = new StackPanel();

        _liveAText = new TextBlock();
        _liveAText.FontSize = 14;
        _liveAText.Margin = new Thickness(5, 5, 5, 2);
        livePanel.Children.Add(_liveAText);

        _liveBText = new TextBlock();
        _liveBText.FontSize = 14;
        _liveBText.Margin = new Thickness(5, 2, 5, 5);
        livePanel.Children.Add(_liveBText);

        // Rotary position moved to Diagnostics tab

        liveGroup.Content = livePanel;
        panel.Children.Add(liveGroup);

        // Calibration inputs
        GroupBox calGroup = new GroupBox();
        calGroup.Header = "Calibration Values";
        calGroup.Margin = new Thickness(0, 0, 0, 15);

        Grid calGrid = new Grid();
        calGrid.Margin = new Thickness(5);
        calGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(80) });
        calGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(90) });
        calGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        calGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(90) });
        calGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        calGrid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        calGrid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        calGrid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });

        // Header row
        AddGridLabel(calGrid, "", 0, 0, FontWeights.Bold);
        AddGridLabel(calGrid, "REST (released)", 0, 1, FontWeights.Bold);
        AddGridLabel(calGrid, "", 0, 2, FontWeights.Normal);
        AddGridLabel(calGrid, "FULL (pressed)", 0, 3, FontWeights.Bold);
        AddGridLabel(calGrid, "", 0, 4, FontWeights.Normal);

        // Clutch A row
        AddGridLabel(calGrid, "Clutch A:", 1, 0, FontWeights.Normal);
        _restABox = AddGridTextBox(calGrid, _plugin.CalRestA.ToString(), 1, 1);
        AddGridCapture(calGrid, "Capture", 1, 2, () =>
        {
            _restABox.Text = _plugin.ClutchAValue.ToString();
        });
        _fullABox = AddGridTextBox(calGrid, _plugin.CalFullA.ToString(), 1, 3);
        AddGridCapture(calGrid, "Capture", 1, 4, () =>
        {
            _fullABox.Text = _plugin.ClutchAValue.ToString();
        });

        // Clutch B row
        AddGridLabel(calGrid, "Clutch B:", 2, 0, FontWeights.Normal);
        _restBBox = AddGridTextBox(calGrid, _plugin.CalRestB.ToString(), 2, 1);
        AddGridCapture(calGrid, "Capture", 2, 2, () =>
        {
            _restBBox.Text = _plugin.ClutchBValue.ToString();
        });
        _fullBBox = AddGridTextBox(calGrid, _plugin.CalFullB.ToString(), 2, 3);
        AddGridCapture(calGrid, "Capture", 2, 4, () =>
        {
            _fullBBox.Text = _plugin.ClutchBValue.ToString();
        });

        calGroup.Content = calGrid;
        panel.Children.Add(calGroup);

        // Save button
        Button saveBtn = new Button();
        saveBtn.Content = "Save Calibration";
        saveBtn.Padding = new Thickness(20, 6, 20, 6);
        saveBtn.Margin = new Thickness(0, 0, 0, 20);
        saveBtn.Click += SaveCalibration_Click;
        panel.Children.Add(saveBtn);

        // SimHub expression generator
        GroupBox exprGroup = new GroupBox();
        exprGroup.Header = "SimHub Custom Protocol Expression (for runtime calibration — optional)";
        exprGroup.Margin = new Thickness(0, 0, 0, 10);

        StackPanel exprPanel = new StackPanel();
        TextBlock exprNote = new TextBlock();
        exprNote.Text =
            "If you want calibration to apply without reflashing, paste this expression into\n" +
            "SimHub > Hardware > [Your Device] > Custom Protocol. Requires firmware built\n" +
            "with RA/FA/RB/FB parsing support (already included in current firmware).";
        exprNote.TextWrapping = TextWrapping.Wrap;
        exprNote.Foreground = new SolidColorBrush(Colors.DimGray);
        exprNote.Margin = new Thickness(5, 5, 5, 5);
        exprPanel.Children.Add(exprNote);

        _expressionBox = new TextBox();
        _expressionBox.IsReadOnly = true;
        _expressionBox.TextWrapping = TextWrapping.Wrap;
        _expressionBox.FontFamily = new FontFamily("Consolas");
        _expressionBox.FontSize = 11;
        _expressionBox.Margin = new Thickness(5);
        _expressionBox.Padding = new Thickness(5);
        _expressionBox.Background = new SolidColorBrush(Color.FromRgb(240, 240, 240));
        exprPanel.Children.Add(_expressionBox);

        exprGroup.Content = exprPanel;
        panel.Children.Add(exprGroup);

        scroll.Content = panel;
        this.Content = scroll;
        this.Loaded += (s, e) => UpdateUI();
    }

    private void SaveCalibration_Click(object sender, RoutedEventArgs e)
    {
        int restA, fullA, restB, fullB;
        if (!int.TryParse(_restABox.Text, out restA)) restA = 0;
        if (!int.TryParse(_fullABox.Text, out fullA)) fullA = 1023;
        if (!int.TryParse(_restBBox.Text, out restB)) restB = 0;
        if (!int.TryParse(_fullBBox.Text, out fullB)) fullB = 1023;
        restA = Math.Max(0, Math.Min(1023, restA));
        fullA = Math.Max(0, Math.Min(1023, fullA));
        restB = Math.Max(0, Math.Min(1023, restB));
        fullB = Math.Max(0, Math.Min(1023, fullB));
        _plugin.SetCalibration(restA, fullA, restB, fullB);
        UpdateExpression(restA, fullA, restB, fullB);
        // Values are now live SimHub properties — the protocol expression sends them
        // to the Arduino automatically on the next cycle. No reflash needed.
        MessageBox.Show("Calibration saved. Values will be sent to Arduino on next protocol cycle.",
            "Saved", MessageBoxButton.OK, MessageBoxImage.None);
    }

    // The SimHub device custom protocol expression — paste this ONCE into SimHub's device settings.
    // It references plugin properties dynamically so it never needs changing, even after recalibration.
    // Cal values (RA/FA/RB/FB) are sent every cycle, so saving calibration here automatically
    // updates the Arduino on the next send — no reflash needed.
    private static readonly string PROTOCOL_EXPRESSION =
        "'BP:' + format([F1WheelHardwareConfigPlugin.ClutchBitePoint], '0.0') + " +
        "';MODE:' + if([F1WheelHardwareConfigPlugin.ClutchAdjustmentMode], '1', '0') + " +
        "';RA:' + [F1WheelHardwareConfigPlugin.CalRestA] + " +
        "';FA:' + [F1WheelHardwareConfigPlugin.CalFullA] + " +
        "';RB:' + [F1WheelHardwareConfigPlugin.CalRestB] + " +
        "';FB:' + [F1WheelHardwareConfigPlugin.CalFullB]";

    private void UpdateExpression(int restA, int fullA, int restB, int fullB)
    {
        if (_expressionBox != null)
        {
            _expressionBox.Text = PROTOCOL_EXPRESSION;
        }
    }

    public void UpdateUI()
    {
        if (_plugin != null)
        {
            _liveAText.Text = string.Format("Clutch A (raw): {0}", _plugin.ClutchAValue);
            _liveBText.Text = string.Format("Clutch B (raw): {0}", _plugin.ClutchBValue);
            UpdateExpression(_plugin.CalRestA, _plugin.CalFullA, _plugin.CalRestB, _plugin.CalFullB);
        }
    }

    // ---- Grid builder helpers ----
    private void AddGridLabel(Grid g, string text, int row, int col, FontWeight weight)
    {
        TextBlock tb = new TextBlock();
        tb.Text = text;
        tb.FontWeight = weight;
        tb.Margin = new Thickness(4, 4, 8, 4);
        tb.VerticalAlignment = VerticalAlignment.Center;
        Grid.SetRow(tb, row);
        Grid.SetColumn(tb, col);
        g.Children.Add(tb);
    }

    private TextBox AddGridTextBox(Grid g, string initialValue, int row, int col)
    {
        TextBox tb = new TextBox();
        tb.Text = initialValue;
        tb.Width = 75;
        tb.Margin = new Thickness(4);
        tb.VerticalAlignment = VerticalAlignment.Center;
        Grid.SetRow(tb, row);
        Grid.SetColumn(tb, col);
        g.Children.Add(tb);
        return tb;
    }

    private void AddGridCapture(Grid g, string label, int row, int col, System.Action onClick)
    {
        Button btn = new Button();
        btn.Content = label;
        btn.Padding = new Thickness(8, 3, 8, 3);
        btn.Margin = new Thickness(4);
        System.Action captured = onClick;
        btn.Click += (s, e) => { if (captured != null) captured(); };
        Grid.SetRow(btn, row);
        Grid.SetColumn(btn, col);
        g.Children.Add(btn);
    }
}
#endregion

#region Wheel Configuration Tab (Future expansion)
public class WheelConfigTab : UserControl
{
    private F1WheelHardwareConfigPlugin _plugin;

    public WheelConfigTab(F1WheelHardwareConfigPlugin plugin)
    {
        _plugin = plugin;
        CreateUI();
    }

    private void CreateUI()
    {
        StackPanel panel = new StackPanel();
        panel.Margin = new Thickness(15);

        TextBlock title = new TextBlock();
        title.Text = "Wheel Settings";
        title.FontSize = 16;
        title.FontWeight = FontWeights.Bold;
        title.Margin = new Thickness(0, 0, 0, 15);
        panel.Children.Add(title);

        TextBlock placeholder = new TextBlock();
        placeholder.Text = "Future wheel configuration options will appear here:\n\n" +
                          "• Display brightness settings\n" +
                          "• LED configuration\n" +
                          "• Rotary encoder settings\n" +
                          "• Custom wheel functions";
        placeholder.FontStyle = FontStyles.Italic;
        placeholder.Foreground = new SolidColorBrush(Colors.Gray);
        panel.Children.Add(placeholder);

        this.Content = panel;
    }

    public void UpdateUI()
    {
        // Future implementation
    }
}
#endregion

#region Button Mapping Tab (Future expansion)
public class ButtonMappingTab : UserControl
{
    private F1WheelHardwareConfigPlugin _plugin;

    public ButtonMappingTab(F1WheelHardwareConfigPlugin plugin)
    {
        _plugin = plugin;
        CreateUI();
    }

    private void CreateUI()
    {
        StackPanel panel = new StackPanel();
        panel.Margin = new Thickness(15);

        TextBlock title = new TextBlock();
        title.Text = "Button Mapping";
        title.FontSize = 16;
        title.FontWeight = FontWeights.Bold;
        title.Margin = new Thickness(0, 0, 0, 15);
        panel.Children.Add(title);

        TextBlock placeholder = new TextBlock();
        placeholder.Text = "Future button mapping options will appear here:\n\n" +
                          "• Custom button functions\n" +
                          "• Rotary switch mappings\n" +
                          "• Multi-function button modes\n" +
                          "• Macro assignments";
        placeholder.FontStyle = FontStyles.Italic;
        placeholder.Foreground = new SolidColorBrush(Colors.Gray);
        panel.Children.Add(placeholder);

        this.Content = panel;
    }

    public void UpdateUI()
    {
        // Future implementation
    }
}
#endregion

#region Diagnostics Tab
public class DiagnosticsTab : UserControl
{
    private F1WheelHardwareConfigPlugin _plugin;
    private TextBlock _debugText;
    private TextBlock _rotary1Text;

    public DiagnosticsTab(F1WheelHardwareConfigPlugin plugin)
    {
        _plugin = plugin;
        CreateUI();
    }

    private void CreateUI()
    {
        StackPanel mainPanel = new StackPanel();
        mainPanel.Margin = new Thickness(15);

        GroupBox debugGroup = new GroupBox();
        debugGroup.Header = "Hardware Status";
        
        _debugText = new TextBlock();
        _debugText.FontFamily = new FontFamily("Consolas");
        _debugText.Foreground = new SolidColorBrush(Colors.LightGreen);
        _debugText.Background = new SolidColorBrush(Colors.Black);
        _debugText.Padding = new Thickness(10);
        _debugText.Margin = new Thickness(5);
        
        debugGroup.Content = _debugText;
        mainPanel.Children.Add(debugGroup);

        // Rotary position (Rotary 1)
        _rotary1Text = new TextBlock();
        _rotary1Text.FontSize = 14;
        _rotary1Text.Margin = new Thickness(5, 8, 5, 5);
        mainPanel.Children.Add(_rotary1Text);

        this.Content = mainPanel;
    }

    public void UpdateUI()
    {
        var settings = _plugin.GetCurrentSettings();
        var debugInfo = string.Format(
            "=== F1 Wheel Hardware Status ===\n" +
            "Plugin Version: {0}\n" +
            "Time: {1}\n\n" +
            "Device ID: {2}\n" +
            "Device Name: {3}\n\n" +
            "Arduino Connection: {4}  (InUse: {9})\n" +
            "Clutch Adjustment Mode: {5}\n" +
            "Clutch Bite Point: {6:F1}%\n\n" +
            "Last Arduino Message:\n{7}\n" +
            "Last Update: {8}",
            F1WheelHardwareConfigPlugin.PLUGIN_VERSION,
            DateTime.Now.ToString("HH:mm:ss"),
            "f35eabd7-6b75-4e14-812d-6c88668e76fb",
            "Redbull RB19 Steering Interface Pre-Processor",
            settings["ArduinoConnected"],
            (bool)settings["ClutchAdjustmentMode"] ? "Active" : "Inactive",
            settings["ClutchBitePoint"],
            _plugin.LastArduinoData,
            settings["LastUpdate"],
            _plugin.DeviceInUse
        );

        _debugText.Text = debugInfo;
        // Update rotary1 position display
        if (_rotary1Text != null)
        {
            _rotary1Text.Text = string.Format("Rotary 1 Position: {0}", _plugin.Rotary1Position);
        }
    }
}
#endregion
