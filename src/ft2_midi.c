// this implements MIDI input only!

#ifdef HAS_MIDI

// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_edit.h"
#include "ft2_config.h"
#include "ft2_gui.h"
#include "ft2_midi.h"
#include "ft2_audio.h"
#include "ft2_mouse.h"
#include "ft2_pattern_ed.h"
#include "ft2_structs.h"
#include "rtmidi/rtmidi_c.h"

// hide POSIX warnings
#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

midi_t midi; // globalized

static volatile bool midiDeviceOpened;
static bool recMIDIValidChn = true;
static volatile RtMidiPtr midiInDev;

static inline void midiInSetChannel(uint8_t status)
{
	recMIDIValidChn = (config.recMIDIAllChn || (status & 0xF) == config.recMIDIChn-1);
}

enum { MIDI_QUEUE_CAPACITY = 1024 };
static uint8_t midiQueue[MIDI_QUEUE_CAPACITY][3];
static SDL_atomic_t midiRead, midiWrite, midiOverflow, midiAccepting;
static bool midiSustain[16], midiDeferredRelease[16][128];

static void resetMidiInputState(void)
{
    releaseAllMidiInputNotes();
    memset(midiSustain, 0, sizeof(midiSustain));
    memset(midiDeferredRelease, 0, sizeof(midiDeferredRelease));
    midi.currMIDIVibDepth = midi.currMIDIPitch = 0;
}

static void midiInKeyAction(uint8_t wireChannel, uint8_t wireNote, uint8_t velocity)
{
    if (!velocity) {
        if (midiSustain[wireChannel]) midiDeferredRelease[wireChannel][wireNote] = true;
        else releaseMidiInputNote(wireChannel, wireNote, true);
        return;
    }
    if (!recMIDIValidChn || ui.sysReqShown) return;
    int note = (int)wireNote - 11;
    if (config.recMIDITransp) note += config.recMIDITranspVal;
    if (note < 1 || note > 96) return;
    int volume = (velocity * 64 * config.recMIDIVolSens) / (127 * 100);
    if (volume > 64) volume = 64;
    if (volume < 1) volume = 1;
    if (!config.recMIDIVelocity) volume = -1;
    midiDeferredRelease[wireChannel][wireNote] = false;
    recordMidiInputNote(wireChannel, wireNote, note, volume);
}

static bool midiInLifecycleControl(uint8_t wireChannel, uint8_t controller, uint8_t value)
{
    if (controller == 64) {
        // Always accept pedal-up so a changed input filter cannot strand notes.
        if (value >= 64 && !recMIDIValidChn) return true;
        midiSustain[wireChannel] = value >= 64;
        if (!midiSustain[wireChannel]) {
            for (int note = 0; note < 128; ++note) {
                if (midiDeferredRelease[wireChannel][note]) releaseMidiInputNote(wireChannel, note, true);
                midiDeferredRelease[wireChannel][note] = false;
            }
        }
        return true;
    }
    if (controller == 120 || controller == 123) {
        for (int note = 0; note < 128; ++note) {
            if (controller == 123 && midiSustain[wireChannel]) midiDeferredRelease[wireChannel][note] = true;
            else {
                releaseMidiInputNote(wireChannel, note, controller == 123);
                midiDeferredRelease[wireChannel][note] = false;
            }
        }
        return true;
    }
    return false;
}

static inline void midiInControlChange(uint8_t data1, uint8_t data2)
{
	if (!recMIDIValidChn || data1 != 1) // 1 = modulation wheel
		return;

	midi.currMIDIVibDepth = data2 << 6;

	if (recMIDIValidChn) // real FT2 forgot to check this here..
	{
		for (uint8_t i = 0; i < song.numChannels; i++)
		{
			if (channel[i].midiVibDepth != 0 || editor.keyOnTab[i] != 0)
				channel[i].midiVibDepth = midi.currMIDIVibDepth;
		}
	}

	const uint8_t vibDepth = (midi.currMIDIVibDepth >> 9) & 0x0F;
	if (vibDepth > 0 && recMIDIValidChn)
		recordMIDIEffect(0x04, 0xA0 | vibDepth);
}

static inline void midiInPitchBendChange(uint8_t data1, uint8_t data2)
{
	if (!recMIDIValidChn) return;
	int16_t pitch = (int16_t)((data2 << 7) | data1) - 8192; // -8192..8191
	pitch >>= 6; // -128..127

	midi.currMIDIPitch = pitch;
	if (recMIDIValidChn)
	{
		channel_t *ch = channel;
		for (uint8_t i = 0; i < song.numChannels; i++, ch++)
		{
			if (ch->midiPitch != 0 || editor.keyOnTab[i] != 0)
				ch->midiPitch = midi.currMIDIPitch;
		}
	}
}

/* One RtMidi producer and one application-thread consumer. No allocation,
 * editor mutation, synth calls, logging, or waiting in the device callback. */
static void midiInCallback(double timeStamp, const unsigned char *message, size_t messageSize, void *userData)
{
    (void)timeStamp;
    (void)userData;
    if (!SDL_AtomicGet(&midiAccepting) || !message || messageSize != 3) return;
    const uint8_t type = message[0] & 0xF0;
    if ((type != 0x80 && type != 0x90 && type != 0xB0 && type != 0xE0) ||
        message[1] > 127 || message[2] > 127 || SDL_AtomicGet(&midiOverflow)) return;
    const int write = SDL_AtomicGet(&midiWrite);
    const int next = (write + 1) % MIDI_QUEUE_CAPACITY;
    if (next == SDL_AtomicGet(&midiRead)) {
        SDL_AtomicSet(&midiOverflow, 1);
        return;
    }
    memcpy(midiQueue[write], message, 3);
    SDL_AtomicSet(&midiWrite, next);
}

void processMidiInput(void)
{
    if (!midi.enable || SDL_AtomicGet(&midiOverflow)) {
        SDL_AtomicSet(&midiRead, SDL_AtomicGet(&midiWrite));
        resetMidiInputState();
        SDL_AtomicSet(&midiOverflow, 0);
        return;
    }
    // Bound work even if the device continually sends data.
    const int end = SDL_AtomicGet(&midiWrite);
    int read = SDL_AtomicGet(&midiRead);
    while (read != end) {
        uint8_t bytes[3];
        memcpy(bytes, midiQueue[read], 3);
        read = (read + 1) % MIDI_QUEUE_CAPACITY;
        SDL_AtomicSet(&midiRead, read);
        midiInSetChannel(bytes[0]);
        const uint8_t wireChannel = bytes[0] & 15;
        switch (bytes[0] & 0xF0) {
            case 0x80: midiInKeyAction(wireChannel, bytes[1], 0); break;
            case 0x90: midiInKeyAction(wireChannel, bytes[1], bytes[2]); break;
            case 0xB0:
                if (!midiInLifecycleControl(wireChannel, bytes[1], bytes[2]))
                    midiInControlChange(bytes[1], bytes[2]);
                break;
            case 0xE0: midiInPitchBendChange(bytes[1], bytes[2]); break;
        }
    }
}

static uint32_t getNumMidiInDevices(void)
{
	if (midiInDev == NULL)
		return 0;

	return rtmidi_get_port_count(midiInDev);
}

static char *getMidiInDeviceName(uint32_t deviceID)
{
	if (midiInDev == NULL)
		return NULL; // MIDI not initialized

	char *devStr = (char *)rtmidi_get_port_name(midiInDev, deviceID);
	if (devStr == NULL || !midiInDev->ok)
		return NULL;

	return devStr;
}

void closeMidiInDevice(void)
{
    SDL_AtomicSet(&midiAccepting, 0);
	if (midiDeviceOpened)
	{
		if (midiInDev != NULL)
		{
			rtmidi_in_cancel_callback(midiInDev);
			rtmidi_close_port(midiInDev);
		}

		midiDeviceOpened = false;
	}
    SDL_AtomicSet(&midiRead, SDL_AtomicGet(&midiWrite));
    SDL_AtomicSet(&midiOverflow, 0);
    resetMidiInputState();
}

void freeMidiIn(void)
{
    closeMidiInDevice();
	if (midiInDev != NULL)
	{
		rtmidi_in_free(midiInDev);
		midiInDev = NULL;
	}
}

bool initMidiIn(void)
{
	midiInDev = rtmidi_in_create_default();
	if (midiInDev == NULL)
		return false;

	if (!midiInDev->ok)
	{
		rtmidi_in_free(midiInDev);
		midiInDev = NULL;
		return false;
	}

	return true;
}

bool openMidiInDevice(uint32_t deviceID)
{
	if (midiDeviceOpened || midiInDev == NULL || midi.numInputDevices == 0)
		return false;

	rtmidi_open_port(midiInDev, deviceID, "FT2 Clone MIDI Port");
	if (!midiInDev->ok)
		return false;

	rtmidi_in_set_callback(midiInDev, midiInCallback, NULL);
	if (!midiInDev->ok)
	{
		rtmidi_close_port(midiInDev);
		return false;
	}

	rtmidi_in_ignore_types(midiInDev, true, true, true);

	SDL_AtomicSet(&midiAccepting, 1);
	midiDeviceOpened = true;
	return true;
}

void recordMIDIEffect(uint8_t efx, uint8_t efxData)
{
	// only handle this in record mode
	if (!midi.enable || (playMode != PLAYMODE_RECSONG && playMode != PLAYMODE_RECPATT))
		return;

	if (config.multiRec)
	{
		note_t *p = &pattern[editor.editPattern][editor.row * MAX_CHANNELS];
		for (int32_t i = 0; i < song.numChannels; i++, p++)
		{
			if (config.multiRecChn[i] && !editor.channelMuted[i])
			{
				if (!allocatePattern(editor.editPattern))
					return;

				if (p->efx == 0)
				{
					p->efx = efx;
					p->efxData = efxData;
					setSongModifiedFlag();
				}
			}
		}
	}
	else
	{
		if (!allocatePattern(editor.editPattern))
			return;

		note_t *p = &pattern[editor.editPattern][(editor.row * MAX_CHANNELS) + cursor.ch];
		if (p->efx != efx || p->efxData != efxData)
			setSongModifiedFlag();

		p->efx = efx;
		p->efxData = efxData;
	}
}

bool saveMidiInputDeviceToConfig(void)
{
	if (!midi.initThreadDone || midiInDev == NULL || !midiDeviceOpened)
		return false;

	const uint32_t numDevices = getNumMidiInDevices();
	if (numDevices == 0)
		return false;

	char *midiInStr = getMidiInDeviceName(midi.inputDevice);
	if (midiInStr == NULL)
		return false;

	FILE *f = UNICHAR_FOPEN(editor.midiConfigFileLocationU, "w");
	if (f == NULL)
	{
		free(midiInStr);
		return false;
	}

	fputs(midiInStr, f);
	free(midiInStr);

	fclose(f);
	return true;
}

bool setMidiInputDeviceFromConfig(void)
{
	uint32_t i;

	if (midiInDev == NULL || editor.midiConfigFileLocationU == NULL)
		goto setDefMidiInputDev;

	const uint32_t numDevices = getNumMidiInDevices();
	if (numDevices == 0)
		goto setDefMidiInputDev;

	FILE *f = UNICHAR_FOPEN(editor.midiConfigFileLocationU, "r");
	if (f == NULL)
		goto setDefMidiInputDev;

	char *devString = (char *)malloc(1024+2);
	if (devString == NULL)
	{
		fclose(f);
		goto setDefMidiInputDev;
	}
	devString[0] = '\0';

	if (fgets(devString, 1024, f) == NULL)
	{
		fclose(f);
		free(devString);
		goto setDefMidiInputDev;
	}

	fclose(f);

	// scan for device in list
	char *midiInStr = NULL;
	for (i = 0; i < numDevices; i++)
	{
		midiInStr = getMidiInDeviceName(i);
		if (midiInStr == NULL)
			continue;

		if (!_stricmp(devString, midiInStr))
			break; // device matched

		free(midiInStr);
		midiInStr = NULL;
	}

	free(devString);

	// device not found in list, set default
	if (i == numDevices)
		goto setDefMidiInputDev;

	if (midi.inputDeviceName != NULL)
	{
		free(midi.inputDeviceName);
		midi.inputDeviceName = NULL;
	}

	midi.inputDevice = i;
	midi.inputDeviceName = midiInStr;
	midi.numInputDevices = numDevices;

	return true;

	// couldn't load device, set default
setDefMidiInputDev:
	if (midi.inputDeviceName != NULL)
	{
		free(midi.inputDeviceName);
		midi.inputDeviceName = NULL;
	}

	midi.inputDevice = 0;
	midi.inputDeviceName = strdup("Error configuring MIDI...");
	midi.numInputDevices = 1;

	return false;
}

void freeMidiInputDeviceList(void)
{
	for (int32_t i = 0; i < MAX_MIDI_DEVICES; i++)
	{
		if (midi.inputDeviceNames[i] != NULL)
		{
			free(midi.inputDeviceNames[i]);
			midi.inputDeviceNames[i] = NULL;
		}
	}

	midi.numInputDevices = 0;
}

void rescanMidiInputDevices(void)
{
	freeMidiInputDeviceList();

	midi.numInputDevices = getNumMidiInDevices();
	if (midi.numInputDevices > MAX_MIDI_DEVICES)
		midi.numInputDevices = MAX_MIDI_DEVICES;

	for (uint32_t i = 0; i < midi.numInputDevices; i++)
	{
		char *deviceName = getMidiInDeviceName(i);
		if (deviceName == NULL)
		{
			if (midi.numInputDevices > 0)
				midi.numInputDevices--; // hide device

			continue;
		}

		midi.inputDeviceNames[i] = deviceName;
	}

	setScrollBarEnd(SB_MIDI_INPUT_SCROLL, midi.numInputDevices);
	setScrollBarPos(SB_MIDI_INPUT_SCROLL, 0, false);
}

void drawMidiInputList(void)
{
	clearRect(114, 4, 365, 165);

	if (!midi.initThreadDone || midiInDev == NULL || midi.numInputDevices == 0)
	{
		textOut(114, 4 + (0 * 11), PAL_FORGRND, "No MIDI input devices found!");
		textOut(114, 4 + (1 * 11), PAL_FORGRND, "Either wait a few seconds for MIDI to initialize, or restart the");
		textOut(114, 4 + (2 * 11), PAL_FORGRND, "tracker if you recently plugged in a MIDI device.");
		return;
	}

	for (uint16_t i = 0; i < 15; i++)
	{
		uint32_t deviceEntry = getScrollBarPos(SB_MIDI_INPUT_SCROLL) + i;
		if (deviceEntry > MAX_MIDI_DEVICES)
			deviceEntry = MAX_MIDI_DEVICES;

		if (deviceEntry < midi.numInputDevices)
		{
			if (midi.inputDeviceNames[deviceEntry] == NULL)
				continue;

			const uint16_t y = 4 + (i * 11);

			if (midi.inputDeviceName != NULL)
			{
				if (_stricmp(midi.inputDeviceName, midi.inputDeviceNames[deviceEntry]) == 0)
					fillRect(114, y, 365, 10, PAL_BOXSLCT); // selection background color
			}

			char *tmpString = utf8ToCp850(midi.inputDeviceNames[deviceEntry], true);
			if (tmpString != NULL)
			{
				textOutClipX(114, y, PAL_FORGRND, tmpString, 479);
				free(tmpString);
			}
		}
	}
}

void scrollMidiInputDevListUp(void)
{
	scrollBarScrollUp(SB_MIDI_INPUT_SCROLL, 1);
}

void scrollMidiInputDevListDown(void)
{
	scrollBarScrollDown(SB_MIDI_INPUT_SCROLL, 1);
}

void sbMidiInputSetPos(uint32_t pos)
{
	if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_MIDI_INPUT)
		drawMidiInputList();

	(void)pos;
}

bool testMidiInputDeviceListMouseDown(void)
{
	if (!ui.configScreenShown || editor.currConfigScreen != CONFIG_SCREEN_MIDI_INPUT)
		return false;

	if (mouse.x < 114 || mouse.x > 479 || mouse.y < 4 || mouse.y > 166)
		return false; // we didn't click inside the list area

	if (!midi.initThreadDone)
		return true;

	uint32_t deviceNum = (uint32_t)scrollBars[SB_MIDI_INPUT_SCROLL].pos + ((mouse.y - 4) / 11);
	if (deviceNum > MAX_MIDI_DEVICES)
		deviceNum = MAX_MIDI_DEVICES;

	if (midi.numInputDevices == 0 || deviceNum >= midi.numInputDevices)
		return true;

	if (midi.inputDeviceName != NULL)
	{
		if (!_stricmp(midi.inputDeviceName, midi.inputDeviceNames[deviceNum]))
			return true; // we clicked the currently selected device, do nothing

		free(midi.inputDeviceName);
		midi.inputDeviceName = NULL;
	}

	midi.inputDeviceName = strdup(midi.inputDeviceNames[deviceNum]);
	midi.inputDevice = deviceNum;

	closeMidiInDevice();
	freeMidiIn();
	initMidiIn();
	openMidiInDevice(midi.inputDevice);

	drawMidiInputList();
	return true;
}

int32_t initMidiFunc(void *ptr)
{
	initMidiIn();
	setMidiInputDeviceFromConfig();
	openMidiInDevice(midi.inputDevice);
	midi.rescanDevicesFlag = true;
	midi.initThreadDone = true;

	return true;
	(void)ptr;
}

#ifdef FT2_STABILITY_TESTS
#include "../tests/midi_tests.inc"
#endif

#else
typedef int prevent_compiler_warning; // kludge: prevent warning about empty .c file if HAS_MIDI is not defined
#endif
