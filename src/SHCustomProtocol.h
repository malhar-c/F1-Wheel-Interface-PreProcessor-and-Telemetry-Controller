#ifndef __SHCUSTOMPROTOCOL_H__
#define __SHCUSTOMPROTOCOL_H__

#include <Arduino.h>

class SHCustomProtocol
{
private:
	void (*clutchUpdateCallback)(uint16_t) = nullptr;
	double clutchBitePoint = 50.0;
	bool clutchAdjustMode = false;
	uint16_t clutchAValue = 0;
	uint16_t clutchBValue = 0;
	uint16_t lastCalculatedPWM = 0;
	unsigned long lastTelemetryTime = 0;
	const unsigned long TELEMETRY_INTERVAL = 100;

public:
	/*
	CUSTOM PROTOCOL CLASS - DUAL CLUTCH WITH BITE POINT
	SEE https://github.com/SHWotever/SimHub/wiki/Custom-Arduino-hardware-support

	GENERAL RULES :
		- ALWAYS BACKUP THIS FILE, reinstalling/updating SimHub would overwrite it with the default version.
		- Read data AS FAST AS POSSIBLE in the read function
		- NEVER block the arduino (using delay for instance)
		- Make sure the data read in "read()" function READS ALL THE DATA from the serial port matching the custom protocol definition
		- Idle function is called hundreds of times per second, never use it for slow code, arduino performances would fall
		- If you use library suspending interrupts make sure to use it only in the "read" function when ALL data has been read from the serial port.
			It is the only interrupt safe place

	COMMON FUNCTIONS :
		- FlowSerialReadStringUntil('\n')
			Read the incoming data up to the end (\n) won't be included
		- FlowSerialReadStringUntil(';')
			Read the incoming data up to the separator (;) separator won't be included
		- FlowSerialDebugPrintLn(string)
			Send a debug message to simhub which will display in the log panel and log file (only use it when debugging, it would slow down arduino in run conditions)

	*/

	void setClutchUpdateCallback(void (*callback)(uint16_t))
	{
		clutchUpdateCallback = callback;
	}

	double getClutchBitePoint() { return clutchBitePoint; }

	void setClutchValues(uint16_t a, uint16_t b)
	{
		clutchAValue = a;
		clutchBValue = b;
	}

	// Calculate combined PWM from dual clutch and bite point
	// Formula: PWM = (clutchA × (100 - BP)/100) + (clutchB × BP/100)
	// This means: clutchA provides the base, clutchB modulates based on BP
	uint16_t calculateCombinedPWM(uint16_t clutchA, uint16_t clutchB)
	{
		// Normalize clutch values from 0-1023 to 0-100 percent
		double clutchAPercent = (clutchA / 1023.0) * 100.0;
		double clutchBPercent = (clutchB / 1023.0) * 100.0;

		// Combined PWM calculation with bite point weighting
		// When BP is low (10%), clutchB has minimal effect, clutchA dominates
		// When BP is high (90%), clutchB has maximum effect
		double weightA = (100.0 - clutchBitePoint) / 100.0;
		double weightB = clutchBitePoint / 100.0;

		double combinedPercent = (clutchAPercent * weightA) + (clutchBPercent * weightB);

		// Convert back to 10-bit PWM range (0-1023)
		// But clamp to valid range
		uint16_t pwmValue = (uint16_t)((combinedPercent / 100.0) * 1023.0);
		pwmValue = constrain(pwmValue, 0, 1023);

		lastCalculatedPWM = pwmValue;
		return pwmValue;
	}

	// Called when starting the arduino (setup method in main sketch)
	void setup()
	{
		// BP is persisted by SimHub plugin and sent on connect - no EEPROM load needed
	}

	// Called when new data is coming from computer
	// Incoming SimHub message format: BP:14.5;MODE:0
	void read()
	{
		String bpToken = FlowSerialReadStringUntil(';');		// "BP:14.5"
		String modeToken = FlowSerialReadStringUntil('\n'); // "MODE:0"

		if (bpToken.startsWith("BP:"))
		{
			double val = bpToken.substring(3).toDouble();
			if (val >= 0.0 && val <= 100.0)
				clutchBitePoint = val;
		}

		if (modeToken.startsWith("MODE:"))
		{
			clutchAdjustMode = (modeToken.charAt(5) == '1');
		}
	}

	// Called once per arduino loop, timing can't be predicted,
	// but it's called between each command sent to the arduino
	void loop()
	{
	}

	// Called once between each byte read on arduino,
	// Streams A/B clutch values back to SimHub only when in clutch adjust mode
	// THIS IS A CRITICAL PATH :
	// AVOID ANY TIME CONSUMING ROUTINES !!!
	// PREFER READ OR LOOP METHOS AS MUCH AS POSSIBLE
	// AVOID ANY INTERRUPTS DISABLE (serial data would be lost!!!)
	void idle()
	{
		if (!clutchAdjustMode)
			return;

		unsigned long now = millis();
		if (now - lastTelemetryTime < TELEMETRY_INTERVAL)
			return;
		lastTelemetryTime = now;

		String msg = "CLT:A:" + String(clutchAValue) + ";B:" + String(clutchBValue);
		FlowSerialDebugPrintLn(msg);
	}
};

#endif