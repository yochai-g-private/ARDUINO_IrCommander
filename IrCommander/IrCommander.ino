/*
 Name:		IrCommander.ino
 Created:	12/22/2019 1:25:46 PM
 Author:	MCP
*/

#include <IRremote.h>

#include "PushButton.h"
#include "Observer.h"
#include "EepromIO.h"
#include "MicroController.h"
#include "RedGreenLed.h"
#include "DipSwitch.h"
#include "Toggler.h"

using namespace NYG;

//-----------------------------------------------------
//	Pin numbers
//-----------------------------------------------------

enum PIN_NUMBERS
{
	// https://learn.sparkfun.com/tutorials/pro-micro--fio-v3-hookup-guide/hardware-overview-pro-micro
	IrReceiver_PIN		= D2,
	IrEmitter_PIN		= D3,
	RedLed_PIN			= D5,
	GreenLed_PIN		= D6,
	DipSwitch_PIN_1		= D8,
	DipSwitch_PIN_2		= D9,
	Buzzer_PIN			= D10,
};

//-----------------------------------------------------
//	Raw I/O elements
//-----------------------------------------------------

static Pin				DipSwitchPins[] = { DipSwitch_PIN_1, DipSwitch_PIN_2 };
static DipSwitch<2>		ModeSelector(DipSwitchPins);

static IRrecv			IrReceiver(IrReceiver_PIN);
static IRsend			IrEmitter(IrEmitter_PIN);

static RedGreenLed		Led(RedLed_PIN, GreenLed_PIN);
static RedGreenLed::GR	GR(Led);
static Toggler			LedToggler;

static DigitalOutputPin	Buzzer(Buzzer_PIN);

//-----------------------------------------------------
//	Global data
//-----------------------------------------------------

enum Mode
{
	PROCESSING,
	LOOPING
};

static Mode st_mode = PROCESSING;

enum SwitchMode
{
	SW_MODE_PLAY			= 0,
	SW_MODE_RECORD			= 1,
	SW_MODE_PLAY_WITH_DELAY = 2,
	SW_MODE_CONFIGURE		= 3,
};

static SwitchMode	st_switch_mode;

//-----------------------------------------------------
//	PREDECLARATIONS
//-----------------------------------------------------

static void Record();
static void Play();
static void Configure();
static void Loop();
static void StartLooping();

//-----------------------------------------------------
//	SETUP
//-----------------------------------------------------

void setup()	{	}
static void signal_before_playing();
static void long_beep();

void initialize()

{
	static bool done = false;

	if (done)
		return;

	done = true;

	delay(1000);

	LOGGER << "Initializing..." << NL;

	bool corrent_mode;

	st_switch_mode = (SwitchMode)ModeSelector.Get();

	switch (st_switch_mode)
	{
		case SW_MODE_RECORD:
		{
			LOGGER << "Recording..." << NL;
			
			long_beep();
			Led.SetRed();

			IrReceiver.enableIRIn();

			break;
		}

		case SW_MODE_PLAY:
		case SW_MODE_PLAY_WITH_DELAY:
		{
			signal_before_playing();

			long_beep();
			Led.SetGreen();

			break;
		}

		case SW_MODE_CONFIGURE:
		{
			LOGGER << "Configuring..." << NL;
			LedToggler.StartOnOff(GR, 1, SECS);
			break;
		}
	}
}

static void check_mode_changed()
{
	if (st_switch_mode != (SwitchMode)ModeSelector.Get())
	{
		int delay_seconds = 5;

		LOGGER << "Mode changed. Restarting in " << delay_seconds << " seconds" << NL;
		delay(delay_seconds * 1000);
		MicroController::Restart();
	}
}

static void long_beep()
{
	static Timer			BuzzerEndTimer;

	BuzzerEndTimer.StartOnce(5, SECS);
	LedToggler.StartOnOff(GR, 200, MILLIS);

	Buzzer.On();

	for (;;)
	{
		delay(10);
		check_mode_changed();

		if (BuzzerEndTimer.Test())
			break;

		LedToggler.Toggle();
	}

	Buzzer.Off();
	LedToggler.Stop();
	Led.SetOff();
	delay(1000);
}

static void signal_before_playing()
{
	if (st_switch_mode == SW_MODE_PLAY_WITH_DELAY)
	{
		LOGGER << "Playing with delay..." << NL;

		class BeepAndBlink : public IDigitalOutput
		{
			void Set(bool value)
			{
				Buzzer.Set(value);
				Led.GetGreen().Set(value);
			}

			bool Get()	const
			{
				return Buzzer.Get();
			}
		};

		BeepAndBlink BaB;
		LedToggler.Start(BaB, Toggler::OnTotal(200, 1000), MILLIS, 30);
	
		do
		{
			delay(10);
			check_mode_changed();
			LedToggler.Toggle();
		} while (LedToggler.IsStarted());
	}
	else
	{
		LOGGER << "Playing..." << NL;
	}
}

//-----------------------------------------------------
//	LOOP
//-----------------------------------------------------
void loop() 
{
	initialize();

	LedToggler.Toggle();

	check_mode_changed();

	if (LOOPING == st_mode)
	{
		Loop();
		return;
	}

	switch (st_switch_mode)
	{
		case SW_MODE_RECORD:			Record();		
										break;
		case SW_MODE_PLAY:				
		case SW_MODE_PLAY_WITH_DELAY:	Play();			
										break;
		case SW_MODE_CONFIGURE:			Configure();	
										break;
	}
}

//-----------------------------------------------------
//	SIGNATURE
//-----------------------------------------------------

char SIGNATURE[] = "IRR_v0";

struct Signature
{
	char text[sizeof(SIGNATURE)];
};

//-----------------------------------------------------
//	RAW DATA
//-----------------------------------------------------

typedef int8_t			DecodeType;
typedef unsigned long	SilenceMillis;
typedef int				CodeLen;
typedef unsigned int	RawCode;

struct RawData
{
	DecodeType		decodeType;
	SilenceMillis	silenceMillis;
	CodeLen			codeLen;
	RawCode			rawCodes[RAWBUF];
};

//-----------------------------------------------------
//	RECORD
//-----------------------------------------------------

static EepromOutput		st_EepromWriter;

static unsigned long	st_last_raw_data_millis = 0;

static bool checkRoom(uint32_t required_size);
static bool storeCode(const decode_results& results);
static void writeTerminator();

static void Record()
{
	decode_results	results;
	static int count = 0;

	if (!IrReceiver.decode(&results))
		return;

	LOGGER << "Decoded" << NL;

	if (results.value != 0xFFFFFFFF)
	{
		Led.Toggle();

		if (!st_last_raw_data_millis)
		{
			LOGGER << "Writing the signature" << NL;
			Signature signature;
			memcpy(signature.text, SIGNATURE, sizeof(SIGNATURE));

			st_EepromWriter << signature;

			writeTerminator();
		}

		uint32_t position = st_EepromWriter.GetPosition();

		bool written = storeCode(results);

		if (written)
		{
			count++;
			LOGGER << "Record #" << count << " stored" << NL;
		}
		else
		{
			LOGGER << "Store failed!" << NL;
			st_EepromWriter.SetPosition(position);
			writeTerminator();
		}

		delay(100);
		Led.Toggle();
	}

	IrReceiver.resume();

	checkRoom(0);
}

static bool store_KNOWN(const decode_results& results);
static bool store_UNKNOWN(const decode_results& results);

static bool storeCode(const decode_results& results)
{
	DecodeType	    decodeType;
	SilenceMillis	silenceMillis;

	if (!checkRoom(sizeof(decodeType) + sizeof(silenceMillis)))
	{
		return false;
	}

	unsigned long now_millis = millis();

	if (st_last_raw_data_millis)
	{
		silenceMillis = now_millis - st_last_raw_data_millis;
	}
	else
	{
		silenceMillis = 0;
	}

	st_last_raw_data_millis = now_millis;

	bool stored;
	// C:\Users\yochai.glauber\Documents\Arduino\library_jungle\IRremote\examples\IRrecord\IRrecord.ino
	// In the IRrecord.ino example are treated only NEC, JVC, SONY and PANASONIC

	switch (results.decode_type)
	{
		#define DECODE_TYPES_LIST(DO_WITH)		\
					DO_WITH(JVC)				\
					DO_WITH(NEC)				\
					DO_WITH(PANASONIC)			\
					DO_WITH(SONY)				\
					/* the next may not work...*/\
					DO_WITH(LG)					\
					DO_WITH(SAMSUNG)			\


		#define DO_CASE(DECODE_TYPE)	case DECODE_TYPE:

		DECODE_TYPES_LIST(DO_CASE)
		{
			decodeType = results.decode_type;
			st_EepromWriter << decodeType << silenceMillis;

			stored = store_KNOWN(results);

			break;
		}
		#undef DO_CASE

		//case RC5:
		//case RC6:
		default:
		{
			decodeType = UNKNOWN;
			st_EepromWriter << decodeType << silenceMillis;

			stored = store_UNKNOWN(results);

			break;
		}
	}
	
	if(stored)
		writeTerminator();

	return stored;
}

static bool checkRoom(uint32_t required_size)
{
	uint32_t available_size = st_EepromWriter.GetRemainder();

	if (required_size <= available_size)
		return true;

	LOGGER << "No more room in EEPROM" << NL;

	for (int cnt = 0; cnt < 20; cnt++)
	{
		check_mode_changed();

		Led.Toggle();
		delay(250);
	}

	st_EepromWriter.SetPositionToEnd();

	StartLooping();

	return false;
}

static bool store_KNOWN(const decode_results& results)
{
	if (!checkRoom(sizeof(results.bits) + sizeof(results.value)))
		return false;

	st_EepromWriter << results.bits << results.value;

	return true;
}

static bool store_UNKNOWN(const decode_results& results)
{
	CodeLen			codeLen	   = results.rawlen - 1;
	RawCode			rawCode;

	uint32_t required_size = sizeof(codeLen)		+
							 (sizeof(rawCode) * codeLen);

	if (!checkRoom(required_size))
		return false;

	st_EepromWriter << codeLen;

	for (int i = 1; i <= codeLen; i++)
	{
		if (i % 2) {
			// Mark
			rawCode = results.rawbuf[i] * USECPERTICK - MARK_EXCESS;
		}
		else {
			// Space
			rawCode = results.rawbuf[i] * USECPERTICK + MARK_EXCESS;
		}

		st_EepromWriter << rawCode;
	}

	return true;
}

static void writeTerminator()
{
	DecodeType terminator = UNUSED;
	st_EepromWriter << terminator;
	st_EepromWriter.SetRelativePosition(-1 * (int32_t)sizeof(terminator));
}

//-----------------------------------------------------
//	PLAY
//-----------------------------------------------------

static void Play()
{
	check_mode_changed();

	EepromInput	st_EepromReader;

	LOGGER << "Reading the signature" << NL;

	Signature signature;
	st_EepromReader >> signature;

	if (memcmp(signature.text, SIGNATURE, sizeof(SIGNATURE)))
	{
		signature.text[sizeof(signature.text) - 1] = 0;

		LOGGER << "Invalid signature '" << signature.text << "'" << NL;

		for (int cnt = 0; cnt < 20; cnt++)
		{
			check_mode_changed();

			Led.Toggle();
			delay(250);
		}

		StartLooping();
		return;
	}

	RawData data;

	int count = 0;

	while(!st_EepromReader.IsEnd())
	{
		count++;

		check_mode_changed();

		st_EepromReader >> data.decodeType >> data.silenceMillis;

		if (data.decodeType == UNUSED)
			break;

		if (data.silenceMillis)
		{
			delay(/*(data.silenceMillis < 700) ? data.silenceMillis / 2 : */data.silenceMillis);
		}

		Led.Toggle();

		if (UNKNOWN == data.decodeType)
		{
			st_EepromReader >> data.codeLen;

			for (int i = 1; i <= data.codeLen; i++)
			{
				st_EepromReader >> data.rawCodes[i - 1];
			}

			// Assume 38 KHz
			IrEmitter.sendRaw(data.rawCodes, data.codeLen, 38);

			LOGGER << count << ". Sending RAW codes, len=" << data.codeLen << NL;
		}
		else
		{
			decode_results results;
			st_EepromReader >> results.bits >> results.value;

			switch (data.decodeType)
			{
				#define SEND_JVC			IrEmitter.sendJVC(results.value, results.bits, false)
				#define SEND_NEC			IrEmitter.sendNEC(results.value, results.bits)
				#define SEND_PANASONIC		IrEmitter.sendPanasonic(results.value, results.bits)
				#define SEND_SONY			IrEmitter.sendSony(results.value, results.bits)
				#define SEND_LG				IrEmitter.sendLG(results.value, results.bits)
				#define SEND_SAMSUNG		IrEmitter.sendSAMSUNG(results.value, results.bits)

				#define DO_CASE(DECODE_TYPE)	case DECODE_TYPE: { LOGGER << count << ". Sending " << #DECODE_TYPE << " : " << RXHEX << results.value << NL; SEND_ ## DECODE_TYPE;	break; }

				DECODE_TYPES_LIST(DO_CASE)
				#undef DO_CASE
			}
		}
		
		Led.Toggle();
	}

	LOGGER << "Play done. " << count << " signals sent" << NL;

	StartLooping();
}

//-----------------------------------------------------
//	Loop
//-----------------------------------------------------

static void StartLooping()
{
	LOGGER << "Looping..." << NL;
	st_mode = LOOPING;
	Led.SetOff();

	LedToggler.Start((st_switch_mode == SW_MODE_RECORD) ?	Led.GetRed() : Led.GetGreen(),
					 Toggler::OnTotal(1, 10),
					 SECS,
					 30);
}

static void Configure()
{
	// TODO >>>>
}

static void Loop()
{
}
