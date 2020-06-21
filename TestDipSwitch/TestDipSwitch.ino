/*
 Name:		TestDipSwitch.ino
 Created:	2/10/2020 10:34:05 AM
 Author:	MCP
*/

#include "DipSwitch.h"

using namespace NYG;

enum PIN_NUMBERS
{
	DipSwitch_PIN_1		= D8,
	DipSwitch_PIN_2		= D9,
};

static Pin				DipSwitchPins[] = { DipSwitch_PIN_1, DipSwitch_PIN_2 };
static DipSwitch<2>		ModeSelector(DipSwitchPins);

// the setup function runs once when you press reset or power the board
void setup() 
{

}

// the loop function runs over and over again until power down or reset
void loop() 
{
	LOGGER << "p1=" << (digitalRead(DipSwitch_PIN_1) == LOW) <<
			 " p2=" << (digitalRead(DipSwitch_PIN_2) == LOW) << NL;

	int mode = ModeSelector.Get();

	LOGGER << mode << NL;

	delay(5000);
}
