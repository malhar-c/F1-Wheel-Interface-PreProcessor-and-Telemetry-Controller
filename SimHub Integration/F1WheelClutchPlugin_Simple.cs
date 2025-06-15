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
using System.Collections.Generic;
using System.Linq;

[PluginDescription("F1 Wheel Hardware Configuration - Simple interface for F1 steering wheel settings and controls")]
[PluginAuthor("Malhar Chakraborty")]
[PluginName("F1WheelHardwareConfig")]
public class F1WheelHardwareConfigPlugin : IPlugin, IDataPlugin, IWPFSettingsV2
{
    #region Private Fields    // Clutch system settings
    private double _clutchBitePoint = 50.0;
    private bool _clutchAdjustmentMode = false;
    
    // Wheel hardware status
    private bool _arduinoConnected = false;    private int _lastClutchA = 0;
    private int _lastClutchB = 0;
    private int _lastPWMOutput = 0;
    private DateTime _lastUpdate = DateTime.Now;
    
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
    
    // Log messages buffer for robust connection detection
    private string _lastProcessedLogMessage = ""; // Store the last processed log message
    #endregion

    #region Public Properties for SimHub
    // Clutch Properties
    public double ClutchBitePoint 
    { 
        get { return _clutchBitePoint; } 
        set 
        { 
            _clutchBitePoint = Math.Max(10.0, Math.Min(90.0, value));
            SendToArduino("CLUTCH_BP", _clutchBitePoint.ToString("F1"));
        }
    }
      public bool ClutchAdjustmentMode
    {
        get { return _clutchAdjustmentMode; }
        set 
        { 
            _clutchAdjustmentMode = value;
            // Send mode change to Arduino
            SendToArduino("CLUTCH_MODE", value ? "1" : "0");
        }
    }
    
    // Hardware Status Properties
    public bool ArduinoConnected 
    { 
        get { return _arduinoConnected; } 
    }
    
    public int ClutchAValue { get { return _lastClutchA; } }
    public int ClutchBValue { get { return _lastClutchB; } }
    public int PWMOutput { get { return _lastPWMOutput; } }    
    // Debug/Info Properties
    public string LastArduinoData { get { return _lastArduinoData; } }
    public string LastUpdateTime { get { return _lastUpdate.ToString("HH:mm:ss.fff"); } }
    public string LastProcessedLogMessage { get { return _lastProcessedLogMessage; } } // Expose last processed log message
    #endregion

    #region IPlugin Implementation
    public PluginManager PluginManager { get; set; }

    public void Init(PluginManager pluginManager)
    {
        PluginManager = pluginManager;
        
        // Expose properties to SimHub for dashboard use        
        this.AttachDelegate("ClutchBitePoint", () => _clutchBitePoint);
        this.AttachDelegate("ClutchAdjustmentMode", () => _clutchAdjustmentMode);
        this.AttachDelegate("ArduinoConnected", () => _arduinoConnected);
        this.AttachDelegate("ClutchAValue", () => _lastClutchA);
        this.AttachDelegate("ClutchBValue", () => _lastClutchB);
        this.AttachDelegate("PWMOutput", () => _lastPWMOutput);
        
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
        
        // Setup UI update timer
        _uiUpdateTimer = new DispatcherTimer();
        _uiUpdateTimer.Interval = TimeSpan.FromMilliseconds(200); // Update UI every 200ms
        _uiUpdateTimer.Tick += UiUpdateTimer_Tick;
        _uiUpdateTimer.Start();
    }

    public void End(PluginManager pluginManager)
    {
        if (_uiUpdateTimer != null)
        {
            _uiUpdateTimer.Stop();
            _uiUpdateTimer = null;
        }
    }    private void UiUpdateTimer_Tick(object sender, EventArgs e)
    {
        // CheckArduinoConnection(); // This will be handled by DataUpdate primarily
        ReadArduinoData();
        if (_settingsControl != null)
        {
            _settingsControl.UpdateUI();
        }
    }
    #endregion

    #region IDataPlugin Implementation
    public void DataUpdate(PluginManager pluginManager, ref GameData data)
    {
        _lastUpdate = DateTime.Now;

        // Arduino Connection Detection via Log Parsing
        string currentLogMessage = pluginManager.GetPropertyValue("DataCorePlugin.LoggingLastMessage") as string;

        if (!string.IsNullOrEmpty(currentLogMessage) && currentLogMessage != _lastProcessedLogMessage)
        {
            bool previousConnectionStatus = _arduinoConnected;

            if (currentLogMessage.Contains("Found one device on COM") && currentLogMessage.Contains(DEVICE_ID) && currentLogMessage.Contains(DEVICE_NAME))
            {
                _arduinoConnected = true;
            }
            else if (currentLogMessage.Contains("Connected to device on COM") && currentLogMessage.Contains(DEVICE_NAME))
            {
                // This handles reconnection scenarios where the "Found one device" message might not appear again
                _arduinoConnected = true;
            }
            else if (currentLogMessage.Contains("Arduino performance report for") && currentLogMessage.Contains(DEVICE_NAME))
            {
                // This message often indicates a disconnect or that the device is no longer actively communicating.
                // SimHub sends this when it stops receiving data or the port is closed.
                _arduinoConnected = false;
            }
            // Add more specific disconnect patterns if available, e.g., "Device disconnected", "COM port closed"
            // else if (currentLogMessage.Contains("Disconnected from device") && currentLogMessage.Contains(DEVICE_NAME))
            // {
            //     _arduinoConnected = false;
            // }            _lastProcessedLogMessage = currentLogMessage;

            // Note: Connection status changes are tracked but not logged to avoid compilation issues
            // The status is available via the ArduinoConnected property and in the Diagnostics tab
        }
        
        // If still connected, or connection status just changed, try reading data
        if (_arduinoConnected)
        {
            ReadArduinoData(); // Ensure data is read if connected
        }
        else
        {
            // Clear stale data if disconnected
            _lastClutchA = 0;
            _lastClutchB = 0;
            _lastPWMOutput = 0;
            _lastArduinoData = "N/A (Disconnected)";
        }
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
    private void CheckArduinoConnection()
    {
        // This method's original logic is now superseded by the log parsing in DataUpdate.
        // Kept for potential future use or if a different non-log-based check is needed.
        // For now, it does nothing to avoid conflict.
        // SimHub.Logging.Current.Info("[F1WheelHardwareConfig] CheckArduinoConnection called (currently inactive due to log-based detection).");
    }

    private void ReadArduinoData()
    {
        if (!_arduinoConnected)
        {
            // Clear stale data when disconnected
            _lastClutchA = 0;
            _lastClutchB = 0;
            _lastPWMOutput = 0;
            _lastArduinoData = "N/A (Disconnected)";
            return;
        }

        try
        {
            // Try to read data using device-specific paths first, then fallback to generic
            var clutchAData = PluginManager.GetPropertyValue(string.Format("DataCorePlugin.ExternalScript.{0}.ClutchA", DEVICE_ID)) ??
                              PluginManager.GetPropertyValue("DataCorePlugin.ExternalScript.Arduino.ClutchA");
            if (clutchAData != null)
            {
                _lastClutchA = Convert.ToInt32(clutchAData);
            }            var clutchBData = PluginManager.GetPropertyValue(string.Format("DataCorePlugin.ExternalScript.{0}.ClutchB", DEVICE_ID)) ??
                              PluginManager.GetPropertyValue("DataCorePlugin.ExternalScript.Arduino.ClutchB");
            if (clutchBData != null)
            {
                _lastClutchB = Convert.ToInt32(clutchBData);
            }

            var pwmData = PluginManager.GetPropertyValue(string.Format("DataCorePlugin.ExternalScript.{0}.PWMOutput", DEVICE_ID)) ??
                          PluginManager.GetPropertyValue("DataCorePlugin.ExternalScript.Arduino.PWMOutput");
            if (pwmData != null)
            {
                _lastPWMOutput = Convert.ToInt32(pwmData);
            }

            _lastArduinoData = string.Format("A:{0} B:{1} PWM:{2}", 
                _lastClutchA, _lastClutchB, _lastPWMOutput);
        }
        catch
        {
            _lastArduinoData = "Error reading data";
        }
    }

    private void SendToArduino(string command, string value)
    {
        if (!_arduinoConnected) return;
        
        try
        {
            string message = string.Format("{0}:{1}", command, value);
            // Send through SimHub's Arduino system
            this.AttachDelegate("Arduino.Command", () => message);        }
        catch
        {
            // Error sending - ignore silently
        }
    }
    #endregion

    #region Wheel Configuration Methods
    public void SetClutchBitePoint(double value)
    {
        ClutchBitePoint = value; // This will automatically send to Arduino
    }    public void ResetClutchSettings()
    {
        // Only allow reset when adjustment mode is enabled
        if (!_clutchAdjustmentMode) return;
        
        ClutchBitePoint = 50.0;
        SendToArduino("CLUTCH_RESET", "1");
        // Update UI if available (same as working UI reset button)
        if (_settingsControl != null)
        {
            _settingsControl.UpdateUI();
        }
    }// Button-assignable methods for SimHub
    public void IncreaseBitePoint()
    {
        // Only allow adjustment when adjustment mode is enabled
        if (!_clutchAdjustmentMode) return;
        
        // Use the same method as the working UI slider
        double newValue = Math.Min(90.0, _clutchBitePoint + 0.5);
        SetClutchBitePoint(newValue);
        // Update UI if available
        if (_settingsControl != null)
        {
            _settingsControl.UpdateUI();
        }
    }

    public void DecreaseBitePoint()
    {
        // Only allow adjustment when adjustment mode is enabled
        if (!_clutchAdjustmentMode) return;
        
        // Use the same method as the working UI slider  
        double newValue = Math.Max(10.0, _clutchBitePoint - 0.5);
        SetClutchBitePoint(newValue);
        // Update UI if available
        if (_settingsControl != null)
        {
            _settingsControl.UpdateUI();
        }
    }

    public void ToggleAdjustmentMode()
    {
        // Use the same method as the working UI checkbox
        ClutchAdjustmentMode = !_clutchAdjustmentMode;
        // Update UI if available
        if (_settingsControl != null)
        {
            _settingsControl.UpdateUI();
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
        
        // Wheel Settings Tab (Future expansion)
        var wheelTabItem = new TabItem();
        wheelTabItem.Header = "Wheel Settings";
        _wheelTab = new WheelConfigTab(_plugin);
        wheelTabItem.Content = _wheelTab;
        _tabControl.Items.Add(wheelTabItem);
        
        // Button Mapping Tab (Future expansion)
        var buttonTabItem = new TabItem();
        buttonTabItem.Header = "Button Mapping";
        _buttonTab = new ButtonMappingTab(_plugin);
        buttonTabItem.Content = _buttonTab;
        _tabControl.Items.Add(buttonTabItem);
        
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

        this.Content = mainPanel;
    }

    public void UpdateUI()
    {
        var settings = _plugin.GetCurrentSettings();        var debugInfo = string.Format(
            "=== F1 Wheel Hardware Status ===\n" +
            "Time: {0}\n\n" +
            "Device ID: {1}\n" +
            "Device Name: {2}\n\n" +            "Arduino Connection: {3}\n" +
            "Clutch Adjustment Mode: {4}\n" +
            "Clutch Bite Point: {5:F1}%\n\n" +
            "Live Data: {6}\n" +
            "Last Update: {7}\n\n" +
            "Last Processed SimHub Log Message for Connection:\n{8}",
            DateTime.Now.ToString("HH:mm:ss"),
            "f35eabd7-6b75-4e14-812d-6c88668e76fb",
            "Redbull RB19 Steering Interface Pre-Processor",
            settings["ArduinoConnected"],
            (bool)settings["ClutchAdjustmentMode"] ? "Active" : "Inactive",
            settings["ClutchBitePoint"],
            _plugin.LastArduinoData,
            settings["LastUpdate"],
            _plugin.LastProcessedLogMessage // Display the last processed log message
        );

        _debugText.Text = debugInfo;
    }
}
#endregion
