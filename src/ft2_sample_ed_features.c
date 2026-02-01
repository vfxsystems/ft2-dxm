/* This file contains the routines for the following sample editor functions:
** - Resampler
** - Echo
** - Mix
** - Volume
** - Chop
**/

// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "ft2_header.h"
#include "ft2_mouse.h"
#include "ft2_audio.h"
#include "ft2_gui.h"
#include "ft2_events.h"
#include "ft2_video.h"
#include "ft2_inst_ed.h"
#include "ft2_sample_ed.h"
#include "ft2_keyboard.h"
#include "ft2_tables.h"
#include "ft2_structs.h"
#include "ft2_sample_ed_features.h"

// External functions from ft2_sample_ed.c needed for slice markers
extern int32_t smpPos2Scr(int32_t pos);
extern int32_t scr2SmpPos(int32_t x);

// Forward declaration
static void analyzeTransients(void);
static void updateSliceMarkersAfterCountChange(void);
static void sbSetNumSlicesPos(uint32_t pos);
static void pbNumSlicesDown(void);
static void pbNumSlicesUp(void);
static void previewSlice(int32_t markerIndex);

static volatile bool stopThread;

static int8_t smpEd_RelReSmp, mix_Balance = 50;
static bool echo_AddMemory, exitFlag, outOfMemory;
static int16_t echo_nEcho = 1, echo_VolChange = 30;
static int32_t echo_Distance = 0x100;
static double dVol_StartVol = 100.0, dVol_EndVol = 100.0;
static SDL_Thread *thread;

// Slice marker data
static int16_t slicer_Sensitivity = 50;
static bool slicer_CreateMarkers = true, slicer_SliceToSamples = false;
// --- NEW: number-of-slices control and marker preservation flag ---
static int16_t slicer_NumSlices = 4;          // desired number of slices (1..16)
static bool   preserveMarkers   = false;      // keep markers after manual editing
int32_t *sliceMarkers = NULL;
int32_t numSliceMarkers = 0;
bool sliceMarkersVisible = false;

// Interactive slice marker data
int32_t selectedSliceMarker = -1;
bool draggingSliceMarker = false;

// Function to compare markers by energy for sorting
static int32_t compareInt32(const void *a, const void *b)
{
    // Compare using the upper bits which contain the energy value
    return (*(int32_t *)b >> 31) - (*(int32_t *)a >> 31);
}

static void pbExit(void)
{
	ui.sysReqShown = false;
	exitFlag = true;
}

static void windowOpen(void)
{
	ui.sysReqShown = true;
	ui.sysReqEnterPressed = false;

	unstuckLastUsedGUIElement();
	SDL_EventState(SDL_DROPFILE, SDL_DISABLE);
}

static void windowClose(bool rewriteSample)
{
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

	if (exitFlag || rewriteSample)
		writeSample(true);
	else
		updateNewSample();

	mouseAnimOff();
}

static void sbSetResampleTones(uint32_t pos)
{
	if (smpEd_RelReSmp != (int8_t)(pos - 36))
		smpEd_RelReSmp = (int8_t)(pos - 36);
}

static void pbResampleTonesDown(void)
{
	if (smpEd_RelReSmp > -36)
		smpEd_RelReSmp--;
}

static void pbResampleTonesUp(void)
{
	if (smpEd_RelReSmp < 36)
		smpEd_RelReSmp++;
}

static int32_t SDLCALL resampleThread(void *ptr)
{
	smpPtr_t sp;

	if (instr[editor.curInstr] == NULL)
		return true;

	sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];
	bool sample16Bit = !!(s->flags & SAMPLE_16BIT);

	const double dRatio = pow(2.0, (int32_t)smpEd_RelReSmp * (1.0 / 12.0));

	double dNewLen = s->length * dRatio;
	if (dNewLen > (double)MAX_SAMPLE_LEN)
		dNewLen = (double)MAX_SAMPLE_LEN;

	const uint32_t newLen = (int32_t)floor(dNewLen);
	if (!allocateSmpDataPtr(&sp, newLen, sample16Bit))
	{
		outOfMemory = true;
		setMouseBusy(false);
		ui.sysReqShown = false;
		return true;
	}

	int8_t *dst = sp.ptr;
	int8_t *src = s->dataPtr;

	// 32.32 fixed-point logic
	const uint64_t delta64 = (const uint64_t)round((UINT32_MAX+1.0) / dRatio);
	uint64_t posFrac64 = 0;

	pauseAudio();
	unfixSample(s);

	/* Fast nearest-neighbor resampling.
	**
	** Could benefit from windowed-sinc interpolation,
	** but it seems like some people prefer it the way it is.
	*/

	if (newLen > 0)
	{
		if (sample16Bit)
		{
			const int16_t *src16 = (const int16_t *)src;
			int16_t *dst16 = (int16_t *)dst;

			for (uint32_t i = 0; i < newLen; i++)
			{
				dst16[i] = src16[posFrac64 >> 32];
				posFrac64 += delta64;
			}
		}
		else // 8-bit
		{
			const int8_t *src8 = src;
			int8_t *dst8 = dst;

			for (uint32_t i = 0; i < newLen; i++)
			{
				dst8[i] = src8[posFrac64 >> 32];
				posFrac64 += delta64;
			}
		}
	}

	freeSmpData(s);
	setSmpDataPtr(s, &sp);

	s->relativeNote += smpEd_RelReSmp;
	s->length = newLen;
	s->loopStart = (int32_t)(s->loopStart * dRatio);
	s->loopLength = (int32_t)(s->loopLength * dRatio);

	sanitizeSample(s);

	fixSample(s);
	resumeAudio();

	setSongModifiedFlag();
	setMouseBusy(false);

	ui.sysReqShown = false;
	return true;

	(void)ptr;
}

static void pbDoResampling(void)
{
	mouseAnimOn();
	thread = SDL_CreateThread(resampleThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!", NULL);
		return;
	}

	SDL_DetachThread(thread);
}

static void drawResampleBox(void)
{
	char sign;
	const int16_t x = 209;
	const int16_t y = 230;
	const int16_t w = 214;
	const int16_t h = 54;

	// main fill
	fillRect(x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);

	// outer border
	vLine(x,         y,         h - 1, PAL_BUTTON1);
	hLine(x + 1,     y,         w - 2, PAL_BUTTON1);
	vLine(x + w - 1, y,         h,     PAL_BUTTON2);
	hLine(x,         y + h - 1, w - 1, PAL_BUTTON2);

	// inner border
	vLine(x + 2,     y + 2,     h - 5, PAL_BUTTON2);
	hLine(x + 3,     y + 2,     w - 6, PAL_BUTTON2);
	vLine(x + w - 3, y + 2,     h - 4, PAL_BUTTON1);
	hLine(x + 2,     y + h - 3, w - 4, PAL_BUTTON1);

	sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];

	double dLenMul = pow(2.0, smpEd_RelReSmp * (1.0 / 12.0));

	double dNewLen = s->length * dLenMul;
	if (dNewLen > (double)MAX_SAMPLE_LEN)
		dNewLen = (double)MAX_SAMPLE_LEN;

	textOutShadow(215, 236, PAL_FORGRND, PAL_BUTTON2, "Rel. h.tones");
	textOutShadow(215, 250, PAL_FORGRND, PAL_BUTTON2, "New sample size");
	hexOut(361, 250, PAL_FORGRND, (int32_t)dNewLen, 8);

	     if (smpEd_RelReSmp == 0) sign = ' ';
	else if (smpEd_RelReSmp  < 0) sign = '-';
	else sign = '+';

	uint16_t val = ABS(smpEd_RelReSmp);
	if (val > 9)
	{
		charOut(291, 236, PAL_FORGRND, sign);
		charOut(298, 236, PAL_FORGRND, '0' + ((val / 10) % 10));
		charOut(305, 236, PAL_FORGRND, '0' + (val % 10));
	}
	else
	{
		charOut(298, 236, PAL_FORGRND, sign);
		charOut(305, 236, PAL_FORGRND, '0' + (val % 10));
	}
}

static void setupResampleBoxWidgets(void)
{
	pushButton_t *p;
	scrollBar_t *s;

	// "Apply" pushbutton
	p = &pushButtons[0];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = "Apply";
	p->x = 214;
	p->y = 264;
	p->w = 73;
	p->h = 16;
	p->callbackFuncOnUp = pbDoResampling;
	p->visible = true;

	// "Exit" pushbutton
	p = &pushButtons[1];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = "Exit";
	p->x = 345;
	p->y = 264;
	p->w = 73;
	p->h = 16;
	p->callbackFuncOnUp = pbExit;
	p->visible = true;

	// scrollbar buttons

	p = &pushButtons[2];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_LEFT_STRING;
	p->x = 314;
	p->y = 234;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbResampleTonesDown;
	p->visible = true;

	p = &pushButtons[3];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_RIGHT_STRING;
	p->x = 395;
	p->y = 234;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbResampleTonesUp;
	p->visible = true;

	// echo num scrollbar
	s = &scrollBars[0];
	memset(s, 0, sizeof (scrollBar_t));
	s->x = 337;
	s->y = 234;
	s->w = 58;
	s->h = 13;
	s->callbackFunc = sbSetResampleTones;
	s->visible = true;
	setScrollBarPageLength(0, 1);
	setScrollBarEnd(0, 36 * 2);
}

void pbSampleResample(void)
{
	uint16_t i;

	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->smp[editor.curSmp].dataPtr == NULL)
	{
		return;
	}

	setupResampleBoxWidgets();
	windowOpen();

	outOfMemory = false;

	exitFlag = false;
	while (ui.sysReqShown)
	{
		readInput();
		if (ui.sysReqEnterPressed)
			pbDoResampling();

		setSyncedReplayerVars();
		handleRedrawing();

		drawResampleBox();
		setScrollBarPos(0, smpEd_RelReSmp + 36, false);
		drawCheckBox(0);
		for (i = 0; i < 4; i++) drawPushButton(i);
		drawScrollBar(0);

		flipFrame();
	}

	for (i = 0; i < 4; i++) hidePushButton(i);
	hideScrollBar(0);

	windowClose(false);

	if (outOfMemory)
		okBox(0, "System message", "Not enough memory!", NULL);
}

static void cbEchoAddMemory(void)
{
	echo_AddMemory ^= 1;
}

static void sbSetEchoNumPos(uint32_t pos)
{
	if (echo_nEcho != (int32_t)pos)
		echo_nEcho = (int16_t)pos;
}

static void sbSetEchoDistPos(uint32_t pos)
{
	if (echo_Distance != (int32_t)pos)
		echo_Distance = (int32_t)pos;
}

static void sbSetEchoFadeoutPos(uint32_t pos)
{
	if (echo_VolChange != (int32_t)pos)
		echo_VolChange = (int16_t)pos;
}

static void pbEchoNumDown(void)
{
	if (echo_nEcho > 0)
		echo_nEcho--;
}

static void pbEchoNumUp(void)
{
	if (echo_nEcho < 64)
		echo_nEcho++;
}

static void pbEchoDistDown(void)
{
	if (echo_Distance > 0)
		echo_Distance--;
}

static void pbEchoDistUp(void)
{
	if (echo_Distance < 16384)
		echo_Distance++;
}

static void pbEchoFadeoutDown(void)
{
	if (echo_VolChange > 0)
		echo_VolChange--;
}

static void pbEchoFadeoutUp(void)
{
	if (echo_VolChange < 100)
		echo_VolChange++;
}

static int32_t SDLCALL createEchoThread(void *ptr)
{
	smpPtr_t sp;

	if (echo_nEcho < 1)
	{
		ui.sysReqShown = false;
		return true;
	}

	sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];

	int32_t readLen = s->length;
	int8_t *readPtr = s->dataPtr;
	bool sample16Bit = !!(s->flags & SAMPLE_16BIT);
	int32_t distance = echo_Distance * 16;
	double dVolChange = echo_VolChange / 100.0;

	// calculate real number of echoes
	double dSmp = sample16Bit ? 32768.0 : 128.0;
	int32_t k = 0;
	while (k < echo_nEcho && dSmp >= 1.0)
	{
		dSmp *= dVolChange;
		k++;
	}
	int32_t nEchoes = k + 1;

	if (nEchoes < 1)
	{
		ui.sysReqShown = false;
		return true;
	}

	// set write length (either original length or full echo length)
	int32_t writeLen = readLen;
	if (echo_AddMemory)
	{
		int64_t tmp64 = (int64_t)distance * (nEchoes - 1);

		tmp64 += writeLen;
		if (tmp64 > MAX_SAMPLE_LEN)
			tmp64 = MAX_SAMPLE_LEN;

		writeLen = (uint32_t)tmp64;
	}

	if (!allocateSmpDataPtr(&sp, writeLen, sample16Bit))
	{
		outOfMemory = true;
		setMouseBusy(false);
		ui.sysReqShown = false;
		return false;
	}

	pauseAudio();
	unfixSample(s);

	int32_t writeIdx = 0;

	if (sample16Bit)
	{
		const int16_t *readPtr16 = (const int16_t *)readPtr;
		int16_t *writePtr16 = (int16_t *)sp.ptr;

		while (writeIdx < writeLen)
		{
			double dSmpOut = 0.0;
			double dSmpMul = 1.0;

			int32_t echoRead = writeIdx;
			int32_t echoCycle = nEchoes;

			while (!stopThread)
			{
				if (echoRead < readLen)
					dSmpOut += (int32_t)readPtr16[echoRead] * dSmpMul;

				dSmpMul *= dVolChange;

				echoRead -= distance;
				if (echoRead <= 0 || --echoCycle <= 0)
					break;
			}

			DROUND(dSmpOut);

			int32_t smp32 = (int32_t)dSmpOut;
			CLAMP16(smp32);
			writePtr16[writeIdx++] = (int16_t)smp32;
		}
	}
	else // 8-bit
	{
		int8_t *writePtr8 = sp.ptr;
		while (writeIdx < writeLen)
		{
			double dSmpOut = 0.0;
			double dSmpMul = 1.0;

			int32_t echoRead = writeIdx;
			int32_t echoCycle = nEchoes;

			while (!stopThread)
			{
				if (echoRead < readLen)
					dSmpOut += (int32_t)readPtr[echoRead] * dSmpMul;

				dSmpMul *= dVolChange;

				echoRead -= distance;
				if (echoRead <= 0 || --echoCycle <= 0)
					break;
			}

			DROUND(dSmpOut);

			int32_t smp32 = (int32_t)dSmpOut;
			CLAMP8(smp32);
			writePtr8[writeIdx++] = (int8_t)smp32;
		}
	}

	freeSmpData(s);
	setSmpDataPtr(s, &sp);

	if (stopThread) // we stopped before echo was done, realloc length
	{
		writeLen = writeIdx;
		reallocateSmpData(s, writeLen, sample16Bit);
		editor.updateCurSmp = true;
	}

	s->length = writeLen;

	fixSample(s);
	resumeAudio();

	setSongModifiedFlag();
	setMouseBusy(false);

	ui.sysReqShown = false;
	return true;

	(void)ptr;
}

static void pbCreateEcho(void)
{
	stopThread = false;

	mouseAnimOn();
	thread = SDL_CreateThread(createEchoThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!", NULL);
		return;
	}

	SDL_DetachThread(thread);
}

static void drawEchoBox(void)
{
	const int16_t x = 171;
	const int16_t y = 220;
	const int16_t w = 291;
	const int16_t h = 66;

	// main fill
	fillRect(x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);

	// outer border
	vLine(x,         y,         h - 1, PAL_BUTTON1);
	hLine(x + 1,     y,         w - 2, PAL_BUTTON1);
	vLine(x + w - 1, y,         h,     PAL_BUTTON2);
	hLine(x,         y + h - 1, w - 1, PAL_BUTTON2);

	// inner border
	vLine(x + 2,     y + 2,     h - 5, PAL_BUTTON2);
	hLine(x + 3,     y + 2,     w - 6, PAL_BUTTON2);
	vLine(x + w - 3, y + 2,     h - 4, PAL_BUTTON1);
	hLine(x + 2,     y + h - 3, w - 4, PAL_BUTTON1);

	textOutShadow(177, 226, PAL_FORGRND, PAL_BUTTON2, "Number of echoes");
	textOutShadow(177, 240, PAL_FORGRND, PAL_BUTTON2, "Echo distance");
	textOutShadow(177, 254, PAL_FORGRND, PAL_BUTTON2, "Fade out");
	textOutShadow(192, 270, PAL_FORGRND, PAL_BUTTON2, "Add memory to sample");

	assert(echo_nEcho <= 64);
	charOut(315 + (2 * 7), 226, PAL_FORGRND, '0' + (char)(echo_nEcho / 10));
	charOut(315 + (3 * 7), 226, PAL_FORGRND, '0' + (echo_nEcho % 10));

	assert(echo_Distance <= 0x4000);
	hexOut(308, 240, PAL_FORGRND, echo_Distance << 4, 5);

	assert(echo_VolChange <= 100);
	textOutFixed(312, 254, PAL_FORGRND, PAL_BUTTONS, dec3StrTab[echo_VolChange]);

	charOutShadow(313 + (3 * 7), 254, PAL_FORGRND, PAL_BUTTON2, '%');
}

static void setupEchoBoxWidgets(void)
{
	checkBox_t *c;
	pushButton_t *p;
	scrollBar_t *s;

	// "Add memory to sample" checkbox
	c = &checkBoxes[0];
	memset(c, 0, sizeof (checkBox_t));
	c->x = 176;
	c->y = 268;
	c->clickAreaWidth = 146;
	c->clickAreaHeight = 12;
	c->callbackFunc = cbEchoAddMemory;
	c->checked = echo_AddMemory ? CHECKBOX_CHECKED : CHECKBOX_UNCHECKED;
	c->visible = true;

	// "Apply" pushbutton
	p = &pushButtons[0];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = "Apply";
	p->x = 345;
	p->y = 266;
	p->w = 56;
	p->h = 16;
	p->callbackFuncOnUp = pbCreateEcho;
	p->visible = true;

	// "Exit" pushbutton
	p = &pushButtons[1];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = "Exit";
	p->x = 402;
	p->y = 266;
	p->w = 55;
	p->h = 16;
	p->callbackFuncOnUp = pbExit;
	p->visible = true;

	// scrollbar buttons

	p = &pushButtons[2];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_LEFT_STRING;
	p->x = 345;
	p->y = 224;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbEchoNumDown;
	p->visible = true;

	p = &pushButtons[3];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_RIGHT_STRING;
	p->x = 434;
	p->y = 224;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbEchoNumUp;
	p->visible = true;

	p = &pushButtons[4];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_LEFT_STRING;
	p->x = 345;
	p->y = 238;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbEchoDistDown;
	p->visible = true;

	p = &pushButtons[5];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_RIGHT_STRING;
	p->x = 434;
	p->y = 238;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbEchoDistUp;
	p->visible = true;

	p = &pushButtons[6];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_LEFT_STRING;
	p->x = 345;
	p->y = 252;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbEchoFadeoutDown;
	p->visible = true;

	p = &pushButtons[7];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_RIGHT_STRING;
	p->x = 434;
	p->y = 252;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbEchoFadeoutUp;
	p->visible = true;

	// echo num scrollbar
	s = &scrollBars[0];
	memset(s, 0, sizeof (scrollBar_t));
	s->x = 368;
	s->y = 224;
	s->w = 66;
	s->h = 13;
	s->callbackFunc = sbSetEchoNumPos;
	s->visible = true;
	setScrollBarPageLength(0, 1);
	setScrollBarEnd(0, 64);

	// echo distance scrollbar
	s = &scrollBars[1];
	memset(s, 0, sizeof (scrollBar_t));
	s->x = 368;
	s->y = 238;
	s->w = 66;
	s->h = 13;
	s->callbackFunc = sbSetEchoDistPos;
	s->visible = true;
	setScrollBarPageLength(1, 1);
	setScrollBarEnd(1, 16384);

	// echo fadeout scrollbar
	s = &scrollBars[2];
	memset(s, 0, sizeof (scrollBar_t));
	s->x = 368;
	s->y = 252;
	s->w = 66;
	s->h = 13;
	s->callbackFunc = sbSetEchoFadeoutPos;
	s->visible = true;
	setScrollBarPageLength(2, 1);
	setScrollBarEnd(2, 100);
}

void handleEchoToolPanic(void)
{
	stopThread = true;
}

void pbSampleEcho(void)
{
	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->smp[editor.curSmp].dataPtr == NULL)
	{
		return;
	}

	setupEchoBoxWidgets();
	windowOpen();

	outOfMemory = false;

	exitFlag = false;
	while (ui.sysReqShown)
	{
		readInput();
		if (ui.sysReqEnterPressed)
			pbCreateEcho();

		setSyncedReplayerVars();
		handleRedrawing();

		drawEchoBox();
		setScrollBarPos(0, echo_nEcho, false);
		setScrollBarPos(1, echo_Distance, false);
		setScrollBarPos(2, echo_VolChange, false);
		drawCheckBox(0);
		for (uint16_t i = 0; i < 8; i++) drawPushButton(i);
		for (uint16_t i = 0; i < 3; i++) drawScrollBar(i);

		flipFrame();
	}

	hideCheckBox(0);
	for (uint16_t i = 0; i < 8; i++) hidePushButton(i);
	for (uint16_t i = 0; i < 3; i++) hideScrollBar(i);

	windowClose(echo_AddMemory ? false : true);

	if (outOfMemory)
		okBox(0, "System message", "Not enough memory!", NULL);
}

static int32_t SDLCALL mixThread(void *ptr)
{
	smpPtr_t sp;

	int8_t *dstPtr, *mixPtr;
	uint8_t mixFlags, dstFlags;
	int32_t dstLen, mixLen;

	int16_t dstIns = editor.curInstr;
	int16_t dstSmp = editor.curSmp;
	int16_t mixIns = editor.srcInstr;
	int16_t mixSmp = editor.srcSmp;

	sample_t *s = &instr[dstIns]->smp[dstSmp];
	sample_t *sSrc = &instr[mixIns]->smp[mixSmp];

	if (dstIns == mixIns && dstSmp == mixSmp)
	{
		setMouseBusy(false);
		ui.sysReqShown = false;
		return true;
	}

	if (instr[mixIns] == NULL)
	{
		mixLen = 0;
		mixPtr = NULL;
		mixFlags = 0;
	}
	else
	{
		mixLen = sSrc->length;
		mixPtr = sSrc->dataPtr;
		mixFlags = sSrc->flags;

		if (mixPtr == NULL)
		{
			mixLen = 0;
			mixFlags = 0;
		}
	}

	if (instr[dstIns] == NULL)
	{
		dstLen = 0;
		dstPtr = NULL;
		dstFlags = 0;
	}
	else
	{
		dstLen = s->length;
		dstPtr = s->dataPtr;
		dstFlags = s->flags;

		if (dstPtr == NULL)
		{
			dstLen = 0;
			dstFlags = 0;
		}
	}

	bool src16Bits = !!(mixFlags & SAMPLE_16BIT);
	bool dst16Bits = !!(dstFlags & SAMPLE_16BIT);

	int32_t maxLen = (dstLen > mixLen) ? dstLen : mixLen;
	if (maxLen == 0)
	{
		setMouseBusy(false);
		ui.sysReqShown = false;
		return true;
	}

	if (!allocateSmpDataPtr(&sp, maxLen, dst16Bits))
	{
		outOfMemory = true;
		setMouseBusy(false);
		ui.sysReqShown = false;
		return true;
	}
	memset(sp.ptr, 0, maxLen);

	if (instr[dstIns] == NULL && !allocateInstr(dstIns))
	{
		outOfMemory = true;
		setMouseBusy(false);
		ui.sysReqShown = false;
		return true;
	}

	pauseAudio();
	unfixSample(s);

	// unfix source sample
	if (instr[mixIns] != NULL)
		unfixSample(sSrc);

	const double dAmp1 = mix_Balance / 100.0;
	const double dAmp2 = 1.0 - dAmp1;
	const double dSmp1ScaleMul = src16Bits ? (1.0 / 32768.0) : (1.0 / 128.0);
	const double dSmp2ScaleMul = dst16Bits ? (1.0 / 32768.0) : (1.0 / 128.0);
	const double dNormalizeMul = dst16Bits ? 32768.0 : 128.0;

	for (int32_t i = 0; i < maxLen; i++)
	{
		double dSmp1 = (i >= mixLen) ? 0.0 : (getSampleValue(mixPtr, i, src16Bits) * dSmp1ScaleMul); // -1.0 .. 0.999inf
		double dSmp2 = (i >= dstLen) ? 0.0 : (getSampleValue(dstPtr, i, dst16Bits) * dSmp2ScaleMul); // -1.0 .. 0.999inf

		const double dSmp = ((dSmp1 * dAmp1) + (dSmp2 * dAmp2)) * dNormalizeMul;
		putSampleValue(sp.ptr, i, dSmp, dst16Bits);
	}

	freeSmpData(s);
	setSmpDataPtr(s, &sp);

	s->length = maxLen;
	s->flags = dstFlags;

	fixSample(s);

	// re-fix source sample again
	if (instr[mixIns] != NULL)
		fixSample(sSrc);

	resumeAudio();

	setSongModifiedFlag();
	setMouseBusy(false);

	ui.sysReqShown = false;
	return true;

	(void)ptr;
}

static void pbMix(void)
{
	mouseAnimOn();
	thread = SDL_CreateThread(mixThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!", NULL);
		return;
	}

	SDL_DetachThread(thread);
}

static void sbSetMixBalancePos(uint32_t pos)
{
	if (mix_Balance != (int8_t)pos)
		mix_Balance = (int8_t)pos;
}

static void pbMixBalanceDown(void)
{
	if (mix_Balance > 0)
		mix_Balance--;
}

static void pbMixBalanceUp(void)
{
	if (mix_Balance < 100)
		mix_Balance++;
}

static void drawMixSampleBox(void)
{
	const int16_t x = 192;
	const int16_t y = 240;
	const int16_t w = 248;
	const int16_t h = 38;

	// main fill
	fillRect(x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);

	// outer border
	vLine(x,         y,         h - 1, PAL_BUTTON1);
	hLine(x + 1,     y,         w - 2, PAL_BUTTON1);
	vLine(x + w - 1, y,         h,     PAL_BUTTON2);
	hLine(x,         y + h - 1, w - 1, PAL_BUTTON2);

	// inner border
	vLine(x + 2,     y + 2,     h - 5, PAL_BUTTON2);
	hLine(x + 3,     y + 2,     w - 6, PAL_BUTTON2);
	vLine(x + w - 3, y + 2,     h - 4, PAL_BUTTON1);
	hLine(x + 2,     y + h - 3, w - 4, PAL_BUTTON1);

	textOutShadow(198, 246, PAL_FORGRND, PAL_BUTTON2, "Mixing balance");

	assert((mix_Balance >= 0) && (mix_Balance <= 100));
	textOutFixed(299, 246, PAL_FORGRND, PAL_BUTTONS, dec3StrTab[mix_Balance]);
}

static void setupMixBoxWidgets(void)
{
	pushButton_t *p;
	scrollBar_t *s;

	// "Apply" pushbutton
	p = &pushButtons[0];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = "Apply";
	p->x = 197;
	p->y = 258;
	p->w = 73;
	p->h = 16;
	p->callbackFuncOnUp = pbMix;
	p->visible = true;

	// "Exit" pushbutton
	p = &pushButtons[1];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = "Exit";
	p->x = 361;
	p->y = 258;
	p->w = 73;
	p->h = 16;
	p->callbackFuncOnUp = pbExit;
	p->visible = true;

	// scrollbar buttons

	p = &pushButtons[2];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_LEFT_STRING;
	p->x = 322;
	p->y = 244;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbMixBalanceDown;
	p->visible = true;

	p = &pushButtons[3];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_RIGHT_STRING;
	p->x = 411;
	p->y = 244;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbMixBalanceUp;
	p->visible = true;

	// mixing balance scrollbar
	s = &scrollBars[0];
	memset(s, 0, sizeof (scrollBar_t));
	s->x = 345;
	s->y = 244;
	s->w = 66;
	s->h = 13;
	s->callbackFunc = sbSetMixBalancePos;
	s->visible = true;
	setScrollBarPageLength(0, 1);
	setScrollBarEnd(0, 100);
}

void pbSampleMix(void)
{
	uint16_t i;

	if (editor.curInstr == 0)
		return;

	setupMixBoxWidgets();
	windowOpen();

	outOfMemory = false;

	exitFlag = false;
	while (ui.sysReqShown)
	{
		readInput();
		if (ui.sysReqEnterPressed)
			pbMix();

		setSyncedReplayerVars();
		handleRedrawing();

		drawMixSampleBox();
		setScrollBarPos(0, mix_Balance, false);
		for (i = 0; i < 4; i++) drawPushButton(i);
		drawScrollBar(0);

		flipFrame();
	}

	for (i = 0; i < 4; i++) hidePushButton(i);
	hideScrollBar(0);

	windowClose(false);

	if (outOfMemory)
		okBox(0, "System message", "Not enough memory!", NULL);
}

static void sbSetStartVolPos(uint32_t pos)
{
	int32_t val = (int32_t)(pos - 200);
	if (val != (int32_t)dVol_StartVol)
	{
		     if (ABS(val)       < 10) val =    0;
		else if (ABS(val - 100) < 10) val =  100;
		else if (ABS(val + 100) < 10) val = -100;

		dVol_StartVol = (double)val;
	}
}

static void sbSetEndVolPos(uint32_t pos)
{
	int32_t val = (int32_t)(pos - 200);
	if (val != (int32_t)dVol_EndVol)
	{
		     if (ABS(val)       < 10) val =    0;
		else if (ABS(val - 100) < 10) val =  100;
		else if (ABS(val + 100) < 10) val = -100;

		dVol_EndVol = val;
	}
}

static void pbSampStartVolDown(void)
{
	if (dVol_StartVol > -200.0)
		dVol_StartVol -= 1.0;

	dVol_StartVol = floor(dVol_StartVol);
}

static void pbSampStartVolUp(void)
{
	if (dVol_StartVol < 200.0)
		dVol_StartVol += 1.0;

	dVol_StartVol = floor(dVol_StartVol);
}

static void pbSampEndVolDown(void)
{
	if (dVol_EndVol > -200.0)
		dVol_EndVol -= 1.0;

	dVol_EndVol = floor(dVol_EndVol);
}

static void pbSampEndVolUp(void)
{
	if (dVol_EndVol < 200.0)
		dVol_EndVol += 1.0;

	dVol_EndVol = floor(dVol_EndVol);
}

static int32_t SDLCALL applyVolumeThread(void *ptr)
{
	int32_t x1, x2;

	if (instr[editor.curInstr] == NULL)
		goto applyVolumeExit;

	sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];

	if (smpEd_Rx1 < smpEd_Rx2)
	{
		x1 = smpEd_Rx1;
		x2 = smpEd_Rx2;

		if (x2 > s->length)
			x2 = s->length;

		if (x1 < 0)
			x1 = 0;

		if (x2 <= x1)
			goto applyVolumeExit;
	}
	else
	{
		// no mark, operate on whole sample
		x1 = 0;
		x2 = s->length;
	}

	const int32_t len = x2 - x1;
	if (len <= 0)
		goto applyVolumeExit;

	const bool mustInterpolate = (dVol_StartVol != dVol_EndVol);
	const double dVolDelta = ((dVol_EndVol - dVol_StartVol) / 100.0) / len;
	double dVol = dVol_StartVol / 100.0;

	pauseAudio();
	unfixSample(s);
	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
		if (mustInterpolate)
		{
			for (int32_t i = 0; i < len; i++)
			{
				int32_t smp32 = (int32_t)((int32_t)ptr16[i] * dVol);
				CLAMP16(smp32);
				ptr16[i] = (int16_t)smp32;

				dVol += dVolDelta;
			}
		}
		else // no interpolation needed
		{
			for (int32_t i = 0; i < len; i++)
			{
				int32_t smp32 = (int32_t)((int32_t)ptr16[i] * dVol);
				CLAMP16(smp32);
				ptr16[i] = (int16_t)smp32;
			}
		}
	}
	else // 8-bit sample
	{
		int8_t *ptr8 = s->dataPtr + x1;
		if (mustInterpolate)
		{
			for (int32_t i = 0; i < len; i++)
			{
				int32_t smp32 = (int32_t)((int32_t)ptr8[i] * dVol);
				CLAMP8(smp32);
				ptr8[i] = (int8_t)smp32;

				dVol += dVolDelta;
			}
		}
		else // no interpolation needed
		{
			for (int32_t i = 0; i < len; i++)
			{
				int32_t smp32 = (int32_t)((int32_t)ptr8[i] * dVol);
				CLAMP8(smp32);
				ptr8[i] = (int8_t)smp32;
			}
		}
	}
	fixSample(s);
	resumeAudio();

	setSongModifiedFlag();

applyVolumeExit:
	setMouseBusy(false);
	ui.sysReqShown = false;

	return true;

	(void)ptr;
}

static void pbApplyVolume(void)
{
	if (dVol_StartVol == 100.0 && dVol_EndVol == 100.0)
	{
		ui.sysReqShown = false;
		return; // no volume change to be done
	}

	mouseAnimOn();
	thread = SDL_CreateThread(applyVolumeThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!", NULL);
		return;
	}

	SDL_DetachThread(thread);
}

static int32_t SDLCALL getMaxScaleThread(void *ptr)
{
	int32_t x1, x2;

	if (instr[editor.curInstr] == NULL)
		goto getScaleExit;

	sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];

	if (smpEd_Rx1 < smpEd_Rx2)
	{
		x1 = smpEd_Rx1;
		x2 = smpEd_Rx2;

		if (x2 > s->length)
			x2 = s->length;

		if (x1 < 0)
			x1 = 0;

		if (x2 <= x1)
			goto getScaleExit;
	}
	else
	{
		// no sample marking, operate on the whole sample
		x1 = 0;
		x2 = s->length;
	}

	uint32_t len = x2 - x1;
	if (len <= 0)
	{
		dVol_StartVol = dVol_EndVol = 100.0;
		goto getScaleExit;
	}

	double dVolChange = 100.0;

	/* If sample is looped and the loopEnd point is inside the marked range,
	** we need to unfix the fixed interpolation sample before scanning,
	** and fix it again after we're done.
	*/
	bool hasLoop = GET_LOOPTYPE(s->flags) != LOOP_OFF;
	const int32_t loopEnd = s->loopStart + s->loopLength;
	bool fixedSampleInRange = hasLoop && (x1 <= loopEnd) && (x2 >= loopEnd);

	if (fixedSampleInRange)
		unfixSample(s);

	int32_t maxAmp = 0;
	if (s->flags & SAMPLE_16BIT)
	{
		const int16_t *ptr16 = (const int16_t *)s->dataPtr + x1;
		for (uint32_t i = 0; i < len; i++)
		{
			const int32_t absSmp = ABS(ptr16[i]);
			if (absSmp > maxAmp)
				maxAmp = absSmp;
		}

		if (maxAmp > 0)
			dVolChange = (32767.0 / maxAmp) * 100.0;
	}
	else // 8-bit
	{
		const int8_t *ptr8 = (const int8_t *)&s->dataPtr[x1];
		for (uint32_t i = 0; i < len; i++)
		{
			const int32_t absSmp = ABS(ptr8[i]);
			if (absSmp > maxAmp)
				maxAmp = absSmp;
		}

		if (maxAmp > 0)
			dVolChange = (127.0 / maxAmp) * 100.0;
	}

	if (fixedSampleInRange)
		fixSample(s);

	if (dVolChange < 100.0) // yes, this can happen...
		dVolChange = 100.0;

	dVol_StartVol = dVol_EndVol = dVolChange;

getScaleExit:
	setMouseBusy(false);
	return true;

	(void)ptr;
}

static void pbGetMaxScale(void)
{
	mouseAnimOn();
	thread = SDL_CreateThread(getMaxScaleThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!", NULL);
		return;
	}

	SDL_DetachThread(thread);
}

static void drawSampleVolumeBox(void)
{
	char sign;
	const int16_t x = 166;
	const int16_t y = 230;
	const int16_t w = 301;
	const int16_t h = 52;
	uint32_t val;

	// main fill
	fillRect(x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);

	// outer border
	vLine(x,         y,         h - 1, PAL_BUTTON1);
	hLine(x + 1,     y,         w - 2, PAL_BUTTON1);
	vLine(x + w - 1, y,         h,     PAL_BUTTON2);
	hLine(x,         y + h - 1, w - 1, PAL_BUTTON2);

	// inner border
	vLine(x + 2,     y + 2,     h - 5, PAL_BUTTON2);
	hLine(x + 3,     y + 2,     w - 6, PAL_BUTTON2);
	vLine(x + w - 3, y + 2,     h - 4, PAL_BUTTON1);
	hLine(x + 2,     y + h - 3, w - 4, PAL_BUTTON1);

	textOutShadow(172, 236, PAL_FORGRND, PAL_BUTTON2, "Start volume");
	textOutShadow(172, 250, PAL_FORGRND, PAL_BUTTON2, "End volume");
	charOutShadow(282, 236, PAL_FORGRND, PAL_BUTTON2, '%');
	charOutShadow(282, 250, PAL_FORGRND, PAL_BUTTON2, '%');

	const int32_t startVol = (int32_t)dVol_StartVol;
	const int32_t endVol = (int32_t)dVol_EndVol;

	if (startVol > 200)
	{
		charOut(253, 236, PAL_FORGRND, '>');
		charOut(260, 236, PAL_FORGRND, '2');
		charOut(267, 236, PAL_FORGRND, '0');
		charOut(274, 236, PAL_FORGRND, '0');
	}
	else
	{
		     if (startVol == 0) sign = ' ';
		else if (startVol  < 0) sign = '-';
		else sign = '+';

		val = ABS(startVol);
		if (val > 99)
		{
			charOut(253, 236, PAL_FORGRND, sign);
			charOut(260, 236, PAL_FORGRND, '0' + (char)(val / 100));
			charOut(267, 236, PAL_FORGRND, '0' + ((val / 10) % 10));
			charOut(274, 236, PAL_FORGRND, '0' + (val % 10));
		}
		else if (val > 9)
		{
			charOut(260, 236, PAL_FORGRND, sign);
			charOut(267, 236, PAL_FORGRND, '0' + (char)(val / 10));
			charOut(274, 236, PAL_FORGRND, '0' + (val % 10));
		}
		else
		{
			charOut(267, 236, PAL_FORGRND, sign);
			charOut(274, 236, PAL_FORGRND, '0' + (char)val);
		}
	}

	if (endVol > 200)
	{
		charOut(253, 250, PAL_FORGRND, '>');
		charOut(260, 250, PAL_FORGRND, '2');
		charOut(267, 250, PAL_FORGRND, '0');
		charOut(274, 250, PAL_FORGRND, '0');
	}
	else
	{
		     if (endVol == 0) sign = ' ';
		else if (endVol  < 0) sign = '-';
		else sign = '+';

		val = ABS(endVol);
		if (val > 99)
		{
			charOut(253, 250, PAL_FORGRND, sign);
			charOut(260, 250, PAL_FORGRND, '0' + (char)(val / 100));
			charOut(267, 250, PAL_FORGRND, '0' + ((val / 10) % 10));
			charOut(274, 250, PAL_FORGRND, '0' + (val % 10));
		}
		else if (val > 9)
		{
			charOut(260, 250, PAL_FORGRND, sign);
			charOut(267, 250, PAL_FORGRND, '0' + (char)(val / 10));
			charOut(274, 250, PAL_FORGRND, '0' + (val % 10));
		}
		else
		{
			charOut(267, 250, PAL_FORGRND, sign);
			charOut(274, 250, PAL_FORGRND, '0' + (char)val);
		}
	}
}

static void setupVolumeBoxWidgets(void)
{
	pushButton_t *p;
	scrollBar_t *s;

	// "Apply" pushbutton
	p = &pushButtons[0];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = "Apply";
	p->x = 171;
	p->y = 262;
	p->w = 73;
	p->h = 16;
	p->callbackFuncOnUp = pbApplyVolume;
	p->visible = true;

	// "Get maximum scale" pushbutton
	p = &pushButtons[1];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = "Get maximum scale";
	p->x = 245;
	p->y = 262;
	p->w = 143;
	p->h = 16;
	p->callbackFuncOnUp = pbGetMaxScale;
	p->visible = true;

	// "Exit" pushbutton
	p = &pushButtons[2];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = "Exit";
	p->x = 389;
	p->y = 262;
	p->w = 73;
	p->h = 16;
	p->callbackFuncOnUp = pbExit;
	p->visible = true;

	// scrollbar buttons

	p = &pushButtons[3];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_LEFT_STRING;
	p->x = 292;
	p->y = 234;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbSampStartVolDown;
	p->visible = true;

	p = &pushButtons[4];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_RIGHT_STRING;
	p->x = 439;
	p->y = 234;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbSampStartVolUp;
	p->visible = true;

	p = &pushButtons[5];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_LEFT_STRING;
	p->x = 292;
	p->y = 248;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbSampEndVolDown;
	p->visible = true;

	p = &pushButtons[6];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_RIGHT_STRING;
	p->x = 439;
	p->y = 248;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbSampEndVolUp;
	p->visible = true;

	// volume start scrollbar
	s = &scrollBars[0];
	memset(s, 0, sizeof (scrollBar_t));
	s->x = 315;
	s->y = 234;
	s->w = 124;
	s->h = 13;
	s->callbackFunc = sbSetStartVolPos;
	s->visible = true;
	setScrollBarPageLength(0, 1);
	setScrollBarEnd(0, 200 * 2);
	setScrollBarPos(0, 200, false);

	// volume end scrollbar
	s = &scrollBars[1];
	memset(s, 0, sizeof (scrollBar_t));
	s->x = 315;
	s->y = 248;
	s->w = 124;
	s->h = 13;
	s->callbackFunc = sbSetEndVolPos;
	s->visible = true;
	setScrollBarPageLength(1, 1);
	setScrollBarEnd(1, 200 * 2);
	setScrollBarPos(1, 200, false);
}

void pbSampleVolume(void)
{
	uint16_t i;

	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->smp[editor.curSmp].dataPtr == NULL)
	{
		return;
	}

	setupVolumeBoxWidgets();
	windowOpen();

	exitFlag = false;
	while (ui.sysReqShown)
	{
		readInput();
		if (ui.sysReqEnterPressed)
		{
			pbApplyVolume();
			keyb.ignoreCurrKeyUp = true; // don't handle key up event for this key release
		}

		setSyncedReplayerVars();
		handleRedrawing();

		// this is needed for the "Get maximum scale" button
		if (ui.setMouseIdle) mouseAnimOff();

		drawSampleVolumeBox();

		const int32_t startVol = (int32_t)dVol_StartVol;
		const int32_t endVol = (int32_t)dVol_EndVol;

		setScrollBarPos(0, 200 + startVol, false);
		setScrollBarPos(1, 200 + endVol, false);
		for (i = 0; i < 7; i++) drawPushButton(i);
		for (i = 0; i < 2; i++) drawScrollBar(i);

		flipFrame();
	}

	for (i = 0; i < 7; i++) hidePushButton(i);
	for (i = 0; i < 2; i++) hideScrollBar(i);

	windowClose(true);
}

// *** SAMPLE SLICER ***

static void cbSlicerCreateMarkers(void)
{
	slicer_CreateMarkers ^= 1;
}

static void cbSlicerSliceToSamples(void)
{
	slicer_SliceToSamples ^= 1;
}

static void sbSetSlicerSensitivity(uint32_t pos)
{
	if (slicer_Sensitivity != (int16_t)pos)
		slicer_Sensitivity = (int16_t)pos;
}

static void pbSlicerSensitivityDown(void)
{
	if (slicer_Sensitivity > 1)
		slicer_Sensitivity--;
}

static void pbSlicerSensitivityUp(void)
{
	if (slicer_Sensitivity < 100)
		slicer_Sensitivity++;
}

static double calculateRMS(int8_t *smpData, int32_t start, int32_t windowSize, bool sample16Bit)
{
	double sum = 0.0;

	for (int32_t i = 0; i < windowSize; i++)
	{
		double sample = getSampleValue(smpData, start + i, sample16Bit);
		sum += sample * sample;
	}

	return sqrt(sum / windowSize);
}

static void freeSliceMarkers(void)
{
	if (sliceMarkers != NULL)
	{
		free(sliceMarkers);
		sliceMarkers = NULL;
	}
	numSliceMarkers = 0;
	sliceMarkersVisible = false;
	preserveMarkers = false;
}

// Remove a slice marker by index
void removeSliceMarker(int32_t index)
{
	if (index < 0 || index >= numSliceMarkers)
		return;

	// Shift remaining markers down
	for (int32_t i = index; i < numSliceMarkers - 1; i++)
		sliceMarkers[i] = sliceMarkers[i + 1];

	numSliceMarkers--;

	// Reallocate to save memory
	if (numSliceMarkers > 0)
	{
		sliceMarkers = (int32_t *)realloc(sliceMarkers, sizeof(int32_t) * numSliceMarkers);
		if (sliceMarkers == NULL)
		{
			numSliceMarkers = 0;
			sliceMarkersVisible = false;
		}
	}
	else
	{
		freeSliceMarkers();
	}
}

// Find slice marker at screen position (within tolerance)
static int32_t findSliceMarkerAtPosition(int32_t screenX)
{
	if (!sliceMarkersVisible || numSliceMarkers == 0)
		return -1;

	const int32_t tolerance = 4; // pixels

	for (int32_t i = 0; i < numSliceMarkers; i++)
	{
		int32_t markerScreenX = smpPos2Scr(sliceMarkers[i]);
		if (markerScreenX >= 0 && markerScreenX < SAMPLE_AREA_WIDTH)
		{
			if (abs(screenX - markerScreenX) <= tolerance)
				return i;
		}
	}

	return -1;
}

// Handle mouse interaction with slice markers
bool handleSliceMarkerMouseDown(int32_t mouseX, int32_t mouseY)
{
	if (!sliceMarkersVisible || numSliceMarkers == 0)
		return false;

	// Check if we're in the sample display area
	if (mouseY < 174 || mouseY >= 174 + SAMPLE_AREA_HEIGHT)
		return false;

	/* First, check if the click was on a marker *number* (preview request). */
	const int32_t smpEdY = 174;
	const int32_t smpEdH = SAMPLE_AREA_HEIGHT;
	const int32_t numberY = smpEdY + smpEdH - 8;

	if (mouseY >= numberY && mouseY <= numberY + 8)
	{
		for (int32_t i = 0; i < numSliceMarkers; i++)
		{
			int32_t markerScreenX = smpPos2Scr(sliceMarkers[i]);
			if (markerScreenX < 0 || markerScreenX >= SCREEN_W)
				continue;

			int32_t textX = markerScreenX + 4;
			if (textX + 8 > SCREEN_W - 16)
				textX = SCREEN_W - 24;

			int32_t digits = (i + 1 >= 10) ? 2 : 1;
			int32_t textWidth = digits * 7; // approx char width

			if (mouseX >= textX && mouseX <= textX + textWidth)
			{
				// Clicked on this number – preview slice
				previewSlice(i);
				return true;
			}
		}
	}

	/* Shift + left-click on the waveform = add new slice marker */
	if (keyb.leftShiftPressed)
	{
		sample_t *s = getCurSample();
		if (s != NULL && s->dataPtr != NULL)
		{
			int32_t newPos = scr2SmpPos(mouseX);
			if (newPos < 0) newPos = 0;
			if (newPos >= s->length) newPos = s->length - 1;

			/* Check if we already have a marker very close (±1 sample) */
			bool exists = false;
			for (int32_t i = 0; i < numSliceMarkers; i++)
			{
				if (ABS(sliceMarkers[i] - newPos) <= 1) { exists = true; break; }
			}

			if (!exists && numSliceMarkers < 16)
			{
				/* Allocate/expand marker array */
				int32_t *tmp = (int32_t *)realloc(sliceMarkers, sizeof (int32_t) * (numSliceMarkers + 1));
				if (tmp != NULL)
				{
					sliceMarkers = tmp;

					/* Insert in sorted order */
					int32_t idx = numSliceMarkers; // append
					while (idx > 0 && sliceMarkers[idx - 1] > newPos)
					{
						sliceMarkers[idx] = sliceMarkers[idx - 1];
						idx--;
					}
					sliceMarkers[idx] = newPos;
					numSliceMarkers++;
					sliceMarkersVisible = true;
					preserveMarkers = true;

					writeSample(true); // full redraw inc. markers
					return true; // event handled
				}
			}
		}
	}

	/* Otherwise, check if we clicked the marker handle/vertical line for dragging. */

	selectedSliceMarker = findSliceMarkerAtPosition(mouseX);
	if (selectedSliceMarker >= 0)
	{
		draggingSliceMarker = true;
		return true; // Consumed the mouse event
	}

	return false;
}

// Handle mouse drag for slice markers
void handleSliceMarkerMouseDrag(int32_t mouseX)
{
    if (!draggingSliceMarker || selectedSliceMarker < 0)
        return;

    // Convert screen position to sample position
    int32_t newSamplePos = scr2SmpPos(mouseX);

    // Clamp to sample bounds
    sample_t *s = getCurSample();
    if (s != NULL && s->dataPtr != NULL)
    {
        if (newSamplePos < 0) newSamplePos = 0;
        if (newSamplePos >= s->length) newSamplePos = s->length - 1;

        	// Update marker position
        	sliceMarkers[selectedSliceMarker] = newSamplePos;

        // Clamp to sample bounds and ensure we don't cross neighboring markers
        sample_t *s = getCurSample();
        if (s != NULL && s->dataPtr != NULL)
        {
            if (newSamplePos < 0) newSamplePos = 0;
            if (newSamplePos >= s->length) newSamplePos = s->length - 1;

            /* Prevent this marker from crossing its neighbors so that the
             * slice marker order (and thus pitch order) is always preserved.
             * We enforce that marker i must stay strictly greater than marker i-1
             * and strictly less than marker i+1 (if such neighbors exist).
             */

            int32_t minPos = 0;
            int32_t maxPos = s->length - 1;

            if (selectedSliceMarker > 0)
                minPos = sliceMarkers[selectedSliceMarker - 1] + 1; // stay ahead of previous

            if (selectedSliceMarker < numSliceMarkers - 1)
                maxPos = sliceMarkers[selectedSliceMarker + 1] - 1; // stay before next

            // Re-clamp against neighbor limits (handles edge cases where min>max)
            if (newSamplePos < minPos) newSamplePos = minPos;
            if (newSamplePos > maxPos) newSamplePos = maxPos;

            // Update marker position
            sliceMarkers[selectedSliceMarker] = newSamplePos;
        }

        // Redraw the border area below the waveform display
        const int32_t smpEdY = 174;
        const int32_t smpEdH = SAMPLE_AREA_HEIGHT;
        const int32_t borderY = smpEdY + smpEdH;

        // First wipe old border/handle rows completely
        clearRect(0, borderY, SCREEN_W, 4);          // border + handle area (4px)
        clearRect(0, smpEdY + smpEdH - 8, SCREEN_W, 8);          // numbers row

        // Redraw the slice markers
        drawSliceMarkers();

        // Update the display
        writeSample(true);
    }
}

// Handle mouse release for slice markers
void handleSliceMarkerMouseUp(void)
{
	draggingSliceMarker = false;
	selectedSliceMarker = -1;
}

// Handle delete key for slice markers
void handleSliceMarkerDelete(void)
{
	if (!sliceMarkersVisible || selectedSliceMarker < 0)
		return;

	removeSliceMarker(selectedSliceMarker);
	selectedSliceMarker = -1;
}

// Draw slice markers on the waveform
void drawSliceMarkers(void)
{
    // always refresh border/handle area to avoid ghosting
    const int32_t smpEdY  = 174;
    const int32_t smpEdH  = SAMPLE_AREA_HEIGHT;
    const int32_t borderY = smpEdY + smpEdH;
    const int32_t numberY = smpEdY + smpEdH - 8;

    /* First wipe old border/handle rows completely */
    clearRect(0, borderY, SCREEN_W, 4);          // border + handle area (4px)
    clearRect(0, numberY, SCREEN_W, 8);          // numbers row

    /* Re-draw full 4-pixel gradient border */
    hLine(0, borderY,     SCREEN_W, PAL_BCKGRND); // 1
    hLine(0, borderY + 1, SCREEN_W, PAL_PATTEXT); // 2
    hLine(0, borderY + 2, SCREEN_W, PAL_DESKTOP); // 3
    hLine(0, borderY + 3, SCREEN_W, PAL_DESKTOP); // 4

    // If no markers, we're done (area is clean)
    if (!sliceMarkersVisible || sliceMarkers == NULL || numSliceMarkers == 0)
        return;

    // -------------------------------------------------------
    char str[8];

    for (int32_t i = 0; i < numSliceMarkers; i++)
    {
        int32_t x = smpPos2Scr(sliceMarkers[i]);
        if (x < 0 || x >= SCREEN_W)
            continue;

        vLine(x, smpEdY, smpEdH, PAL_FORGRND); // vertical marker

        int32_t handleX = x - 2;
        if (handleX < 0) handleX = 0;
        if (handleX + 5 > SCREEN_W) handleX = SCREEN_W - 5;
        fillRect(handleX, borderY, 5, 4, PAL_FORGRND);   // handle (4-pixel height)

        if (i == selectedSliceMarker)
            hLine(handleX, borderY + 3, 5, PAL_BUTTON1); // highlight bottom edge

        sprintf(str, "%d", i + 1);
        int32_t textX = x + 4;
        if (textX + 8 > SCREEN_W - 16) textX = SCREEN_W - 24;
        textOut(textX, numberY, PAL_FORGRND, str);
    }
}

// Get slice markers visibility state
bool getSliceMarkersVisible(void)
{
    return sliceMarkersVisible;
}

// Get number of slice markers
int32_t getNumSliceMarkers(void)
{
    return numSliceMarkers;
}

// Get slice marker positions (for external access)
int32_t *getSliceMarkers(void)
{
    return sliceMarkers;
}

// Analyze sample for transients
static int32_t SDLCALL analyzeTransientsThread(void *ptr)
{
    (void)ptr;

    if (editor.curInstr == 0 || instr[editor.curInstr] == NULL)
    {
        mouseAnimOff();
        return 0;
    }

    sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];
    if (s->dataPtr == NULL)
    {
        mouseAnimOff();
        return 0;
    }

    // Clear existing markers
    freeSliceMarkers();

    // Calculate initial sensitivity threshold (0-100 to 0.0-1.0)
    // Higher sensitivity = lower threshold = more markers
    double threshold = 1.0 - ((double)slicer_Sensitivity / 100.0);
    if (threshold < 0.01) threshold = 0.01;
    if (threshold > 0.99) threshold = 0.99;

    // Find transients
    int32_t *markers = NULL;
    int32_t numMarkers = 0;
    const int32_t desiredMarkers = CLAMP(slicer_NumSlices, 1, 16);
    const int32_t maxMarkers = desiredMarkers; // enforce target count
    const int32_t minMarkers = desiredMarkers;
    int32_t minDistance = s->length / (desiredMarkers * 2); // Minimum distance between markers

    // Allocate temporary array for markers
    markers = (int32_t *)malloc(maxMarkers * sizeof (int32_t));
    if (markers == NULL)
    {
        outOfMemory = true;
        mouseAnimOff();
        return 0;
    }

    // Calculate RMS window size (smaller window = more precise transient detection)
    // Reduced from 1% to 0.3% of sample length for more precise placement
    const int32_t windowSize = s->length / 333; // ~0.3% of sample length
    const int32_t stepSize = windowSize / 4;    // 25% overlap between windows

    // Find transients using RMS analysis with dynamic threshold adjustment
    double lastRMS = 0.0;
    double currentRMS = 0.0;
    int32_t lastPeakPos = 0;
    bool inPeak = false;
    double dynamicThreshold = threshold;
    int32_t attempts = 0;
    const int32_t maxAttempts = 5; // Maximum number of threshold adjustment attempts

    while (attempts < maxAttempts)
    {
        numMarkers = 0;
        lastRMS = 0.0;
        currentRMS = 0.0;
        lastPeakPos = 0;
        inPeak = false;

        for (int32_t i = 0; i < s->length - windowSize; i += stepSize)
        {
            // Calculate RMS for current window
            currentRMS = calculateRMS(s->dataPtr, i, windowSize, !!(s->flags & SAMPLE_16BIT));

            // Detect rising edge (transient)
            if (!inPeak && currentRMS > lastRMS * (1.0 + dynamicThreshold))
            {
                inPeak = true;
                // Place marker slightly before the peak for better slicing
                lastPeakPos = i - (windowSize / 3);
                if (lastPeakPos < 0) lastPeakPos = 0;
            }
            // Detect falling edge
            else if (inPeak && currentRMS < lastRMS * (1.0 - dynamicThreshold))
            {
                inPeak = false;

                // Check if this peak is significant enough and far enough from previous marker
                if (numMarkers == 0 || (lastPeakPos - markers[numMarkers-1]) >= minDistance)
                {
                    if (numMarkers < maxMarkers)
                    {
                        markers[numMarkers++] = lastPeakPos;
                    }
                }
            }

            lastRMS = currentRMS;
        }

        // Adjust threshold if we don't have enough markers
        if (numMarkers < minMarkers)
        {
            // Lower threshold to get more markers
            dynamicThreshold *= 0.5;
            attempts++;
            continue;
        }
        // If we have too many markers, try to find the most significant ones
        else if (numMarkers > maxMarkers)
        {
            // Sort markers by RMS energy
            for (int32_t i = 0; i < numMarkers; i++)
            {
                double energy = calculateRMS(s->dataPtr, markers[i], windowSize, !!(s->flags & SAMPLE_16BIT));
                // Store energy in upper bits of marker position
                markers[i] = (markers[i] & 0x7FFFFFFF) | ((int32_t)(energy * 1000.0) << 31);
            }

            // Sort by energy (descending)
            qsort(markers, numMarkers, sizeof(int32_t), compareInt32);

            // Keep only the top maxMarkers
            numMarkers = maxMarkers;

            // Restore original positions
            for (int32_t i = 0; i < numMarkers; i++)
                markers[i] &= 0x7FFFFFFF;
        }

        break; // We have a good number of markers
    }

    // Create slice markers
    if (numMarkers > 0)
    {
        sliceMarkers = (int32_t *)malloc(numMarkers * sizeof (int32_t));
        if (sliceMarkers != NULL)
        {
            memcpy(sliceMarkers, markers, numMarkers * sizeof (int32_t));
            numSliceMarkers = numMarkers;
            sliceMarkersVisible = true;
        }
    }

    free(markers);
    mouseAnimOff();
    return 0;
}

static void pbAnalyzeTransients(void)
{
    if (editor.curInstr == 0 || instr[editor.curInstr] == NULL)
        return;

    sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];
    if (s->dataPtr == NULL)
        return;

    // Start analysis in a separate thread
    mouseAnimOn();
    SDL_Thread *thread = SDL_CreateThread(analyzeTransientsThread, "AnalyzeTransients", NULL);
    if (thread != NULL)
    {
        SDL_DetachThread(thread);
    }
    else
    {
        okBox(0, "System message", "Couldn't create thread!", NULL);
        mouseAnimOff();
    }
}

// Slice samples button callback
static int32_t SDLCALL sliceToSamplesThread(void *ptr)
{
    sample_t *srcSample = getCurSample();
    if (srcSample == NULL || srcSample->dataPtr == NULL)
    {
        setMouseBusy(false);
        return false;
    }

    instr_t *ins = instr[editor.curInstr];
    if (ins == NULL)
    {
        setMouseBusy(false);
        return false;
    }

    bool sample16Bit = !!(srcSample->flags & SAMPLE_16BIT);
    bool stereo = !!(srcSample->flags & SAMPLE_STEREO);

    // Create slices
    int32_t sliceCount = 0;
    for (int32_t i = 0; i < numSliceMarkers - 1 && sliceCount < MAX_SMP_PER_INST; i++)
    {
        int32_t sliceStart = sliceMarkers[i];
        int32_t sliceEnd = sliceMarkers[i + 1];
        int32_t sliceLen = sliceEnd - sliceStart;

        if (sliceLen <= 0)
            continue;

        // Find empty sample slot
        int32_t targetSmp = -1;
        for (int32_t j = 0; j < MAX_SMP_PER_INST; j++)
        {
            if (ins->smp[j].dataPtr == NULL || ins->smp[j].length == 0)
            {
                targetSmp = j;
                break;
            }
        }

        if (targetSmp == -1)
            break; // No more empty slots

        pauseAudio();

        // Allocate new sample
        sample_t *dstSample = &ins->smp[targetSmp];
        freeSample(editor.curInstr, targetSmp);

        if (!allocateSmpData(dstSample, sliceLen, sample16Bit, stereo))
        {
            resumeAudio();
            okBoxThreadSafe(0, "System message", "Not enough memory!", NULL);
            break;
        }

        // Copy sample data
        if (stereo) {
            if (sample16Bit) {
                int16_t *srcL = (int16_t *)srcSample->dataPtrL;
                int16_t *srcR = (int16_t *)srcSample->dataPtrR;
                int16_t *dstL = (int16_t *)dstSample->dataPtrL;
                int16_t *dstR = (int16_t *)dstSample->dataPtrR;
                memcpy(dstL, &srcL[sliceStart], sliceLen * sizeof(int16_t));
                memcpy(dstR, &srcR[sliceStart], sliceLen * sizeof(int16_t));
            } else {
                int8_t *srcL = (int8_t *)srcSample->dataPtrL;
                int8_t *srcR = (int8_t *)srcSample->dataPtrR;
                int8_t *dstL = (int8_t *)dstSample->dataPtrL;
                int8_t *dstR = (int8_t *)dstSample->dataPtrR;
                memcpy(dstL, &srcL[sliceStart], sliceLen);
                memcpy(dstR, &srcR[sliceStart], sliceLen);
            }
        } else {
            if (sample16Bit) {
                int16_t *src = (int16_t *)srcSample->dataPtrL;
                int16_t *dst = (int16_t *)dstSample->dataPtrL;
                memcpy(dst, &src[sliceStart], sliceLen * sizeof(int16_t));
            } else {
                int8_t *src = (int8_t *)srcSample->dataPtrL;
                int8_t *dst = (int8_t *)dstSample->dataPtrL;
                memcpy(dst, &src[sliceStart], sliceLen);
            }
        }

        // Copy sample properties
        dstSample->length = sliceLen;
        dstSample->volume = srcSample->volume;
        dstSample->panning = srcSample->panning;
        dstSample->finetune = srcSample->finetune;

        // Adjust relative note to compensate for keyboard position
        // Each slice needs to be pitched down by i semitones to maintain original pitch
        dstSample->relativeNote = srcSample->relativeNote - i;

        dstSample->flags = srcSample->flags & ~(LOOP_FWD | LOOP_BIDI); // No loop

        // Set sample name
        sprintf(dstSample->name, "Slice %02d", i + 1);

        // Map to keyboard starting from C-4
        if (i < 96) // Max notes we can map
        {
            ins->note2SampleLUT[48 + i] = targetSmp;
        }

        fixSample(dstSample);
        resumeAudio();

        sliceCount++;
    }

    editor.updateCurInstr = true;
    setSongModifiedFlag();

    // Clear markers and reset preservation state now that slicing is done
    freeSliceMarkers();
    preserveMarkers = false;

    setMouseBusy(false);

    // Close dialog after slicing
    ui.sysReqShown = false;

    return true;
    (void)ptr;
}

// Slice to samples callback
static void pbSliceToSamples(void)
{
    // Always allow slicing regardless of checkbox state
    if (numSliceMarkers < 2)
    {
        okBox(0, "System message", "Need at least 2 slice markers!", NULL);
        return;
    }

    mouseAnimOn();
    thread = SDL_CreateThread(sliceToSamplesThread, NULL, NULL);
    if (thread == NULL)
    {
        okBox(0, "System message", "Couldn't create thread!", NULL);
        mouseAnimOff();
        return;
    }
    SDL_DetachThread(thread);
}
// Setup slicer dialog widgets
void setupSlicerBoxWidgets(void)
{
    pushButton_t *p;
    scrollBar_t *s;
    checkBox_t *c;
    const int16_t x = 180;
    const int16_t y = 210;

    // Ensure dialog box stays within screen bounds
    if (x + 280 > SCREEN_W || y + 96 > SCREEN_H)
        return;

    // "Create markers" checkbox
    c = &checkBoxes[0];
    memset(c, 0, sizeof (checkBox_t));
    c->x = x + 20;
    c->y = y + 42;
    c->clickAreaWidth = 100;
    c->clickAreaHeight = 12;
    c->callbackFunc = cbSlicerCreateMarkers;
    c->checked = slicer_CreateMarkers ? CHECKBOX_CHECKED : CHECKBOX_UNCHECKED;
    c->visible = true;

    // "Slice to samples" checkbox
    c = &checkBoxes[1];
    memset(c, 0, sizeof (checkBox_t));
    c->x = x + 20;
    c->y = y + 58;
    c->clickAreaWidth = 110;
    c->clickAreaHeight = 12;
    c->callbackFunc = cbSlicerSliceToSamples;
    c->checked = slicer_SliceToSamples ? CHECKBOX_CHECKED : CHECKBOX_UNCHECKED;
    c->visible = true;

    // "Analyze" pushbutton
    // p = &pushButtons[0];
    // memset(p, 0, sizeof (pushButton_t));
    // p->caption = "Analyze";
    // p->x = x + 145;
    // p->y = y + 38;
    // p->w = 60;
    // p->h = 16;
    // p->callbackFuncOnUp = pbAnalyzeTransients;
    // p->visible = true;

    // "Process" pushbutton
    p = &pushButtons[1];
    memset(p, 0, sizeof (pushButton_t));
    p->caption = "Process";
    p->x = x + 215;
    p->y = y + 56;
    p->w = 60;
    p->h = 16;
    p->callbackFuncOnUp = pbSliceToSamples;
    p->visible = true;

    // "Close" pushbutton
    p = &pushButtons[2];
    memset(p, 0, sizeof (pushButton_t));
    p->caption = "Close";
    p->x = x + 215;
    p->y = y + 74;
    p->w = 60;
    p->h = 16;
    p->callbackFuncOnUp = pbExit;
    p->visible = true;

    // scrollbar buttons
    p = &pushButtons[3];
    memset(p, 0, sizeof (pushButton_t));
    p->caption = ARROW_LEFT_STRING;
    p->x = x + 116;
    p->y = y + 22;
    p->w = 23;
    p->h = 13;
    p->preDelay = 1;
    p->delayFrames = 3;
    p->callbackFuncOnDown = pbNumSlicesDown;
    p->visible = true;

    p = &pushButtons[4];
    memset(p, 0, sizeof (pushButton_t));
    p->caption = ARROW_RIGHT_STRING;
    p->x = x + 252;
    p->y = y + 22;
    p->w = 23;
    p->h = 13;
    p->preDelay = 1;
    p->delayFrames = 3;
    p->callbackFuncOnDown = pbNumSlicesUp;
    p->visible = true;

    // sensitivity scrollbar
    s = &scrollBars[0];
    memset(s, 0, sizeof (scrollBar_t));
    s->x = x + 139;
    s->y = y + 22;
    s->w = 113;
    s->h = 13;
    s->callbackFunc = sbSetNumSlicesPos;
    s->visible = true;
    setScrollBarPageLength(0, 1);
    setScrollBarEnd(0, 16);
    setScrollBarPos(0, slicer_NumSlices, false);
}

// Draw the slicer dialog box
static void drawSlicerBox(void)
{
    const int16_t x = 180;
    const int16_t y = 210;
    const int16_t w = 280;
    const int16_t h = 96;

    // Calculate dialog position
    int16_t dialogX = x;
    int16_t dialogY = y;

    // If dialog would be outside screen, center it
    if (x < 0 || y < 0 || x + w > SCREEN_W || y + h > SCREEN_H)
    {
        dialogX = (SCREEN_W - w) / 2;
        dialogY = (SCREEN_H - h) / 2;
    }

    // main fill
    fillRect(dialogX + 1, dialogY + 1, w - 2, h - 2, PAL_BUTTONS);

    // outer border
    vLine(dialogX,         dialogY,         h - 1, PAL_BUTTON1);
    hLine(dialogX + 1,     dialogY,         w - 2, PAL_BUTTON1);
    vLine(dialogX + w - 1, dialogY,         h,     PAL_BUTTON2);
    hLine(dialogX,         dialogY + h - 1, w - 1, PAL_BUTTON2);

    // inner border
    vLine(dialogX + 2,     dialogY + 2,     h - 5, PAL_BUTTON2);
    hLine(dialogX + 3,     dialogY + 2,     w - 6, PAL_BUTTON2);
    vLine(dialogX + w - 3, dialogY + 2,     h - 4, PAL_BUTTON1);
    hLine(dialogX + 2,     dialogY + h - 3, w - 4, PAL_BUTTON1);

    // Draw title and labels
    textOutShadow(dialogX + 6, dialogY + 6, PAL_FORGRND, PAL_BUTTON2, "Sample Chop");
    textOutShadow(dialogX + 6, dialogY + 24, PAL_FORGRND, PAL_BUTTON2, "Chops");
    textOutShadow(dialogX + 40, dialogY + 42, PAL_FORGRND, PAL_BUTTON2, "Keep Markers");
    // textOutShadow(dialogX + 20, dialogY + 58, PAL_FORGRND, PAL_BUTTON2, "Slice to samples");

	// Display slice count if markers are visible
//    if (sliceMarkersVisible)
//    {
//        char sliceText[16];
//        sprintf(sliceText, "Slices: %d", numSliceMarkers);
//        textOut(dialogX + 20, dialogY + 74, PAL_FORGRND, sliceText);
//    }

    // Display current slice count value
    char slicesText[8];
    sprintf(slicesText, "%d", slicer_NumSlices);
    textOut(dialogX + 85, dialogY + 24, PAL_FORGRND, slicesText);
}

// Main slicer dialog function
void pbSampleSlicer(void)
{
    if (editor.curInstr == 0 ||
        instr[editor.curInstr] == NULL ||
        instr[editor.curInstr]->smp[editor.curSmp].dataPtr == NULL)
    {
        return;
    }

    // Save current state
    bool oldMarkersVisible = sliceMarkersVisible;

    setupSlicerBoxWidgets();
    windowOpen();

    // Perform initial slice detection if needed
    if (!sliceMarkersVisible || !preserveMarkers)
    {
        preserveMarkers = false;
        freeSliceMarkers();
        analyzeTransients();
    }

    // Sync displayed slice count with current markers if we are preserving them
    if (sliceMarkersVisible)
        slicer_NumSlices = CLAMP(numSliceMarkers, 1, 16);

    outOfMemory = false;
    exitFlag = false;

    // Show the dialog box
    ui.sysReqShown = true;
    ui.sysReqEnterPressed = false;

    while (ui.sysReqShown)
    {
        readInput();
        if (ui.sysReqEnterPressed)
            pbAnalyzeTransients();

        setSyncedReplayerVars();
        handleRedrawing();

        // Draw slice markers first (behind the dialog)
        if (sliceMarkersVisible)
            drawSliceMarkers();

        // Draw the slicer dialog box
        drawSlicerBox();

        // Update scrollbar position
        setScrollBarPos(0, slicer_NumSlices, false);

        // Draw all UI elements
        drawCheckBox(0);
        // drawCheckBox(1);
        for (uint16_t i = 0; i < 5; i++)
            drawPushButton(i);
        drawScrollBar(0);

        flipFrame();
    }

    // Hide all UI elements
    hideCheckBox(0);
    hideCheckBox(1);
    for (uint16_t i = 0; i < 5; i++)
        hidePushButton(i);
    hideScrollBar(0);

    // Decide whether to preserve markers for next session
    if (!slicer_CreateMarkers && sliceMarkersVisible)
    {
        // user chose not to keep markers
        freeSliceMarkers();
        writeSample(true);
        preserveMarkers = false;
    }
    else
    {
        preserveMarkers = sliceMarkersVisible;
    }

    windowClose(true);

    if (outOfMemory)
        okBox(0, "System message", "Not enough memory!", NULL);
}

static void analyzeTransients(void)
{
    if (editor.curInstr == 0 || instr[editor.curInstr] == NULL)
        return;

    sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];
    if (s->dataPtr == NULL)
        return;

    // Clear existing markers
    freeSliceMarkers();

    // Calculate initial sensitivity threshold (0-100 to 0.0-1.0)
    // Higher sensitivity = lower threshold = more markers
    double threshold = 1.0 - ((double)slicer_Sensitivity / 100.0);
    if (threshold < 0.01) threshold = 0.01;
    if (threshold > 0.99) threshold = 0.99;

    // Find transients
    int32_t *markers = NULL;
    int32_t numMarkers = 0;
    const int32_t desiredMarkers = CLAMP(slicer_NumSlices, 1, 16);
    const int32_t maxMarkers = desiredMarkers; // enforce target count
    const int32_t minMarkers = desiredMarkers;
    int32_t minDistance = s->length / (desiredMarkers * 2); // Minimum distance between markers

    // Allocate temporary array for markers
    markers = (int32_t *)malloc(maxMarkers * sizeof (int32_t));
    if (markers == NULL)
    {
        outOfMemory = true;
        return;
    }

    // Calculate RMS window size (smaller window = more precise transient detection)
    // Reduced from 1% to 0.3% of sample length for more precise placement
    const int32_t windowSize = s->length / 333; // ~0.3% of sample length
    const int32_t stepSize = windowSize / 4;    // 25% overlap between windows

    // Find transients using RMS analysis with dynamic threshold adjustment
    double lastRMS = 0.0;
    double currentRMS = 0.0;
    int32_t lastPeakPos = 0;
    bool inPeak = false;
    double dynamicThreshold = threshold;
    int32_t attempts = 0;
    const int32_t maxAttempts = 5; // Maximum number of threshold adjustment attempts

    while (attempts < maxAttempts)
    {
        numMarkers = 0;
        lastRMS = 0.0;
        currentRMS = 0.0;
        lastPeakPos = 0;
        inPeak = false;

        for (int32_t i = 0; i < s->length - windowSize; i += stepSize)
        {
            // Calculate RMS for current window
            currentRMS = calculateRMS(s->dataPtr, i, windowSize, !!(s->flags & SAMPLE_16BIT));

            // Detect rising edge (transient)
            if (!inPeak && currentRMS > lastRMS * (1.0 + dynamicThreshold))
            {
                inPeak = true;
                // Place marker slightly before the peak for better slicing
                lastPeakPos = i - (windowSize / 3);
                if (lastPeakPos < 0) lastPeakPos = 0;
            }
            // Detect falling edge
            else if (inPeak && currentRMS < lastRMS * (1.0 - dynamicThreshold))
            {
                inPeak = false;

                // Check if this peak is significant enough and far enough from previous marker
                if (numMarkers == 0 || (lastPeakPos - markers[numMarkers-1]) >= minDistance)
                {
                    if (numMarkers < maxMarkers)
                    {
                        markers[numMarkers++] = lastPeakPos;
                    }
                }
            }

            lastRMS = currentRMS;
        }

        // Adjust threshold if we don't have enough markers
        if (numMarkers < minMarkers)
        {
            // Lower threshold to get more markers
            dynamicThreshold *= 0.5;
            attempts++;
            continue;
        }
        // If we have too many markers, try to find the most significant ones
        else if (numMarkers > maxMarkers)
        {
            // Sort markers by RMS energy
            for (int32_t i = 0; i < numMarkers; i++)
            {
                double energy = calculateRMS(s->dataPtr, markers[i], windowSize, !!(s->flags & SAMPLE_16BIT));
                // Store energy in upper bits of marker position
                markers[i] = (markers[i] & 0x7FFFFFFF) | ((int32_t)(energy * 1000.0) << 31);
            }

            // Sort by energy (descending)
            qsort(markers, numMarkers, sizeof(int32_t), compareInt32);

            // Keep only the top maxMarkers
            numMarkers = maxMarkers;

            // Restore original positions
            for (int32_t i = 0; i < numMarkers; i++)
                markers[i] &= 0x7FFFFFFF;
        }

        break; // We have a good number of markers
    }

    // Create slice markers
    if (numMarkers > 0)
    {
        sliceMarkers = (int32_t *)malloc(numMarkers * sizeof (int32_t));
        if (sliceMarkers != NULL)
        {
            memcpy(sliceMarkers, markers, numMarkers * sizeof (int32_t));
            numSliceMarkers = numMarkers;
            sliceMarkersVisible = true;
        }
    }

    free(markers);
}

static void sliceToSamples(void)
{
    if (editor.curInstr == 0 || instr[editor.curInstr] == NULL)
        return;

    sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];
    if (s->dataPtr == NULL || numSliceMarkers == 0)
        return;

    // Sort markers by position
    qsort(sliceMarkers, numSliceMarkers, sizeof (int32_t), compareInt32);

    // Create slices
    int32_t i;
    for (i = 0; i < numSliceMarkers; i++)
    {
        int32_t start = (i == 0) ? 0 : sliceMarkers[i-1];
        int32_t end = sliceMarkers[i];
        int32_t length = end - start;

        // Create new sample
        sample_t *newSmp = &instr[editor.curInstr]->smp[i+1];
        newSmp->length = length;
        newSmp->loopStart = 0;
        newSmp->loopLength = 0;
        newSmp->volume = s->volume;
        newSmp->finetune = s->finetune;
        newSmp->flags = SAMPLE_16BIT;
        newSmp->panning = s->panning;
        newSmp->relativeNote = s->relativeNote;

        // Allocate memory for new sample
        newSmp->dataPtr = (int8_t *)malloc(length * sizeof (int16_t));
        if (newSmp->dataPtr == NULL)
        {
            outOfMemory = true;
            return;
        }

        // Copy sample data
        int16_t *src16 = (int16_t *)s->dataPtr;
        int16_t *dst16 = (int16_t *)newSmp->dataPtr;
        memcpy(dst16, &src16[start], length * sizeof (int16_t));

        // Update sample name
        char name[23];
        sprintf(name, "Slice %d", i+1);
        strncpy(newSmp->name, name, 22);
        newSmp->name[22] = '\0';

        // Update note mapping
        instr[editor.curInstr]->note2SampleLUT[i] = i+1;
    }

    // Clear slice markers
    freeSliceMarkers();
    sliceMarkersVisible = false;
    preserveMarkers = false; // reset preserve flag when markers are cleared

    // Update UI
    updateSampleEditor();
}

// --------------------------------------------------------------------
// New slice count widgets (1..16)

static void updateSliceMarkersAfterCountChange(void)
{
    // when user changes desired slice count we invalidate preserved markers
    preserveMarkers = false;
    freeSliceMarkers();

    // Re-run analysis with new target count
    analyzeTransients();

    // redraw waveform to reflect new markers
    writeSample(true);
}

static void sbSetNumSlicesPos(uint32_t pos)
{
    int16_t newVal = (int16_t)pos;
    if (newVal < 1) newVal = 1;
    if (newVal > 16) newVal = 16;

    if (slicer_NumSlices != newVal)
    {
        slicer_NumSlices = newVal;
        updateSliceMarkersAfterCountChange();
    }
}

static void pbNumSlicesDown(void)
{
    if (slicer_NumSlices > 1)
    {
        slicer_NumSlices--;
        updateSliceMarkersAfterCountChange();
    }
}

static void pbNumSlicesUp(void)
{
    if (slicer_NumSlices < 16)
    {
        slicer_NumSlices++;
        updateSliceMarkersAfterCountChange();
    }
}

// Play the slice corresponding to marker index (from marker to next marker/end)
static void previewSlice(int32_t markerIndex)
{
    if (!sliceMarkersVisible || markerIndex < 0 || markerIndex >= numSliceMarkers)
        return;

    sample_t *s = getCurSample();
    if (s == NULL || s->dataPtr == NULL)
        return;

    int32_t startPos = sliceMarkers[markerIndex];
    int32_t endPos;

    if (markerIndex < numSliceMarkers - 1)
        endPos = sliceMarkers[markerIndex + 1];
    else
        endPos = s->length;

    if (endPos <= startPos)
        return;

    /* Use same helper as other preview routines – plays on current sample edit
     * note (editor.smpEd_NoteNr) and current channel, full volume.
     */
    playRange(cursor.ch, editor.curInstr, editor.curSmp,
              editor.smpEd_NoteNr, 0, 0, startPos, endPos - startPos);
}
