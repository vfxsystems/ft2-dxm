// for finding memory leaks in debug mode with Visual Studio
#include "ft2_pushbuttons.h"
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_audio.h"
#include "ft2_gui.h"
#include "ft2_pattern_ed.h"
#include "ft2_diskop.h"
#include "scopes/ft2_scopes.h"
#include "ft2_config.h"
#include "ft2_mouse.h"
#include "ft2_audio.h"
#include "ft2_wav_renderer.h"
#include "ft2_render_settings_gui.h"
#include "ft2_structs.h"
#include "ft2_checkboxes.h"

extern checkBox_t checkBoxes[]; // from ft2_checkboxes.c

#define UPDATE_VISUALS_AT_TICK 4
#define TICKS_PER_RENDER_CHUNK 64

enum
{
	WAV_FORMAT_PCM = 0x0001,
	WAV_FORMAT_IEEE_FLOAT = 0x0003
};

typedef struct wavHeader_t
{
	uint32_t chunkID, chunkSize, format, subchunk1ID, subchunk1Size;
	uint16_t audioFormat, numChannels;
	uint32_t sampleRate, byteRate;
	uint16_t blockAlign, bitsPerSample;
	uint32_t subchunk2ID, subchunk2Size;
} wavHeader_t;

bool useLegacyBPM = false;
uint8_t WDBitDepth = 16; // exposed for slot renderer
static uint8_t WDStartPos, WDStopPos;
uint8_t *wavRenderBuffer = NULL;
bool WDRenderToSlot = false; // false = file, true = sample slot
static uint32_t WDChannelMask = 0xFFFF; // 16-bit channel mask
int16_t WDAmp;
uint32_t WDFrequency = 44100;
static SDL_Thread *thread;

static void updateWavRenderer(void)
{
	char str[16];

	fillRect(195, 116, 56, 8, PAL_DESKTOP);
	textOut(237, 116, PAL_FORGRND, "Hz");
	sprintf(str, "%6d", WDFrequency);
	textOutFixed(195, 116, PAL_FORGRND, PAL_DESKTOP, str);
	
	fillRect(229, 130, 21, 8, PAL_DESKTOP);
	charOut(243, 130, PAL_FORGRND, 'x');
	sprintf(str, "%02d", WDAmp);
	textOut(229, 130, PAL_FORGRND, str);

	fillRect(237, 144, 13, 8, PAL_DESKTOP);
	hexOut(237, 144, PAL_FORGRND, WDStartPos, 2);

	fillRect(237, 158, 13, 8, PAL_DESKTOP);
	hexOut(237, 158, PAL_FORGRND, WDStopPos, 2);
}

void cbToggleWavRenderBPMMode(void)
{
	useLegacyBPM ^= 1;
}

void setWavRenderFrequency(int32_t freq)
{
	WDFrequency = CLAMP(freq, MIN_WAV_RENDER_FREQ, MAX_WAV_RENDER_FREQ);
	if (ui.wavRendererShown)
		updateWavRenderer();
}

void setWavRenderBitDepth(uint8_t bitDepth)
{
	if (bitDepth == 16)
		WDBitDepth = 16;
	else if (bitDepth == 32)
		WDBitDepth = 32;

	if (ui.wavRendererShown)
		updateWavRenderer();
}

void updateWavRendererSettings(void) // called when changing config.boostLevel
{
	WDAmp = config.boostLevel;
}

void drawWavRenderer(void)
{
	drawFramework(0,   92, 291, 17, FRAMEWORK_TYPE1);
	drawFramework(0,  109,  79, 64, FRAMEWORK_TYPE1);
	drawFramework(79, 109, 212, 64, FRAMEWORK_TYPE1);

	textOutShadow(4,   95, PAL_FORGRND, PAL_DSKTOP2, "Rendering and Resampling:");
	textOutShadow(205, 95, PAL_FORGRND, PAL_DSKTOP2, "16bit");
	textOutShadow(255, 95, PAL_FORGRND, PAL_DSKTOP2, "32bit");

	textOutShadow(19, 114, PAL_FORGRND, PAL_DSKTOP2, "Imprecise");
	textOutShadow(4,  127, PAL_FORGRND, PAL_DSKTOP2, "BPM (FT2)");

	textOutShadow(165, 116, PAL_FORGRND, PAL_DSKTOP2, "Rate");
	textOutShadow(165, 130, PAL_FORGRND, PAL_DSKTOP2, "Amp");
	textOutShadow(165, 144, PAL_FORGRND, PAL_DSKTOP2, "Start Pos");
	textOutShadow(165, 158, PAL_FORGRND, PAL_DSKTOP2, "End Pos");
	// labels for target
	textOutShadow(105, 116, PAL_FORGRND, PAL_DSKTOP2, "> File");
	textOutShadow(105, 130, PAL_FORGRND, PAL_DSKTOP2, "> Slot");

	showPushButton(PB_WAV_RENDER);
	showPushButton(PB_WAV_SETTINGS);
	showPushButton(PB_WAV_FREQ_UP);
	showPushButton(PB_WAV_FREQ_DOWN);
	showPushButton(PB_WAV_AMP_UP);
	showPushButton(PB_WAV_AMP_DOWN);
	showPushButton(PB_WAV_START_UP);
	showPushButton(PB_WAV_START_DOWN);
	showPushButton(PB_WAV_END_UP);
	showPushButton(PB_WAV_END_DOWN);
	showPushButton(PB_WAV_EXIT);
	showCheckBox(CB_WAV_BPM_MODE);

	// bitdepth radiobuttons
	radioButtons[RB_WAV_RENDER_BITDEPTH16].state = RADIOBUTTON_UNCHECKED;
	radioButtons[RB_WAV_RENDER_BITDEPTH32].state = RADIOBUTTON_UNCHECKED;

	if (WDBitDepth == 16)
		radioButtons[RB_WAV_RENDER_BITDEPTH16].state = RADIOBUTTON_CHECKED;
	else
		radioButtons[RB_WAV_RENDER_BITDEPTH32].state = RADIOBUTTON_CHECKED;

	showRadioButtonGroup(RB_GROUP_WAV_RENDER_BITDEPTH);

	// target radiobuttons
	radioButtons[RB_WAV_RENDER_TARGET_FILE].state = RADIOBUTTON_UNCHECKED;
	radioButtons[RB_WAV_RENDER_TARGET_SLOT].state = RADIOBUTTON_UNCHECKED;
	if (WDRenderToSlot)
		radioButtons[RB_WAV_RENDER_TARGET_SLOT].state = RADIOBUTTON_CHECKED;
	else
		radioButtons[RB_WAV_RENDER_TARGET_FILE].state = RADIOBUTTON_CHECKED;
	
	showRadioButtonGroup(RB_GROUP_WAV_RENDER_TARGET);

	updateWavRenderer();
}

void resetWavRenderer(void)
{
	WDStartPos = 0;
	WDStopPos = (uint8_t)song.songLength - 1;

	if (ui.wavRendererShown)
		updateWavRenderer();
}

void showWavRenderer(void)
{
	if (ui.extendedPatternEditor)
		exitPatternEditorExtended();

	hideTopScreen();
	showTopScreen(false);

	ui.wavRendererShown = true;
	ui.scopesShown = false;

	WDStartPos = 0;
	WDStopPos = (uint8_t)song.songLength - 1;

	drawWavRenderer();
}

void hideWavRenderer(void)
{
	ui.wavRendererShown = false;

	hidePushButton(PB_WAV_RENDER);
	hidePushButton(PB_WAV_EXIT);
	hidePushButton(PB_WAV_FREQ_UP);
	hidePushButton(PB_WAV_FREQ_DOWN);
	hidePushButton(PB_WAV_AMP_UP);
	hidePushButton(PB_WAV_AMP_DOWN);
	hidePushButton(PB_WAV_START_UP);
	hidePushButton(PB_WAV_START_DOWN);
	hidePushButton(PB_WAV_END_UP);
	hidePushButton(PB_WAV_END_DOWN);
	hidePushButton(PB_WAV_SETTINGS);
	hideCheckBox(CB_WAV_BPM_MODE);
	hideRadioButtonGroup(RB_GROUP_WAV_RENDER_BITDEPTH);
	hideRadioButtonGroup(RB_GROUP_WAV_RENDER_TARGET);

	ui.scopesShown = true;
	drawScopeFramework();
}

void exitWavRenderer(void)
{
	hideWavRenderer();
}

bool dump_Init(uint32_t frq, int16_t amp, int16_t songPos)
{
	int32_t bytesPerSample = (WDBitDepth / 8) * 2; // 2 channels
	int32_t maxSamplesPerTick = (int32_t)ceil(frq / (MIN_BPM / 2.5)) + 1;

	// *2 for stereo
	wavRenderBuffer = (uint8_t *)malloc((TICKS_PER_RENDER_CHUNK * maxSamplesPerTick) * bytesPerSample);
	if (wavRenderBuffer == NULL)
		return false;

	// Reset end-of-tune flag for fresh rendering
	editor.wavReachedEndFlag = false;

	editor.wavIsRendering = true;

	setPos(songPos, 0, true);
	playMode = PLAYMODE_SONG;
	songPlaying = true;

	resetChannels();
	setNewAudioFreq(frq);
	setAudioAmp(amp, config.masterVol, (WDBitDepth == 32));

	stopVoices();
	song.globalVolume = 64;
	setMixerBPM(song.BPM);

	resetPlaybackTime();
	return true;
}

void dump_Close(FILE *f, uint32_t totalSamples)
{
    /* Support in-memory renders (renderSelectionToSlot) by allowing f==NULL */
    if (f == NULL)
    {
        if (wavRenderBuffer != NULL)
        {
            free(wavRenderBuffer);
            wavRenderBuffer = NULL;
        }

        stopPlaying();

        /* kludge: set speed to 6 if speed was set to 0 */
        if (song.speed == 0)
            song.speed = 6;

        setBackOldAudioFreq();
        setMixerBPM(song.BPM);
        setAudioAmp(config.boostLevel, config.masterVol, !!(config.specialFlags & BITDEPTH_32));
        editor.wavIsRendering = false;
        setMouseBusy(false);
        return;
    }
	wavHeader_t wavHeader;

	if (wavRenderBuffer != NULL)
	{
		free(wavRenderBuffer);
		wavRenderBuffer = NULL;
	}

	uint32_t totalBytes;
	if (WDBitDepth == 16)
		totalBytes = totalSamples * sizeof (int16_t);
	else
		totalBytes = totalSamples * sizeof (float);

	if (totalBytes & 1)
		fputc(0, f); // write pad byte

	uint32_t tmpLen = ftell(f)-8;

	// go back and fill in WAV header
	rewind(f);

	wavHeader.chunkID = 0x46464952; // "RIFF"
	wavHeader.chunkSize = tmpLen;
	wavHeader.format = 0x45564157; // "WAVE"
	wavHeader.subchunk1ID = 0x20746D66; // "fmt "
	wavHeader.subchunk1Size = 16;

	if (WDBitDepth == 16)
		wavHeader.audioFormat = WAV_FORMAT_PCM;
	else
		wavHeader.audioFormat = WAV_FORMAT_IEEE_FLOAT;

	wavHeader.numChannels = 2;
	wavHeader.sampleRate = WDFrequency;
	wavHeader.byteRate = (wavHeader.sampleRate * wavHeader.numChannels * WDBitDepth) / 8;
	wavHeader.blockAlign = (wavHeader.numChannels * WDBitDepth) / 8;
	wavHeader.bitsPerSample = WDBitDepth;
	wavHeader.subchunk2ID = 0x61746164; // "data"
	wavHeader.subchunk2Size = totalBytes;

	// write main header
	fwrite(&wavHeader, 1, sizeof (wavHeader_t), f);
	fclose(f);

	stopPlaying();

	// kludge: set speed to 6 if speed was set to 0
	if (song.speed == 0)
		song.speed = 6;

	setBackOldAudioFreq();
	setMixerBPM(song.BPM);
	setAudioAmp(config.boostLevel, config.masterVol, !!(config.specialFlags & BITDEPTH_32));
	editor.wavIsRendering = false;

	setMouseBusy(false);
}

bool dump_EndOfTune(int16_t endSongPos)
{
	bool returnValue = (editor.wavReachedEndFlag && song.row == 0 && song.tick == 1) || (song.speed == 0);

	// FT2 bugfix for EEx (pattern delay) on first row of a pattern
	if (song.pattDelTime2 > 0)
		returnValue = false;

	if (song.songPos == endSongPos && song.row == 0 && song.tick == 1)
		editor.wavReachedEndFlag = true;

	return returnValue;
}

void dump_TickReplayer(void)
{
	replayerBusy = true;
	if (!musicPaused)
	{
		if (audio.volumeRampingFlag)
			resetRampVolumes();

		tickReplayer();
		updateVoices();
	}
	replayerBusy = false;
}

static void updateVisuals(void)
{
	editor.editPattern = (uint8_t)song.pattNum;
	editor.row = song.row;
	editor.songPos = song.songPos;
	editor.BPM = song.BPM;
	editor.speed = song.speed;
	editor.globalVolume = song.globalVolume;

	ui.drawPosEdFlag = true;
	ui.drawPattNumLenFlag = true;
	ui.drawReplayerPianoFlag = true;
	ui.drawBPMFlag = true;
	ui.drawSpeedFlag = true;
	ui.drawGlobVolFlag = true;
	ui.updatePatternEditor = true;
}

static int32_t SDLCALL renderWavThread(void *ptr)
{
	(void)ptr;

	FILE *f = (FILE *)editor.wavRendererFileHandle;
	fseek(f, sizeof (wavHeader_t), SEEK_SET);

	pauseAudio();

	if (!dump_Init(WDFrequency, WDAmp, WDStartPos))
	{
		resumeAudio();
		okBoxThreadSafe(0, "System message", "Not enough memory!", NULL);
		return true;
	}

	uint32_t sampleCounter = 0;
	bool overflow = false, renderDone = false;
	uint8_t tickCounter = UPDATE_VISUALS_AT_TICK;
	uint64_t tickSamplesFrac = 0;

	uint64_t bytesInFile = sizeof (wavHeader_t);

	editor.wavReachedEndFlag = false;
	while (!renderDone)
	{
		uint32_t samplesInChunk = 0;

		// render several ticks at once to prevent frequent disk I/O (speeds up the process)
		uint8_t *ptr8 = wavRenderBuffer;
		for (uint32_t i = 0; i < TICKS_PER_RENDER_CHUNK; i++)
		{
			if (!editor.wavIsRendering || dump_EndOfTune(WDStopPos))
			{
				renderDone = true;
				break;
			}

			dump_TickReplayer();
			uint32_t tickSamples = audio.samplesPerTickInt;

			if (!useLegacyBPM)
			{
				tickSamplesFrac += audio.samplesPerTickFrac;
				if (tickSamplesFrac >= BPM_FRAC_SCALE)
				{
					tickSamplesFrac &= BPM_FRAC_MASK;
					tickSamples++;
				}
			}

			mixReplayerTickToBuffer(tickSamples, ptr8, WDBitDepth);

			tickSamples *= 2; // stereo
			samplesInChunk += tickSamples;
			sampleCounter += tickSamples;

			// increase buffer pointer
			if (WDBitDepth == 16)
			{
				ptr8 += tickSamples * sizeof (int16_t);
				bytesInFile += tickSamples * sizeof (int16_t);
			}
			else
			{
				ptr8 += tickSamples * sizeof (float);
				bytesInFile += tickSamples * sizeof (float);
			}

			if (bytesInFile >= INT32_MAX)
			{
				renderDone = true;
				overflow = true;
				break;
			}

			if (++tickCounter >= UPDATE_VISUALS_AT_TICK)
			{
				tickCounter = 0;
				updateVisuals();
			}
		}

		// write buffer to disk
		if (samplesInChunk > 0)
		{
			if (WDBitDepth == 16)
				fwrite(wavRenderBuffer, sizeof (int16_t), samplesInChunk, f);
			else
				fwrite(wavRenderBuffer, sizeof (float), samplesInChunk, f);
		}
	}

	updateVisuals();
	drawPlaybackTime(); // this is needed after the song stopped

	dump_Close(f, sampleCounter);
	resumeAudio();

	if (overflow)
		okBoxThreadSafe(0, "System message", "Rendering stopped, file exceeded 2GB!", NULL);

	editor.diskOpReadOnOpen = true;
	return true;
}

static void wavRender(bool checkOverwrite);
void pbWavSettings(void)
{
    showRenderSettingsDialog();
}

static void wavRender(bool checkOverwrite)
{
	WDStartPos = (uint8_t)(MAX(0, MIN(WDStartPos, song.songLength - 1)));
	WDStopPos  = (uint8_t)(MAX(0, MIN(MAX(WDStartPos, WDStopPos), song.songLength - 1)));

	updateWavRenderer();

	diskOpChangeFilenameExt(".wav");

	char *filename = getDiskOpFilename();
	if (checkOverwrite && fileExistsAnsi(filename))
	{
		char buf[256];
		createFileOverwriteText(filename, buf);
		if (okBox(2, "System request", buf, NULL) != 1)
			return;
	}

	editor.wavRendererFileHandle = fopen(filename, "wb");
	if (editor.wavRendererFileHandle == NULL)
	{
		okBox(0, "System message", "General I/O error while writing to WAV (is the file in use)?", NULL);
		return;
	}

	mouseAnimOn();
	thread = SDL_CreateThread(renderWavThread, NULL, NULL);
	if (thread == NULL)
	{
		fclose((FILE *)editor.wavRendererFileHandle);
		okBox(0, "System message", "Couldn't create thread!", NULL);
		return;
	}

	SDL_DetachThread(thread);
}

void pbWavRender(void)
{
	if (WDRenderToSlot)
	{
		/* Build channel mask from current mute states (bit set = channel active) */
		uint32_t chMask = 0;
		for (int i = 0; i < 16 && i < MAX_CHANNELS; i++)
		{
			if (!editor.channelMuted[i])
				chMask |= (1U << i);
		}
		if (!renderSelectionToSlot(chMask, WDStartPos, WDStopPos, WDBitDepth))
		{
			okBox(0, "System message", "Resample failed!", NULL);
		}
		return;
	}

	wavRender(config.cfg_OverwriteWarning ? true : false);
}

void pbWavExit(void)
{
	exitWavRenderer();
}

void pbWavFreqUp(void)
{
	if (WDFrequency < MAX_WAV_RENDER_FREQ)
	{
		     if (WDFrequency ==  44100) WDFrequency = 48000;
		else if (WDFrequency ==  48000) WDFrequency = 96000;
		else if (WDFrequency ==  96000) WDFrequency = 192000;
		else if (WDFrequency == 192000) WDFrequency = 384000;

		updateWavRenderer();
	}
}

void pbWavFreqDown(void)
{
	if (WDFrequency > MIN_WAV_RENDER_FREQ)
	{
		     if (WDFrequency == 384000) WDFrequency = 192000;
		else if (WDFrequency == 192000) WDFrequency = 96000;
		else if (WDFrequency ==  96000) WDFrequency = 48000;
		else if (WDFrequency ==  48000) WDFrequency = 44100;

		updateWavRenderer();
	}
}

void pbWavAmpUp(void)
{
	if (WDAmp >= 32)
		return;

	WDAmp++;
	updateWavRenderer();
}

void pbWavAmpDown(void)
{
	if (WDAmp <= 1)
		return;

	WDAmp--;
	updateWavRenderer();
}

void pbWavSongStartUp(void)
{
	if (WDStartPos >= song.songLength-1)
		return;

	WDStartPos++;
	WDStopPos = (uint8_t)(MIN(MAX(WDStartPos, WDStopPos), song.songLength - 1));
	updateWavRenderer();
}

void pbWavSongStartDown(void)
{
	if (WDStartPos == 0)
		return;

	WDStartPos--;
	updateWavRenderer();
}

void pbWavSongEndUp(void)
{
	if (WDStopPos >= 255)
		return;

	WDStopPos++;
	WDStopPos = (uint8_t)(MIN(MAX(WDStartPos, WDStopPos), song.songLength - 1));
	updateWavRenderer();
}

void pbWavSongEndDown(void)
{
	if (WDStopPos == 0)
		return;

	WDStopPos--;
	WDStopPos = (uint8_t)(MIN(MAX(WDStartPos, WDStopPos), song.songLength - 1));
	updateWavRenderer();
}

void rbWavRenderBitDepth16(void)
{
	checkRadioButton(RB_WAV_RENDER_BITDEPTH16);
	WDBitDepth = 16;
}

void rbWavRenderBitDepth32(void)
{
	checkRadioButton(RB_WAV_RENDER_BITDEPTH32);
	WDBitDepth = 32;
}

// ---------------------------------------------------------------------
// Render target radio button callbacks
// ---------------------------------------------------------------------
void rbWavRenderTargetFile(void)
{
	checkRadioButton(RB_WAV_RENDER_TARGET_FILE);
	WDRenderToSlot = false;
}

void rbWavRenderTargetSlot(void)
{
	checkRadioButton(RB_WAV_RENDER_TARGET_SLOT);
	WDRenderToSlot = true;
}
