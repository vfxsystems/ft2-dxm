/* Included by ft2_midi.c to exercise the actual device callback without hardware. */
#include "ft2_v2.h"
static void injectMidi(uint8_t status, uint8_t data1, uint8_t data2)
{
    const uint8_t message[3] = {status, data1, data2};
    midiInCallback(0, message, sizeof(message), NULL);
}

static int midiQueueProducer(void *data)
{
    for (int i = 0; i < 4000; ++i) {
        injectMidi(0x90, 60, 100);
        injectMidi(0x80, 60, 0);
    }
    SDL_AtomicSet((SDL_atomic_t *)data, 1);
    return 0;
}

bool runMidiRegressionTests(void)
{
    bool ok = true;
#define MIDI_CHECK(condition, description) do { if (!(condition)) { \
    fprintf(stderr, "MIDI regression: %s\n", description); ok = false; } } while (0)
    midi.enable = true;
    SDL_AtomicSet(&midiAccepting, 1);
    config.recMIDIAllChn = true;
    config.recMIDITransp = false;
    config.recMIDIVelocity = true;
    config.recMIDIVolSens = 100;
    config.multiKeyJazz = false;
    cursor.ch = 0;
    editor.curInstr = 1;
    injectMidi(0x90, 60, 100);
    MIDI_CHECK(editor.keyOnTab[0] == 0, "device callback does not edit tracker state");
    processMidiInput();
    MIDI_CHECK(editor.keyOnTab[0] == 49, "application thread processes note-on");
    const uint8_t truncated[2] = {0x90, 60};
    midiInCallback(0, truncated, sizeof(truncated), NULL);
    injectMidi(0x80, 60, 255);
    processMidiInput();
    MIDI_CHECK(editor.keyOnTab[0] == 49, "malformed messages cannot become note-offs");
    config.recMIDITransp = true;
    config.recMIDITranspVal = 12;
    config.recMIDIAllChn = false;
    config.recMIDIChn = 2;
    cursor.ch = 1;
    editor.curInstr = 2;
    injectMidi(0x80, 60, 0);
    processMidiInput();
    MIDI_CHECK(editor.keyOnTab[0] == 0, "release survives transpose/filter/cursor/instrument change");

    config.recMIDIAllChn = true;
    config.recMIDITransp = false;
    cursor.ch = 0;
    editor.curInstr = 1;
    injectMidi(0x90, 60, 100);
    injectMidi(0xB0, 64, 127);
    injectMidi(0x90, 60, 0);
    processMidiInput();
    MIDI_CHECK(editor.keyOnTab[0] == 49, "pedal holds a released input note");
    config.recMIDIAllChn = false;
    injectMidi(0xB0, 64, 0);
    processMidiInput();
    MIDI_CHECK(editor.keyOnTab[0] == 0, "pedal-up survives channel-filter changes");
    config.recMIDIAllChn = true;
    injectMidi(0x90, 60, 100);
    injectMidi(0x91, 62, 100);
    injectMidi(0x80, 60, 0);
    processMidiInput();
    MIDI_CHECK(editor.keyOnTab[0] == 51, "stolen channel is not released by its former owner");
    injectMidi(0x81, 62, 0);
    processMidiInput();
    injectMidi(0x90, 107, 100);
    processMidiInput();
    MIDI_CHECK(editor.keyOnTab[0] == 96, "top tracker note is playable from MIDI");
    ui.sysReqShown = true;
    injectMidi(0x80, 107, 0);
    injectMidi(0x90, 60, 100);
    processMidiInput();
    MIDI_CHECK(editor.keyOnTab[0] == 0, "dialogs release held notes and suppress new notes");
    ui.sysReqShown = false;
    injectMidi(0x90, 60, 100);
    processMidiInput();
    for (int i = 0; i < MIDI_QUEUE_CAPACITY + 10; ++i) injectMidi(0xB0, 1, 0);
    processMidiInput();
    MIDI_CHECK(editor.keyOnTab[0] == 0, "overflow releases owned notes instead of losing note-off");
    injectMidi(0x90, 60, 100);
    processMidiInput();
    MIDI_CHECK(editor.keyOnTab[0] == 49, "input recovers after queue overflow");
    closeMidiInDevice();
    MIDI_CHECK(editor.keyOnTab[0] == 0, "device close releases input-owned notes");
    injectMidi(0x90, 60, 100);
    processMidiInput();
    MIDI_CHECK(editor.keyOnTab[0] == 0, "closed device cannot enqueue input");
    SDL_AtomicSet(&midiAccepting, 1);
    SDL_atomic_t producerDone = {0};
    SDL_Thread *producer = SDL_CreateThread(midiQueueProducer, "MIDI regression", &producerDone);
    MIDI_CHECK(producer != NULL, "create concurrent MIDI producer");
    if (producer) {
        while (!SDL_AtomicGet(&producerDone)) processMidiInput();
        SDL_WaitThread(producer, NULL);
        processMidiInput();
        MIDI_CHECK(editor.keyOnTab[0] == 0, "concurrent queue traffic leaves no owned notes");
    }
    closeMidiInDevice();
    config.recMIDIChn = 1;
    config.recMIDITranspVal = 0;
    ft2_v2_panic();
#undef MIDI_CHECK
    return ok;
}
