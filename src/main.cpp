#include <Arduino.h>
// J revision sketch
#define VERSION 'j'

#define INCLUDE_WS2812B //{"Name":"INCLUDE_WS2812B","Type":"autodefine","Condition":"[WS2812B_RGBLEDCOUNT]>0"}

#define INCLUDE_ENCODERS //{"Name":"INCLUDE_ENCODERS","Type":"autodefine","Condition":"[ENABLED_ENCODERS_COUNT]>0","IsInput":true}
#define INCLUDE_BUTTONS	 //{"Name":"INCLUDE_BUTTONS","Type":"autodefine","Condition":"[ENABLED_BUTTONS_COUNT]>0","IsInput":true}

#include <avr/pgmspace.h>

#include <EEPROM.h>
#include <SPI.h>
#include "Arduino.h"
#include <avr/pgmspace.h>
#include <Wire.h>
// #include "Adafruit_GFX.h"
#include "FlowSerialRead.h"
#include "setPwmFrequency.h"
#include "SHButton.h"

#include <hardwareSettings.h>

#include "SHCustomProtocol.h"
SHCustomProtocol shCustomProtocol;
#include "SHCommands.h"
#include "SHCommandsGlcd.h"
unsigned long lastMatrixRefresh = 0;

void idle(bool critical)
{

#ifdef INCLUDE_ENCODERS
	for (int i = 0; i < ENABLED_ENCODERS_COUNT; i++)
	{
		SHRotaryEncoders[i]->read();
	}
#endif

	if (ButtonsDebouncer.Debounce())
	{
		bool changed = false;
#ifdef INCLUDE_BUTTONS
		for (int btnIdx = 0; btnIdx < ENABLED_BUTTONS_COUNT; btnIdx++)
		{
			BUTTONS[btnIdx]->read();
		}
#endif

		shCustomProtocol.idle();
	}
}

#ifdef INCLUDE_ENCODERS
void EncoderPositionChanged(int encoderId, int position, byte direction)
{
#ifdef INCLUDE_GAMEPAD
	UpdateGamepadEncodersState(true);
#else
	if (direction < 2)
	{
		arqserial.CustomPacketStart(0x01, 3);
		arqserial.CustomPacketSendByte(encoderId);
		arqserial.CustomPacketSendByte(direction);
		arqserial.CustomPacketSendByte(position);
		arqserial.CustomPacketEnd();
	}
	else
	{
		arqserial.CustomPacketStart(0x02, 2);
		arqserial.CustomPacketSendByte(encoderId);
		arqserial.CustomPacketSendByte(direction - 2);
		arqserial.CustomPacketEnd();
	}
#endif
}
#endif

void buttonStatusChanged(int buttonId, byte Status)
{
#ifdef INCLUDE_GAMEPAD
	Joystick.setButton(TM1638_ENABLEDMODULES * 8 + buttonId - 1, Status);
	Joystick.sendState();
#else
	arqserial.CustomPacketStart(0x03, 2);
	arqserial.CustomPacketSendByte(buttonId);
	arqserial.CustomPacketSendByte(Status);
	arqserial.CustomPacketEnd();
#endif
}

void setup()
{

	FlowSerialBegin(19200);

#ifdef INCLUDE_WS2812B

#if (WS2812B_USEADAFRUITLIBRARY == 0)
#include "SHRGBLedsNeoPixelFastLed.h"
	shRGBLedsWS2812B.begin(WS2812B_RGBLEDCOUNT, WS2812B_RIGHTTOLEFT, WS2812B_TESTMODE);
#else
#include "SHRGBLedsNeoPixel.h"
	shRGBLedsWS2812B.begin(&WS2812B_strip, WS2812B_RGBLEDCOUNT, WS2812B_RIGHTTOLEFT, WS2812B_TESTMODE);
#endif
#endif

#ifdef INCLUDE_BUTTONS
	// EXTERNAL BUTTONS INIT
	for (int btnIdx = 0; btnIdx < ENABLED_BUTTONS_COUNT; btnIdx++)
	{
		BUTTONS[btnIdx]->begin(btnIdx + 1, BUTTON_PINS[btnIdx], buttonStatusChanged, BUTTON_WIRING_MODES[btnIdx], BUTTON_LOGIC_MODES[btnIdx]);
	}
#endif

#ifdef INCLUDE_ENCODERS
	void InitEncoders(); // Forward declaration
	InitEncoders();
#endif

	shCustomProtocol.setup();
	arqserial.setIdleFunction(idle);
}

#ifdef INCLUDE_ENCODERS
void InitEncoders()
{
	if (ENABLED_ENCODERS_COUNT > 0)
		encoder1.begin(ENCODER1_CLK_PIN, ENCODER1_DT_PIN, ENCODER1_BUTTON_PIN, ENCODER1_REVERSE_DIRECTION, ENCODER1_ENABLE_PULLUP, 1, ENCODER1_ENABLE_HALFSTEPS, EncoderPositionChanged);
	if (ENABLED_ENCODERS_COUNT > 1)
		encoder2.begin(ENCODER2_CLK_PIN, ENCODER2_DT_PIN, ENCODER2_BUTTON_PIN, ENCODER2_REVERSE_DIRECTION, ENCODER2_ENABLE_PULLUP, 2, ENCODER2_ENABLE_HALFSTEPS, EncoderPositionChanged);
	if (ENABLED_ENCODERS_COUNT > 2)
		encoder3.begin(ENCODER3_CLK_PIN, ENCODER3_DT_PIN, ENCODER3_BUTTON_PIN, ENCODER3_REVERSE_DIRECTION, ENCODER3_ENABLE_PULLUP, 3, ENCODER3_ENABLE_HALFSTEPS, EncoderPositionChanged);
	if (ENABLED_ENCODERS_COUNT > 3)
		encoder4.begin(ENCODER4_CLK_PIN, ENCODER4_DT_PIN, ENCODER4_BUTTON_PIN, ENCODER4_REVERSE_DIRECTION, ENCODER4_ENABLE_PULLUP, 4, ENCODER4_ENABLE_HALFSTEPS, EncoderPositionChanged);
	if (ENABLED_ENCODERS_COUNT > 4)
		encoder5.begin(ENCODER5_CLK_PIN, ENCODER5_DT_PIN, ENCODER5_BUTTON_PIN, ENCODER5_REVERSE_DIRECTION, ENCODER5_ENABLE_PULLUP, 5, ENCODER5_ENABLE_HALFSTEPS, EncoderPositionChanged);
	if (ENABLED_ENCODERS_COUNT > 5)
		encoder6.begin(ENCODER6_CLK_PIN, ENCODER6_DT_PIN, ENCODER6_BUTTON_PIN, ENCODER6_REVERSE_DIRECTION, ENCODER6_ENABLE_PULLUP, 6, ENCODER6_ENABLE_HALFSTEPS, EncoderPositionChanged);
	if (ENABLED_ENCODERS_COUNT > 6)
		encoder7.begin(ENCODER7_CLK_PIN, ENCODER7_DT_PIN, ENCODER7_BUTTON_PIN, ENCODER7_REVERSE_DIRECTION, ENCODER7_ENABLE_PULLUP, 7, ENCODER7_ENABLE_HALFSTEPS, EncoderPositionChanged);
	if (ENABLED_ENCODERS_COUNT > 7)
		encoder8.begin(ENCODER8_CLK_PIN, ENCODER8_DT_PIN, ENCODER8_BUTTON_PIN, ENCODER8_REVERSE_DIRECTION, ENCODER8_ENABLE_PULLUP, 8, ENCODER8_ENABLE_HALFSTEPS, EncoderPositionChanged);
}
#endif

char loop_opt;
unsigned long lastSerialActivity = 0;

void loop()
{

	shCustomProtocol.loop();

	// Wait for data
	if (FlowSerialAvailable() > 0)
	{
		if (FlowSerialTimedRead() == MESSAGE_HEADER)
		{
			lastSerialActivity = millis();
			// Read command
			loop_opt = FlowSerialTimedRead();

			if (loop_opt == '1')
				Command_Hello();
			else if (loop_opt == '8')
				Command_SetBaudrate();
			else if (loop_opt == 'J')
				Command_ButtonsCount();
			else if (loop_opt == '2')
				Command_TM1638Count();
			else if (loop_opt == 'B')
				Command_SimpleModulesCount();
			else if (loop_opt == 'A')
				Command_Acq();
			else if (loop_opt == 'N')
				Command_DeviceName();
			else if (loop_opt == 'I')
				Command_UniqueId();
			else if (loop_opt == '0')
				Command_Features();
			else if (loop_opt == '3')
				Command_TM1638Data();
			else if (loop_opt == 'V')
				Command_Motors();
			else if (loop_opt == 'S')
				Command_7SegmentsData();
			else if (loop_opt == '4')
				Command_RGBLEDSCount();
			else if (loop_opt == '6')
				Command_RGBLEDSData();
			else if (loop_opt == 'R')
				Command_RGBMatrixData();
			else if (loop_opt == 'M')
				Command_MatrixData();
			else if (loop_opt == 'G')
				Command_GearData();
			else if (loop_opt == 'L')
				Command_I2CLCDData();
			else if (loop_opt == 'K')
				Command_GLCDData(); // Nokia | OLEDS
			else if (loop_opt == 'P')
				Command_CustomProtocolData();
			else if (loop_opt == 'X')
			{
				String xaction = FlowSerialReadStringUntil(' ', '\n');
				if (xaction == F("list"))
					Command_ExpandedCommandsList();
				else if (xaction == F("mcutype"))
					Command_MCUType();
				else if (xaction == F("tach"))
					Command_TachData();
				else if (xaction == F("speedo"))
					Command_SpeedoData();
				else if (xaction == F("boost"))
					Command_BoostData();
				else if (xaction == F("temp"))
					Command_TempData();
				else if (xaction == F("fuel"))
					Command_FuelData();
				else if (xaction == F("cons"))
					Command_ConsData();
				else if (xaction == F("encoderscount"))
					Command_EncodersCount();
			}
		}
	}

	if (millis() - lastSerialActivity > 5000)
	{
		Command_Shutdown();
	}
}