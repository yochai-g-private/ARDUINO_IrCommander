/*
 Name:		TestEeprom.ino
 Created:	12/24/2019 1:11:47 PM
 Author:	MCP
*/

#include "EepromIO.h"
#include "Logger.h"

using namespace NYG;

char SIGNATURE[] = "Test EEPROM";

struct Signature
{
	char text[sizeof(SIGNATURE)];
};

// the setup function runs once when you press reset or power the board
void setup() 
{
	Signature signature;
	memcpy(signature.text, SIGNATURE, sizeof(SIGNATURE));

	EepromOutput EepromWriter;
	EepromWriter << signature;

	EepromInput	EepromReader;

	memset(&signature, 0, sizeof(signature));
	EepromReader >> signature;

	LOGGER << "Read from EEPROM: " << signature.text << NL;
}

// the loop function runs over and over again until power down or reset
void loop() {
  
}
