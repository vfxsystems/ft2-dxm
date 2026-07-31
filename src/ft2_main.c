// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h> // modf()
#include <string.h>
#ifdef _WIN32
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#include <SDL2/SDL_syswm.h>
#else
#include <unistd.h> // chdir()
#endif
#include "ft2_header.h"
#include "ft2_v2.h"
#include "ft2_v2_preparations.h"
#include "ft2_gui.h"
#include "ft2_video.h"
#include "ft2_audio.h"
#include "ft2_mouse.h"
#include "ft2_keyboard.h"
#include "ft2_config.h"
#include "ft2_sample_ed.h"
#include "ft2_diskop.h"
#include "scopes/ft2_scopes.h"
#include "scopes/ft2_scopedraw.h"
#include "ft2_about.h"
#include "ft2_pattern_ed.h"
#include "ft2_module_loader.h"
#include "ft2_sampling.h"
#include "ft2_audioselector.h"
#include "ft2_help.h"
#include "ft2_midi.h"
#include "ft2_events.h"
#include "ft2_bmp.h"
#include "ft2_structs.h"
#include "ft2_hpc.h"
#include "ft2_smpfx.h"
#include "ft2_mixer.h"
#include "ft2_macro_map.h"
#include "ft2_ui_render.h"
#include "mixer/ft2_quadratic_spline.h"
#include "mixer/ft2_cubic_spline.h"
#include "mixer/ft2_windowed_sinc.h"

static void initializeVars(void);
static bool runSelfTest(void);
static bool runV2StressTest(void);
static void cleanUpAndExit(void); // never call this inside the main loop
#ifdef __APPLE__
static void osxSetDirToProgramDirFromArgs(char **argv);
#endif

#ifdef _WIN32
static void disableWasapi(void);
#endif

int main(int argc, char *argv[])
{
#if defined _WIN32 || defined __APPLE__
	SDL_version sdlVer;
#endif
	bool debugConsole = false;
	bool debugSynthRouting = false;

	// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--version") == 0)
		{
			printf("ft2-dxm %s\n", PROG_VER_STR);
			return 0;
		}
		else if (strcmp(argv[i], "--self-test") == 0)
		{
			return runSelfTest() ? 0 : 1;
		}
		else if (strcmp(argv[i], "--v2-stress-test") == 0)
		{
			return runV2StressTest() ? 0 : 1;
		}
		else
		if (strcmp(argv[i], "--debug") == 0)
		{
			debugConsole = true;
			break;
		}
		else if (strcmp(argv[i], "--debug-synth-routing") == 0)
		{
			debugSynthRouting = true;
			debugConsole = true;
		}
	}

	if (!debugConsole)
	{
#ifdef _WIN32
		freopen("NUL", "w", stdout);
		freopen("NUL", "w", stderr);
#else
		freopen("/dev/null", "w", stdout);
		freopen("/dev/null", "w", stderr);
#endif
	}

#if SDL_MAJOR_VERSION == 2 && SDL_MINOR_VERSION == 0 && SDL_PATCHLEVEL < 5
#pragma message("WARNING: The SDL2 dev lib is older than ver 2.0.5. You'll get fullscreen mode issues and no audio input sampling.")
#pragma message("At least version 2.0.7 is recommended.")
#endif

	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
	SDL_EnableScreenSaver(); // allow screensaver to activate

	initializeVars();
	if (debugSynthRouting)
		audioSetSynthRoutingDebug(true);
	setupCrashHandler();
	
	// TODO: Initialize v2 preparations
	// ft2_v2_initialize();
	// ft2_v2_apply_optimizations();

	// on Windows and macOS, test what version SDL2.DLL is (against library version used in compilation)
#if defined _WIN32 || defined __APPLE__
	SDL_GetVersion(&sdlVer);
	if (sdlVer.major != SDL_MAJOR_VERSION || sdlVer.minor != SDL_MINOR_VERSION || sdlVer.patch != SDL_PATCHLEVEL)
	{
#ifdef _WIN32
		showErrorMsgBox("SDL2.dll is not the expected version, the program will terminate.\n\n" \
		                "Loaded dll version: %d.%d.%d\n" \
		                "Required (compiled with) version: %d.%d.%d\n\n",
		                sdlVer.major, sdlVer.minor, sdlVer.patch,
		                SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
#else
		showErrorMsgBox("The loaded SDL2 library is not the expected version, the program will terminate.\n\n" \
		                "Loaded library version: %d.%d.%d\n" \
		                "Required (compiled with) version: %d.%d.%d",
		                sdlVer.major, sdlVer.minor, sdlVer.patch,
		                SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
#endif
		return 0;
	}
#endif

	// ALT+F4 is used in FT2, but is "close program" in some cases...
#if SDL_MINOR_VERSION >= 24 || (SDL_MINOR_VERSION == 0 && SDL_PATCHLEVEL >= 4)
	SDL_SetHint("SDL_WINDOWS_NO_CLOSE_ON_ALT_F4", "1");
#endif

#ifdef _WIN32
#ifndef _MSC_VER
	SetProcessDPIAware();
#endif

	if (!cpu.hasSSE)
	{
		showErrorMsgBox("Your computer's processor doesn't have the SSE instruction set\n" \
		                "which is needed for this program to run. Sorry!");
		return 0;
	}

	if (!cpu.hasSSE2)
	{
		showErrorMsgBox("Your computer's processor doesn't have the SSE2 instruction set\n" \
		                "which is needed for this program to run. Sorry!");
		return 0;
	}

	disableWasapi(); // disable problematic WASAPI SDL2 audio driver on Windows (causes clicks/pops sometimes...)
	                 // 13.03.2020: This is still needed with SDL 2.0.12...
#endif

	/* SDL 2.0.9 for Windows has a serious bug where you need to initialize the joystick subsystem
	** (even if you don't use it) or else weird things happen like random stutters, keyboard (rarely) being
	** reinitialized in Windows and what not.
	** Ref.: https://bugzilla.libsdl.org/show_bug.cgi?id=4391
	*/
#if defined _WIN32 && SDL_MAJOR_VERSION == 2 && SDL_MINOR_VERSION == 0 && SDL_PATCHLEVEL == 9
	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0)
#else
	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) != 0)
#endif
	{
		showErrorMsgBox("Couldn't initialize SDL:\n%s", SDL_GetError());
		return 1;
	}

	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

	/* Text input is started by default in SDL2, turn it off to remove ~2ms spikes per key press.
	** We manuallay start it again when a text edit box is activated, and stop it when done.
	** Ref.: https://bugzilla.libsdl.org/show_bug.cgi?id=4166
	*/
	SDL_StopTextInput();

	hpc_Init();
	hpc_SetDurationInHz(&video.vblankHpc, VBLANK_HZ);

#ifdef __APPLE__
	osxSetDirToProgramDirFromArgs(argv);
#endif
	if (!setupExecutablePath() || !loadBMPs() || !setupQuadraticSplineTable() || !setupCubicSplineTable() || !setupWindowedSincTables())
	{
		cleanUpAndExit();
		return 1;
	}

	loadConfigOrSetDefaults(); // config must be loaded at this exact point

	if (!setupWindow() || !setupRenderer())
	{
		// error message was shown in the functions above
		cleanUpAndExit();
		return 1;
	}

#ifdef _WIN32
	// allow only one instance, and send arguments to it (what song to play)
	if (handleSingleInstancing(argc, argv))
	{
		cleanUpAndExit();
		return 0; // close current instance, the main instance got a message now
	}
#endif

	if (!setupDiskOp())
	{
		cleanUpAndExit();
		return 1;
	}

	audio.currOutputDevice = getAudioOutputDeviceFromConfig();
	audio.currInputDevice = getAudioInputDeviceFromConfig();

	if (!setupAudio(CONFIG_HIDE_ERRORS)) // can we open the audio device?
	{
		// nope, try with the default audio device
		setToDefaultAudioOutputDevice();

		if (!setupAudio(CONFIG_HIDE_ERRORS)) // does it work this time?
		{
			// nope, try safe values (44.1kHz 16-bit @ 1024 samples)
			config.audioFreq = 44100;
			config.specialFlags &= ~(BITDEPTH_32 + BUFFSIZE_512 + BUFFSIZE_2048);
			config.specialFlags |=  (BITDEPTH_16 + BUFFSIZE_1024);

			if (!setupAudio(CONFIG_SHOW_ERRORS)) // this time it surely must work?!
			{
				cleanUpAndExit(); // well, nope!
				return 1;
			}
		}
	}

	if (!setupReplayer() || !setupGUI() || !initScopes())
	{
		cleanUpAndExit();
		return 1;
	}

	// Initialize FT2 GUI system (no external GUI toolkit needed)
	SDL_Surface* windowSurface = SDL_GetWindowSurface(video.window);
	if (!windowSurface)
	{
		// Surface creation failed, but this is not critical for FT2's core functionality
		printf("Warning: Failed to get window surface - using default rendering\n");
	}

	pauseAudio();
	resumeAudio();
	rescanAudioDevices();

#ifdef _WIN32 // on Windows we show the window at this point
	SDL_ShowWindow(video.window);
#endif

	if (config.windowFlags & START_IN_FULLSCR)
	{
		video.fullscreen = true;
		enterFullscreen();
	}

#ifdef HAS_MIDI
#ifdef __APPLE__
	// MIDI init can take several seconds on Mac, use thread
	midi.initMidiThread = SDL_CreateThread(initMidiFunc, NULL, NULL);
	if (midi.initMidiThread == NULL)
	{
		showErrorMsgBox("Couldn't create MIDI initialization thread!");
		cleanUpAndExit();
		return 1;
	}
#else
	initMidiFunc(NULL);
#endif
#endif

	hpc_ResetCounters(&video.vblankHpc); // quirk: this is needed for potential okBox() calls in handleModuleLoadFromArg()
	handleModuleLoadFromArg(argc, argv);

	editor.mainLoopOngoing = true;
	hpc_ResetCounters(&video.vblankHpc); // this must be the last thing we do before entering the main loop

	while (editor.programRunning)
	{
		beginFPSCounter();
		handleThreadEvents();
		readInput();
		handleEvents();
		
		// Handle macro UI sync requests from audio thread
		handleMacroUiSync();
		handleDspUiSync();
		
		handleRedrawing();
		flipFrame();
		endFPSCounter();

		if (mouse.leftButtonPressed)
			handlePushButtonsWhileMouseDown();
	}

	if (config.cfg_AutoSave)
		saveConfig(CONFIG_HIDE_ERRORS);

	cleanUpAndExit();
	return 0;
}

static void initializeVars(void)
{
	cpu.hasSSE = SDL_HasSSE();
	cpu.hasSSE2 = SDL_HasSSE2();

	// clear common structs
#ifdef HAS_MIDI
	memset(&midi, 0, sizeof (midi));
#endif
	memset(&video, 0, sizeof (video));
	memset(&keyb, 0, sizeof (keyb));
	memset(&mouse, 0, sizeof (mouse));
	memset(&editor, 0, sizeof (editor));
	memset((void *)&pattMark, 0, sizeof (pattMark));
	memset(&pattSync, 0, sizeof (pattSync));
	memset(&chSync, 0, sizeof (chSync));
	memset(&song, 0, sizeof (song));

	// used for scopes and sampling position line (sampler screen)
	for (int32_t i = 0; i < MAX_CHANNELS; i++)
	{
		lastChInstr[i].instrNum = 255;
		lastChInstr[i].smpNum = 255;
	}

	// now set data that must be initialized to non-zero values...

	audio.locked = true; // XXX: Why..?
	audio.rescanAudioDevicesSupported = true;

	// set non-zero values

	editor.moduleSaveMode = MOD_SAVE_MODE_XM;
	editor.sampleSaveMode = SMP_SAVE_MODE_WAV;

	ui.sampleDataOrLoopDrag = -1;

	mouse.lastUsedObjectID = OBJECT_ID_NONE;

	editor.editRowSkip = 1;
	editor.srcInstr = 1;
	editor.curInstr = 1;
	editor.curOctave = 4;
	editor.smpEd_NoteNr = 1+NOTE_C4;

	editor.ptnJumpPos[0] = 0x00;
	editor.ptnJumpPos[1] = 0x10;
	editor.ptnJumpPos[2] = 0x20;
	editor.ptnJumpPos[3] = 0x30;

	editor.copyMaskEnable = true;
	memset(editor.copyMask, 1, sizeof (editor.copyMask));
	memset(editor.pasteMask, 1, sizeof (editor.pasteMask));

	editor.diskOpReadOnOpen = true;

	audio.linearPeriodsFlag = true;
	calcReplayerLogTab();

#ifdef HAS_MIDI
	midi.enable = true;
#endif

	editor.programRunning = true;

	/* initialize per-channel mixer defaults */
	mixerInit();
}

static bool runSelfTest(void)
{
	char macroErr[128];

	initializeVars();

	if (!ft2_macro_map_self_test(macroErr, sizeof (macroErr)))
	{
		fprintf(stderr, "self-test: %s\n", macroErr);
		return false;
	}

	if (!ft2_ui_render_self_test(macroErr, sizeof (macroErr)))
	{
		fprintf(stderr, "self-test: %s\n", macroErr);
		return false;
	}

	if (!setupQuadraticSplineTable())
	{
		fprintf(stderr, "self-test: setupQuadraticSplineTable() failed\n");
		return false;
	}

	if (!setupCubicSplineTable())
	{
		fprintf(stderr, "self-test: setupCubicSplineTable() failed\n");
		freeQuadraticSplineTable();
		return false;
	}

	if (!setupWindowedSincTables())
	{
		fprintf(stderr, "self-test: setupWindowedSincTables() failed\n");
		freeCubicSplineTable();
		freeQuadraticSplineTable();
		return false;
	}

	freeWindowedSincTables();
	freeCubicSplineTable();
	freeQuadraticSplineTable();

	printf("ft2-dxm self-test passed\n");
	return true;
}

static bool runV2StressTest(void)
{
	float left[256], right[256];
	uint8_t *patch = NULL, *blob = NULL;
	bool ok = false;

	initializeVars();
	editor.curInstr = 1;

	if (!allocateInstr(editor.curInstr))
	{
		fprintf(stderr, "v2-stress-test: allocateInstr() failed\n");
		return false;
	}

	instr[editor.curInstr]->useV2 = true;
	ft2_v2_init(44100);

	const int patchSize = ft2_v2_get_patch_size();
	if (patchSize <= 0)
	{
		fprintf(stderr, "v2-stress-test: invalid V2 patch size\n");
		goto cleanup;
	}

	patch = (uint8_t *)malloc((size_t)patchSize);
	if (patch == NULL)
	{
		fprintf(stderr, "v2-stress-test: patch allocation failed\n");
		goto cleanup;
	}

	for (int i = 0; i < 128; i++)
	{
		const uint8_t note = (uint8_t)(48 + (i % 24));

		memset(left, 0, sizeof (left));
		memset(right, 0, sizeof (right));

		if (!ft2_v2_load_factory_preset_for_instrument(editor.curInstr, i))
		{
			fprintf(stderr, "v2-stress-test: factory preset %d failed\n", i);
			goto cleanup;
		}

		ft2_v2_send_midi_to_instrument(editor.curInstr, 0x90, note, 100);
		ft2_v2_render_for_channel(editor.curInstr, left, right, 128, 0);

		if (ft2_v2_get_patch_data(editor.curInstr, patch, patchSize) != patchSize)
		{
			fprintf(stderr, "v2-stress-test: patch export failed at preset %d\n", i);
			goto cleanup;
		}

		ft2_v2_set_mod_count_for_instrument(editor.curInstr, 255);
		ft2_v2_set_mod_slot_for_instrument(editor.curInstr, 254, 255, 255, 255);
		if (!ft2_v2_load_patch_for_instrument(editor.curInstr, patch, (size_t)patchSize))
		{
			fprintf(stderr, "v2-stress-test: patch reload failed at preset %d\n", i);
			goto cleanup;
		}

		ft2_v2_send_midi_to_instrument(editor.curInstr, 0x80, note, 0);
		ft2_v2_render_for_channel(editor.curInstr, left, right, 128, 0);
	}

	const size_t blobSize = ft2_v2_serialize_state(editor.curInstr, NULL, 0);
	if (blobSize == 0)
	{
		fprintf(stderr, "v2-stress-test: state size query failed\n");
		goto cleanup;
	}

	blob = (uint8_t *)malloc(blobSize);
	if (blob == NULL)
	{
		fprintf(stderr, "v2-stress-test: state allocation failed\n");
		goto cleanup;
	}

	if (ft2_v2_serialize_state(editor.curInstr, blob, blobSize) != blobSize)
	{
		fprintf(stderr, "v2-stress-test: state serialization failed\n");
		goto cleanup;
	}

	if (!ft2_v2_deserialize_state(editor.curInstr, blob, blobSize))
	{
		fprintf(stderr, "v2-stress-test: state deserialization failed\n");
		goto cleanup;
	}

	memset(left, 0, sizeof (left));
	memset(right, 0, sizeof (right));
	ft2_v2_send_midi_to_instrument(editor.curInstr, 0x90, 60, 100);
	ft2_v2_render_for_channel(editor.curInstr, left, right, 256, 0);
	ft2_v2_panic();
	ft2_v2_render_for_channel(editor.curInstr, left, right, 256, 0);

	if (ft2_v2_get_active_voice_count(editor.curInstr) != 0)
	{
		fprintf(stderr, "v2-stress-test: panic left active voices\n");
		goto cleanup;
	}

	ok = true;

cleanup:
	free(blob);
	free(patch);
	ft2_v2_clear_persistent_state(editor.curInstr);
	ft2_v2_shutdown();
	freeInstr(editor.curInstr);

	if (ok)
		printf("ft2-dxm V2 stress test passed\n");

	return ok;
}

static void cleanUpAndExit(void) // never call this inside the main loop!
{
#ifdef HAS_MIDI
#ifdef __APPLE__
	// on Mac we used a thread to init MIDI (as it could take several seconds)
	if (midi.initMidiThread != NULL)
	{
		SDL_WaitThread(midi.initMidiThread, NULL);
		midi.initMidiThread = NULL;
	}
#endif
	midi.enable = false; // stop MIDI callback from doing things
	while (midi.callbackBusy) SDL_Delay(1); // wait for MIDI callback to finish

	closeMidiInDevice();
	freeMidiIn();
	freeMidiInputDeviceList();

	if (midi.inputDeviceName != NULL)
	{
		free(midi.inputDeviceName);
		midi.inputDeviceName = NULL;
	}

	if (editor.midiConfigFileLocationU != NULL)
	{
		free(editor.midiConfigFileLocationU);
		editor.midiConfigFileLocationU = NULL;
	}
#endif

	closeAudio();
	closeReplayer();
	closeVideo();
	// No external GUI toolkit cleanup needed
	freeSprites();
	freeDiskOp();
	clearCopyBuffer();
	clearSampleUndo();
	freeAudioDeviceSelectorBuffers();
	windUpFTHelp();
	freeTextBoxes();
	freeMouseCursors();
	freeBMPs();
	freeScopeIntrpLUT();

	if (editor.audioDevConfigFileLocationU != NULL)
	{
		free(editor.audioDevConfigFileLocationU);
		editor.audioDevConfigFileLocationU = NULL;
	}

	if (editor.configFileLocationU != NULL)
	{
		free(editor.configFileLocationU);
		editor.configFileLocationU = NULL;
	}

	if (editor.binaryPathU != NULL)
	{
		free(editor.binaryPathU);
		editor.binaryPathU = NULL;
	}

#ifdef _WIN32
	closeSingleInstancing();
#endif

	SDL_Quit();
}

#ifdef __APPLE__
static void osxSetDirToProgramDirFromArgs(char **argv)
{
	/* OS X/macOS: hackish way of setting the current working directory to the place where we double clicked
	** on the icon (for FT2.CFG loading)
	*/

	// if we launched from the terminal, argv[0][0] would be '.'
	if (argv[0] != NULL && argv[0][0] == DIR_DELIMITER) // don't do the hack if we launched from the terminal
	{
		char *tmpPath = strdup(argv[0]);
		if (tmpPath != NULL)
		{
			// cut off program filename
			int32_t tmpPathLen = strlen(tmpPath);
			for (int32_t i = tmpPathLen - 1; i >= 0; i--)
			{
				if (tmpPath[i] == DIR_DELIMITER)
				{
					tmpPath[i] = '\0';
					break;
				}
			}

			chdir(tmpPath); // path to binary
			chdir("../../../"); // we should now be in the directory where the config can be

			free(tmpPath);
		}
	}
}
#endif

#ifdef _WIN32
static void disableWasapi(void)
{
	// disable problematic WASAPI SDL2 audio driver on Windows (causes clicks/pops sometimes...)

	const int32_t numAudioDrivers = SDL_GetNumAudioDrivers();
	if (numAudioDrivers <= 1)
		return;

	// look for directsound and enable it if found
	for (int32_t i = 0; i < numAudioDrivers; i++)
	{
		const char *audioDriver = SDL_GetAudioDriver(i);
		if (audioDriver != NULL && strcmp("directsound", audioDriver) == 0)
		{
			SDL_setenv("SDL_AUDIODRIVER", "directsound", true);
			audio.rescanAudioDevicesSupported = false;
			return;
		}
	}

	// directsound is not available, try winmm
	for (int32_t i = 0; i < numAudioDrivers; i++)
	{
		const char *audioDriver = SDL_GetAudioDriver(i);
		if (audioDriver != NULL && strcmp("winmm", audioDriver) == 0)
		{
			SDL_setenv("SDL_AUDIODRIVER", "winmm", true);
			audio.rescanAudioDevicesSupported = false;
			return;
		}
	}

	// we didn't find directsound or winmm, let's use wasapi after all...
}
#endif
