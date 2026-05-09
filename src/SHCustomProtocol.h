#ifndef __SHCUSTOMPROTOCOL_H__
#define __SHCUSTOMPROTOCOL_H__

#include <Arduino.h>

class SHCustomProtocol
{
private:
	void (*clutchUpdateCallback)(uint16_t) = nullptr;
	void (*calibrationCallback)(uint16_t, uint16_t, uint16_t, uint16_t) = nullptr;

	// Extract the integer value after a key like "RA:" up to the next ';' or end of string.
	static uint16_t extractUInt(const String &s, int start)
	{
		int end = s.indexOf(';', start);
		String token = (end > 0) ? s.substring(start, end) : s.substring(start);
		return (uint16_t)token.toInt();
	}

	double clutchBitePoint = 50.0;
	bool clutchAdjustMode = false;
	uint16_t clutchAValue = 0;
	uint16_t clutchBValue = 0;
	uint8_t simhubPositions[3] = {8, 9, 10};
	uint16_t lastCalculatedPWM = 0; // Restored: stores last computed 10-bit PWM
	uint16_t rotaryPosition = 0;  // ROT1 switch position (1-12)
	uint8_t  rotary2Position = 0; // ROT2 switch position (1-12)
	uint8_t  rotary3Position = 0; // ROT3 switch position (1-12)
	uint8_t  rotary4Position = 0; // ROT4 switch position (1-12)
	bool rotaryPositionSent = false;
	unsigned long lastTelemetryTime = 0;
	const unsigned long TELEMETRY_INTERVAL = 100;

	unsigned long _lastReadMs = 0;     // for reconnect detection in read()
	unsigned long lastHeartbeatTime = 0; // for 5-second periodic ROT1 in idle()

public:
	const uint8_t* getSimHubPositions() const { return simhubPositions; }

	// Setters — called from main.cpp idle() on each position read
	void setRotaryPosition(uint8_t pos)  { rotaryPosition  = pos; }
	void setRotary2Position(uint8_t pos) { rotary2Position = pos; }
	void setRotary3Position(uint8_t pos) { rotary3Position = pos; }
	void setRotary4Position(uint8_t pos) { rotary4Position = pos; }

	// Send rotary positions to SimHub — called on boot/reconnect and on position change.
	// Does NOT stream continuously.
	void sendRotaryPosition()
	{
		FlowSerialDebugPrintLn("ROT1:" + String(rotaryPosition));
		rotaryPositionSent = true;
	}
	void sendRotary2Position() { FlowSerialDebugPrintLn("ROT2:" + String(rotary2Position)); }
	void sendRotary3Position() { FlowSerialDebugPrintLn("ROT3:" + String(rotary3Position)); }
	void sendRotary4Position() { FlowSerialDebugPrintLn("ROT4:" + String(rotary4Position)); }
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

	void setCalibrationCallback(void (*callback)(uint16_t, uint16_t, uint16_t, uint16_t))
	{
		calibrationCallback = callback;
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
	// Extended format (when SimHub custom protocol expression includes cal fields):
	//   BP:14.5;MODE:0;RA:608;FA:950;RB:610;FB:940
	// The RA/FA/RB/FB fields are optional and backward-compatible.
	void read()
	{
		// Send ROT1 immediately on first read() (_lastReadMs == 0) or after a 3s gap.
		// First read() == SimHub just finished handshake and is ready to receive.
		// Gap > 3s == SimHub restarted mid-session.
		// This is more reliable than any flag/idle() mechanism because SimHub is
		// guaranteed to be listening at the exact moment read() is called.
		unsigned long now = millis();
		if (_lastReadMs == 0 || (now - _lastReadMs) > 3000)
		{
			// First read after boot or reconnect — send all rotary positions immediately.
			// SimHub is guaranteed to be listening at this exact moment.
			FlowSerialDebugPrintLn("ROT1:" + String(rotaryPosition));
			FlowSerialDebugPrintLn("ROT2:" + String(rotary2Position));
			FlowSerialDebugPrintLn("ROT3:" + String(rotary3Position));
			FlowSerialDebugPrintLn("ROT4:" + String(rotary4Position));
		}
		_lastReadMs = now;

		// Read the entire message in one shot for clean token isolation.
		String msg = FlowSerialReadStringUntil('\n');

		// Handle explicit rotary request from host (e.g., plugin asks for current position)
		if (msg.indexOf("REQROT") >= 0 || msg.indexOf("GETROT") >= 0 || msg.indexOf("REQ_ROT") >= 0)
		{
			String resp = "ROT1:" + String(rotaryPosition);
			FlowSerialDebugPrintLn(resp);
			rotaryPositionSent = true;
			// continue processing the message if it contains other tokens
		}

		// --- Bite Point ---
		int bpIdx = msg.indexOf("BP:");
		if (bpIdx >= 0)
		{
			int end = msg.indexOf(';', bpIdx);
			String val = (end > 0) ? msg.substring(bpIdx + 3, end) : msg.substring(bpIdx + 3);
			double d = val.toDouble();
			if (d >= 0.0 && d <= 100.0)
				clutchBitePoint = d;
		}

		// --- Adjust Mode ---
		int modeIdx = msg.indexOf("MODE:");
		if (modeIdx >= 0)
			clutchAdjustMode = (msg.charAt(modeIdx + 5) == '1');

		// --- Optional runtime calibration (requires SimHub device custom protocol
		//     expression to include RA/FA/RB/FB fields — see Architecture.md) ---
		if (calibrationCallback != nullptr)
		{
			int raIdx = msg.indexOf("RA:");
			int faIdx = msg.indexOf("FA:");
			int rbIdx = msg.indexOf("RB:");
			int fbIdx = msg.indexOf("FB:");
			if (raIdx >= 0 && faIdx >= 0 && rbIdx >= 0 && fbIdx >= 0)
			{
				calibrationCallback(
						extractUInt(msg, raIdx + 3),
						extractUInt(msg, faIdx + 3),
						extractUInt(msg, rbIdx + 3),
						extractUInt(msg, fbIdx + 3));
			}
		}

		// --- Configurable SimHub rotary positions (SHP:p1,p2,p3) ---
		int shpIdx = msg.indexOf("SHP:");
		if (shpIdx >= 0)
		{
			String raw = msg.substring(shpIdx + 4);
			int c1 = raw.indexOf(',');
			int c2 = (c1 >= 0) ? raw.indexOf(',', c1 + 1) : -1;
			if (c1 > 0 && c2 > c1)
			{
				int end = raw.indexOf(';', c2 + 1);
				uint8_t p1 = (uint8_t)raw.substring(0, c1).toInt();
				uint8_t p2 = (uint8_t)raw.substring(c1 + 1, c2).toInt();
				uint8_t p3 = (uint8_t)(end > 0 ? raw.substring(c2 + 1, end) : raw.substring(c2 + 1)).toInt();
				if (p1 >= 1 && p1 <= 12 && p2 >= 1 && p2 <= 12 && p3 >= 1 && p3 <= 12
				    && p1 != p2 && p1 != p3 && p2 != p3)
				{
					simhubPositions[0] = p1;
					simhubPositions[1] = p2;
					simhubPositions[2] = p3;
				}
			}
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
		unsigned long now = millis();

		// CLT telemetry — only when clutch adjust mode is active
		if (clutchAdjustMode && (now - lastTelemetryTime >= TELEMETRY_INTERVAL))
		{
			lastTelemetryTime = now;
			FlowSerialDebugPrintLn("CLT:A:" + String(clutchAValue) + ";B:" + String(clutchBValue));
		}

		// Heartbeat: resend all rotary positions every 5 seconds.
		// Keeps the plugin's connection-alive timer fresh and delivers current
		// positions in case the read()-triggered send was missed.
		if (now - lastHeartbeatTime >= 5000)
		{
			lastHeartbeatTime = now;
			FlowSerialDebugPrintLn("ROT1:" + String(rotaryPosition));
			FlowSerialDebugPrintLn("ROT2:" + String(rotary2Position));
			FlowSerialDebugPrintLn("ROT3:" + String(rotary3Position));
			FlowSerialDebugPrintLn("ROT4:" + String(rotary4Position));
		}
	}
};

#endif