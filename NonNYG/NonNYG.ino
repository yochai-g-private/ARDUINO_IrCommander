/*
 Name:		NonNYG.ino
 Created:	12/23/2019 7:48:23 AM
 Author:	MCP
*/

// C:\Users\yochai.glauber\Documents\Arduino\library_jungle\IRremote\examples\IRrecord\IRrecord.ino

#include <IRremote.h>

enum {
	IR_EMITTER_PIN = 2,
	IR_RECEIVER_PIN = 12,
};

IRrecv irrecv(IR_RECEIVER_PIN);  // Create a class object used to receive class
IRsend irsend(IR_EMITTER_PIN);

bool learning = false;

void learn();
void play();

typedef unsigned long	SilenceMillis;
typedef int				CodeLen;
typedef unsigned int	RawCode;

struct RawData
{
	SilenceMillis	silenceMillis;
	CodeLen			codeLen;
	RawCode			rawCodes[RAWBUF];
};

// the setup function runs once when you press reset or power the board
void setup() {
	Serial.begin(9600);
	Serial.println("IrCommander started!");
	Serial.print("Sizeof IR RawData: "); Serial.println(sizeof(RawData));
}

// the loop function runs over and over again until power down or reset
void loop()
{
	if (learning)
		learn();
	else
		play();
}

//================================
// LEARN
//================================

char SIGNATURE[] = "IrCommander";

struct Signature
{
	char text[sizeof(SIGNATURE)];
};

unsigned long	last_raw_data_millis;

#include <EEPROM.h>

uint16_t		EEPROM_index;

template<class T>
void WriteToEEPROM(const T& t)
{
	//EEPROM.put(EEPROM_index, t);
	EEPROM_index += sizeof(T);
}

void WriteTerminatorToEEPROM()
{
	uint32_t end = 0;
	WriteToEEPROM(end);
	EEPROM_index -= sizeof(end);
}

void storeCode(decode_results& results, bool first)
{
	CodeLen			codeLen;
	SilenceMillis	silenceMillis;
	RawCode			rawCode;

	unsigned long now_millis = millis();

	if (first)
	{
		silenceMillis = 0;
	}
	else
	{
		silenceMillis = now_millis - last_raw_data_millis;
	}

	last_raw_data_millis = now_millis;

	codeLen = results.rawlen - 1;

	WriteToEEPROM(codeLen);
	WriteToEEPROM(silenceMillis);

	for (int i = 1; i <= codeLen; i++)
	{
		if (i % 2) {
			// Mark
			rawCode = results.rawbuf[i] * USECPERTICK - MARK_EXCESS;
			//			Serial.print(" m");
		}
		else {
			// Space
			rawCode = results.rawbuf[i] * USECPERTICK + MARK_EXCESS;
			//			Serial.print(" s");
		}

		WriteToEEPROM(rawCode);
		//		Serial.print(rawCodes[i - 1], DEC);
	}

	WriteTerminatorToEEPROM();
	//	Serial.println("");

}

void learn()
{
	irrecv.enableIRIn();

	EEPROM_index = 0;

	Signature signature;
	memcpy(signature.text, SIGNATURE, sizeof(SIGNATURE));

	WriteToEEPROM(signature);
	WriteTerminatorToEEPROM();

	//Serial.println("Learning...");

	decode_results	results;
	bool			first = true;

	if (irrecv.decode(&results))
	{
		if (results.value != 0xFFFFFFFF)
		{
			//#define PRINT_RESULT(fld, hex)  \
			//	Serial.print(#fld "=");\
			//	Serial.print(results.fld, (hex) ? HEX : DEC);\
			//	Serial.print("; ")

			//PRINT_RESULT(decode_type, false);
			//PRINT_RESULT(address, false);
			//PRINT_RESULT(value, true);
			//PRINT_RESULT(bits, false);
			//PRINT_RESULT(overflow, false);
			//Serial.println();

			//Serial.print("Storing raw #"); Serial.println(raw_data_counter + 1);

			//Serial.flush();

			storeCode(results, first);
			first = false;
		}

		irrecv.resume();
	}
}

//================================
// PLAY
//================================

template<class T>
void ReadFromEEPROM(T& t)
{
	//EEPROM.get(EEPROM_index, t);
	EEPROM_index += sizeof(T);
}

void play()
{
	Serial.println("Playing...");

	EEPROM_index = 0;

	Signature signature;
	//memcpy(signature.text, SIGNATURE, sizeof(SIGNATURE));

	ReadFromEEPROM(signature);

	if (memcmp(signature.text, SIGNATURE, sizeof(SIGNATURE)))
	{
		learning = true;
		return;
	}

	RawData data;

	for (;;)
	{
		ReadFromEEPROM(data.codeLen);
		if (!data.codeLen)
			break;

		ReadFromEEPROM(data.silenceMillis);

		for (int i = 1; i <= data.codeLen; i++)
		{
			ReadFromEEPROM(data.rawCodes[i - 1]);
		}

		if (data.silenceMillis)
		{
			//Serial.print("Delaying "); Serial.println(data.silence_millis); Serial.flush();
			delay(data.silenceMillis);
		}

		//Serial.print("Sending raw #"); Serial.println(idx + 1); Serial.flush();
		// Assume 38 KHz
		irsend.sendRaw(data.rawCodes, data.codeLen, 38);
	}

	for (;;)
		delay(1000);
}
