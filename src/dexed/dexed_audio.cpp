#include "dexed_audio.h"
#include "msfa/env.h"
#include "msfa/freqlut.h"
#include "msfa/sin.h"
#include "msfa/exp2.h"
#include "msfa/pitchenv.h"
#include "msfa/porta.h"
#include "msfa/lfo.h"
#include "msfa/fm_op_kernel.h"
#include "msfa/aligned_buf.h"
#include "msfa/synth.h"
#include "msfa/tuning.h"
#include "msfa/dx7note.h"
#include "PluginFx.h"
#include "EngineMkI.h"

#include <cstring>
#include <algorithm>
#include <mutex>
#include <memory>
#include <vector>
#include <queue>
#include <stdio.h>
#include <pmmintrin.h> // Keep this for __SSE__ if needed, otherwise remove
#include <cmath>

// Global initialization
static std::mutex g_initMutex;
static bool g_initialized = false;

// Forward declarations for JUCE-free replacements
struct MidiMessage {
    uint8_t data[3];
    int size;
    MidiMessage(uint8_t status, uint8_t d1, uint8_t d2) {
        data[0] = status;
        data[1] = d1;
        data[2] = d2;
        size = 3;
    }
};

struct ProcessorVoice {
    int channel;
    int midi_note;
    int velocity;
    bool keydown;
    bool sustained;
    bool live;
    int mpePitchBend;
    Dx7Note *dx7_note;
    uint32_t age;
};

class DexedAudio::Impl {
public:
    static const int MAX_ACTIVE_NOTES = 16;
    ProcessorVoice voices[MAX_ACTIVE_NOTES];
    int currentNote;
    uint32_t noteCounter;
    Lfo lfo;
    bool sustain;
    bool monoMode;
    float extra_buf[512]; // Keep this if needed by DexedAudioProcessor, otherwise remove
    int extra_buf_size;   // Keep this if needed by DexedAudioProcessor, otherwise remove
    int currentProgram;
    int currentPreset;
    Controllers controllers;
    uint8_t data[156];  // 156 bytes: 155 data + 1 checksum byte
    PluginFx fx;
    float sampleRate;
    int samplesPerBlock;
    std::mutex patchMutex;
    std::queue<std::vector<uint8_t>> patchQueue;
    std::shared_ptr<TuningState> synthTuningState;
    MTSClient *mtsClient;
    EngineMkI engineMkI; // The FM core instance
    bool voicesNeedUpdate; // Flag to signal that voices need updating
    bool patchInitialized;
    float blockBuf[N];
    int blockPos;
    int lastActiveVoice;

    Impl() : sampleRate(44100.0f), samplesPerBlock(512), currentNote(-1),
             sustain(false), monoMode(false), extra_buf_size(0),
             currentProgram(0), currentPreset(-1), mtsClient(nullptr),
             voicesNeedUpdate(false), patchInitialized(false) {
        std::lock_guard<std::mutex> lock(g_initMutex);
        if (!g_initialized) {
            Exp2::init();
            Tanh::init();
            Sin::init();
            g_initialized = true;
        }

        // Initialize tuning state
        synthTuningState = createStandardTuning();

        // Initialize voices
        for (int note = 0; note < MAX_ACTIVE_NOTES; ++note) {
            voices[note].dx7_note = new Dx7Note(synthTuningState, nullptr); // Pass mtsClient if available
            voices[note].midi_note = -1;
            voices[note].keydown = false;
            voices[note].sustained = false;
            voices[note].live = false;
            voices[note].age = 0;
        }
        noteCounter = 0;
        blockPos = N;
        lastActiveVoice = -1;

        // Initialize the FM core (controllers.core is set in prepareToPlay)
        // Only set the pointer here, values are set in prepareToPlay or initCtrl
        controllers.core = &engineMkI;

        // Initial default controller values (these will be set more fully in prepareToPlay)
        controllers.values_[kControllerPitchRangeUp] = 3;
        controllers.values_[kControllerPitchRangeDn] = 3;
        controllers.values_[kControllerPitchStep] = 0;
        controllers.masterTune = 0;
        controllers.values_[kControllerPitch] = 0x2000;
        controllers.modwheel_cc = 0;
        controllers.foot_cc = 0;
        controllers.breath_cc = 0;
        controllers.aftertouch_cc = 0;
        controllers.portamento_enable_cc = false;
        controllers.portamento_gliss_cc = false;
        controllers.portamento_cc = 0;
        controllers.mpeEnabled = false;
        controllers.mpePitchBendRange = 24; // Default as in PluginProcessor
        // Ensure opSwitch is initialized to "111111"
        strcpy(controllers.opSwitch, "111111");
    }

    ~Impl() {
        for (int note = 0; note < MAX_ACTIVE_NOTES; ++note) {
            if (voices[note].dx7_note != nullptr) {
                delete voices[note].dx7_note;
                voices[note].dx7_note = nullptr;
            }
        }
    }

    void prepareToPlay(double sr, int blockSize) {
        sampleRate = static_cast<float>(sr);
        samplesPerBlock = blockSize;

        Freqlut::init(sampleRate);
        Lfo::init(sampleRate);
        PitchEnv::init(sampleRate);
        Env::init_sr(sampleRate);
        Porta::init_sr(sampleRate);
        fx.init(static_cast<int>(sampleRate)); // Ensure this cast is safe

        // FULLY initialize controller state here (once)
        controllers.values_[kControllerPitch] = 0x2000;
        controllers.modwheel_cc = 0;
        controllers.foot_cc = 0;
        controllers.breath_cc = 0;
        controllers.aftertouch_cc = 0;
        controllers.portamento_enable_cc = false;
        controllers.portamento_gliss_cc = false;
        controllers.portamento_cc = 0;
        controllers.refresh(); // Important: Call refresh after setting values

        sustain = false;
        extra_buf_size = 0; // Reset extra buffer size
        currentNote = 0;    // Reset current note

        if (!patchInitialized) {
            resetToSafePatch();
        } else {
            lfo.reset(data + 137);
        }
    }

    void releaseResources() {
        currentNote = -1;
        for (int note = 0; note < MAX_ACTIVE_NOTES; ++note) {
            if (voices[note].dx7_note != nullptr) {
                voices[note].dx7_note->keyup();
            }
            voices[note].keydown = false;
            voices[note].sustained = false;
            voices[note].live = false;
        }
    }

    void processMidiMessage(const MidiMessage& msg) {
        const uint8_t *buf = msg.data;
        uint8_t cmd = buf[0];
        auto channel = (cmd & 0x0F) + 1;

        switch(cmd & 0xf0) {
        case 0x80:
            keyup(channel, buf[1], buf[2]);
            break;

        case 0x90:
            if (buf[2] != 0) {
                keydown(channel, buf[1], buf[2]);
            } else {
                keyup(channel, buf[1], buf[2]);
            }
            break;

        case 0xb0: {
            int ctrl = buf[1];
            int value = buf[2];
            switch(ctrl) {
            case 1:
                controllers.modwheel_cc = value;
                controllers.refresh();
                break;
            case 2:
                controllers.breath_cc = value;
                controllers.refresh();
                break;
            case 4:
                controllers.foot_cc = value;
                controllers.refresh();
                break;
            case 5:
                controllers.portamento_cc = value;
                break;
            case 64:
                sustain = value > 63;
                if (!sustain) {
                    for (int note = 0; note < MAX_ACTIVE_NOTES; note++) {
                        if (voices[note].sustained && !voices[note].keydown) {
                            voices[note].dx7_note->keyup();
                            voices[note].sustained = false;
                        }
                    }
                }
                break;
            case 65:
                controllers.portamento_enable_cc = value >= 64;
                break;
            case 120:
                panic();
                break;
            case 123:
                for (int note = 0; note < MAX_ACTIVE_NOTES; note++) {
                    if (voices[note].keydown)
                        keyup(channel, voices[note].midi_note, 0);
                }
                break;
            }
            break;
        }

        case 0xd0:
            controllers.aftertouch_cc = buf[1];
            controllers.refresh();
            break;

        case 0xe0:
            controllers.values_[kControllerPitch] = buf[1] | (buf[2] << 7);
            break;
        }
    }

    void keydown(uint8_t channel, uint8_t pitch, uint8_t velo) {
        if (velo == 0) {
            keyup(channel, pitch, velo);
            return;
        }

        int note = currentNote;
        for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
            if (!voices[note].keydown) {
                break;
            }
            note = (note + 1) % MAX_ACTIVE_NOTES;
        }

        if (voices[note].keydown) {
            int candidate = -1;
            for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
                if (voices[i].live && !voices[i].keydown) {
                    if (candidate < 0 || voices[i].dx7_note->isReleased()) {
                        candidate = i;
                        if (voices[i].dx7_note->isReleased()) break;
                    }
                }
            }

            if (candidate < 0) {
                candidate = 0;
                for (int i = 1; i < MAX_ACTIVE_NOTES; i++) {
                    if (voices[i].age < voices[candidate].age) {
                        candidate = i;
                    }
                }
            }
            note = candidate;
        }

        currentNote = (note + 1) % MAX_ACTIVE_NOTES;
        lfo.keydown();
        voices[note].channel = channel;
        voices[note].midi_note = pitch;
        voices[note].velocity = velo;
        voices[note].sustained = sustain;
        voices[note].keydown = true;
        voices[note].live = true;
        voices[note].age = ++noteCounter;

        // Initialize voice with current patch data (mutex already handled by caller or processPatchQueue)
        // Mutex is needed here to protect access to 'data' which is shared with loadPatchBlob.
        {
            std::lock_guard<std::mutex> lock(patchMutex);
            voices[note].dx7_note->init(data, pitch, velo, channel, &controllers);
        }

        if (data[136]) // Check for Osc Sync flag in patch
            voices[note].dx7_note->oscSync();

        if (lastActiveVoice >= 0 &&
            controllers.portamento_enable_cc &&
            controllers.portamento_cc > 0) {
            voices[note].dx7_note->initPortamento(*voices[lastActiveVoice].dx7_note);
        }

        if (monoMode) {
            for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
                if (voices[i].live) {
                    if (!voices[i].keydown) {
                        voices[i].live = false;
                        voices[note].dx7_note->transferSignal(*voices[i].dx7_note);
                    } else {
                        voices[i].live = false;
                        voices[note].dx7_note->transferState(*voices[i].dx7_note);
                    }
                    break;
                }
            }
        }
        voices[note].live = true; // Mark voice as live AFTER initialization
        lastActiveVoice = note;
    }

    void keyup(uint8_t chan, uint8_t pitch, uint8_t velo) {
        int note;
        for (note = 0; note < MAX_ACTIVE_NOTES; ++note) {
            if (voices[note].midi_note == pitch && voices[note].keydown) {
                voices[note].keydown = false;
                break;
            }
        }

        if (note >= MAX_ACTIVE_NOTES) {
            return;
        }

        if (monoMode) {
            int highNote = -1;
            int target = 0;
            for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
                if (voices[i].keydown && voices[i].midi_note > highNote) {
                    target = i;
                    highNote = voices[i].midi_note;
                }
            }

            if (highNote != -1 && voices[note].live) {
                voices[note].live = false;
                voices[target].live = true;
                voices[target].dx7_note->transferState(*voices[note].dx7_note);
            }
        }

        if (sustain) {
            voices[note].sustained = true; // Mark as sustained instead of immediately keying up
        } else {
            // Only call keyup if not sustained, keep voice alive for release
            if (voices[note].live) {
                voices[note].dx7_note->keyup();
            }
        }
    }

    void render(float* leftOut, float* rightOut, int frameCount) {
        // Prevent denormalized floats (can crash FPU)
        #ifdef __SSE__
        _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
        _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
        #endif

        processPatchQueue();

        // If a parameter has changed, update all active voices
        if (voicesNeedUpdate) {
            std::lock_guard<std::mutex> lock(patchMutex);
            for (int note = 0; note < MAX_ACTIVE_NOTES; ++note) {
                if (voices[note].live && voices[note].dx7_note != nullptr) {
                    voices[note].dx7_note->update(data, voices[note].midi_note,
                                                 voices[note].velocity, voices[note].channel);
                }
            }
            voicesNeedUpdate = false;
        }

        for (int i = 0; i < frameCount; i++) {
            if (blockPos >= N) {
                int32_t audiobuf[N];
                float sumbuf[N];
                memset(audiobuf, 0, sizeof(audiobuf));
                memset(sumbuf, 0, sizeof(sumbuf));

                int32_t lfovalue = lfo.getsample();
                int32_t lfodelay = lfo.getdelay();

                for (int note = 0; note < MAX_ACTIVE_NOTES; ++note) {
                    if (voices[note].live && voices[note].dx7_note != nullptr) {
                        memset(audiobuf, 0, sizeof(audiobuf));
                        voices[note].dx7_note->compute(audiobuf, lfovalue, lfodelay, &controllers);

                        for (int j = 0; j < N; ++j) {
                            int32_t val = audiobuf[j];
                            val = val >> 4;
                            int clip_val = val < -(1 << 24) ? 0x8000 : val >= (1 << 24) ? 0x7fff : val >> 9;
                            float f = static_cast<float>(clip_val) / static_cast<float>(0x8000);
                            if (f > 1.0f) f = 1.0f;
                            if (f < -1.0f) f = -1.0f;
                            sumbuf[j] += f;
                        }
                    }
                }

                for (int note = 0; note < MAX_ACTIVE_NOTES; ++note) {
                    if (voices[note].live && !voices[note].keydown && !voices[note].sustained &&
                        voices[note].dx7_note != nullptr && voices[note].dx7_note->isReleased()) {
                        voices[note].live = false;
                        voices[note].midi_note = -1;
                    }
                }

                for (int j = 0; j < N; ++j) {
                    blockBuf[j] = sumbuf[j];
                }

                blockPos = 0;
            }

            float v = blockBuf[blockPos++];
            leftOut[i] = v;
            rightOut[i] = v;
        }

        fx.process(leftOut, frameCount);
        if (leftOut != rightOut) {
            std::memcpy(rightOut, leftOut, frameCount * sizeof(float));
        }
    }

    void processPatchQueue() {
        std::lock_guard<std::mutex> lock(patchMutex);
        while (!patchQueue.empty()) {
            auto& patch = patchQueue.front();
            if (patch.size() >= 156) {
                // Apply patch to all voices (use 156 bytes for Dx7Note compatibility)
                for (int i = 0; i < 156 && i < (int)patch.size(); i++) {
                    data[i] = patch[i];
                }
                for (int note = 0; note < MAX_ACTIVE_NOTES; ++note) {
                    if (voices[note].live && voices[note].dx7_note != nullptr) {
                        voices[note].dx7_note->update(data, voices[note].midi_note,
                                                     voices[note].velocity, voices[note].channel);
                    }
                }
                lfo.reset(data + 137);
            }
            patchQueue.pop();
        }
    }

    void midiNoteOn(uint8_t note, uint8_t velocity) {
        MidiMessage msg(0x90, note, velocity);
        processMidiMessage(msg);
    }

    void midiNoteOff(uint8_t note) {
        MidiMessage msg(0x80, note, 0);
        processMidiMessage(msg);
    }

    void midiCC(uint8_t cc, uint8_t value) {
        MidiMessage msg(0xB0, cc, value);
        processMidiMessage(msg);
    }

    void panic() {
        for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
            voices[i].midi_note = -1;
            voices[i].keydown = false;
            voices[i].live = false;
            if (voices[i].dx7_note != nullptr) {
                voices[i].dx7_note->oscSync();
            }
        }
    }

    void resetToSafePatch() {
        std::lock_guard<std::mutex> lock(patchMutex);
        std::memset(data, 0, sizeof(data));

        // Set up a minimal safe patch (algorithm 0, all operators enabled)
        data[134] = 0;  // Algorithm 0
        data[135] = 0;  // Feedback 0
        data[136] = 0;  // Osc sync off

        // Enable all operators and set reasonable output levels (offset +16 for output level)
        // Operator 1 (op=0, offset=0)
        data[0 + 16] = 99;  // Output level (99 = max)
        data[0 + 4] = 50;   // Level 1 (rates/levels are 0-99 in unpacked format)
        data[0 + 5] = 50;   // Level 2
        data[0 + 6] = 50;   // Level 3

        // Operator 2 (op=1, offset=21)
        data[21 + 16] = 99;  // Output level
        data[21 + 4] = 50;   // Level 1

        // Operator 3 (op=2, offset=42)
        data[42 + 16] = 99;  // Output level
        data[42 + 4] = 50;   // Level 1

        // Operator 4 (op=3, offset=63)
        data[63 + 16] = 99;  // Output level
        data[63 + 4] = 50;   // Level 1

        // Operator 5 (op=4, offset=84)
        data[84 + 16] = 99;  // Output level
        data[84 + 4] = 50;   // Level 1

        // Operator 6 (op=5, offset=105)
        data[105 + 16] = 99;  // Output level
        data[105 + 4] = 50;   // Level 1

        lfo.reset(data + 137);
        patchInitialized = true;
    }

    void setDx7FactoryDefaults() {
        resetToSafePatch();
    }

    void setParam(int param, float value) {
        std::lock_guard<std::mutex> lock(patchMutex);

        if (param >= 0 && param < 156) {
            if (std::isnan(value)) value = 0.0f;
            if (value < 0.0f) value = 0.0f;
            if (value > 1.0f) value = 1.0f;

            int maxVal = 127;
            if (param >= 0 && param < 126) {
                int opParam = param % 21;
                switch (opParam) {
                    case 13: maxVal = 7;  break;  // rate scaling
                    case 14: maxVal = 3;  break;  // amp mod sens
                    case 15: maxVal = 7;  break;  // key vel sens
                    case 17: maxVal = 1;  break;  // osc mode
                    case 18: maxVal = 31; break;  // coarse
                    case 19: maxVal = 99; break;  // fine
                    case 20: maxVal = 14; break;  // detune
                    default: maxVal = 99; break;  // most op params
                }
            } else if (param >= 126 && param <= 133) {
                maxVal = 99; // pitch EG rates/levels
            } else if (param == 134) {
                maxVal = 31; // algorithm
            } else if (param == 135) {
                maxVal = 7; // feedback
            } else if (param == 136) {
                maxVal = 1; // key sync
            } else if (param >= 137 && param <= 140) {
                maxVal = 99; // LFO rate/delay/pitch/amp depth
            } else if (param == 141) {
                maxVal = 1; // LFO sync
            } else if (param == 142) {
                maxVal = 5; // LFO waveform
            } else if (param == 143) {
                maxVal = 7; // pitch mod sensitivity
            }

            data[param] = static_cast<uint8_t>(lroundf(value * (float)maxVal));

            // Signal that voices need updating instead of doing it here
            voicesNeedUpdate = true;

            // LFO parameters are cached in the LFO instance; refresh on change.
            if (param >= 136 && param <= 142) {
                lfo.reset(data + 137);
            }
        }
    }

    float getParam(int param) {
        std::lock_guard<std::mutex> lock(patchMutex);

        if (param >= 0 && param < 156) {
            int maxVal = 127;
            if (param >= 0 && param < 126) {
                int opParam = param % 21;
                switch (opParam) {
                    case 13: maxVal = 7;  break;
                    case 14: maxVal = 3;  break;
                    case 15: maxVal = 7;  break;
                    case 17: maxVal = 1;  break;
                    case 18: maxVal = 31; break;
                    case 19: maxVal = 99; break;
                    case 20: maxVal = 14; break;
                    default: maxVal = 99; break;
                }
            } else if (param >= 126 && param <= 133) {
                maxVal = 99;
            } else if (param == 134) {
                maxVal = 31;
            } else if (param == 135) {
                maxVal = 7;
            } else if (param == 136) {
                maxVal = 1;
            } else if (param >= 137 && param <= 140) {
                maxVal = 99;
            } else if (param == 141) {
                maxVal = 1;
            } else if (param == 142) {
                maxVal = 5;
            } else if (param == 143) {
                maxVal = 7;
            }

            if (maxVal <= 0) return 0.0f;
            return static_cast<float>(data[param]) / (float)maxVal;
        }
        return 0.0f;
    }

    int getActiveVoiceCount() {
        int count = 0;
        for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
            if (voices[i].live) {
                count++;
            }
        }
        return count;
    }

    void* getLatestVoice() {
        for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
            if (voices[i].live) {
                return voices[i].dx7_note;
            }
        }
        return nullptr;
    }

    bool loadPatchBlob(const uint8_t* patchData, size_t size) {
        // Accept either 155 or 156 byte patches
        size_t copySize = (size >= 156) ? 156 : size;
        if (size < 155) {
            // printf("[DEXED] loadPatchBlob: size %zu too small (need 155+)\n", size);
            return false;
        }

        // printf("[DEXED] loadPatchBlob: loaded %zu bytes\n", copySize);
        // printf("[DEXED]   OP1 out=%d OP2 out=%d OP3 out=%d OP4 out=%d OP5 out=%d OP6 out=%d\n",
        //        data[16], data[37], data[58], data[79], data[100], data[121]);
        // printf("[DEXED]   LFO rate=%d delay=%d pitch=%d amp=%d\n",
        //        data[137], data[138], data[139], data[140]);

        std::lock_guard<std::mutex> lock(patchMutex);
        std::memcpy(data, patchData, copySize);

        // Pad to 156 bytes if needed
        if (size < 156) {
            data[155] = 0;  // Zero checksum byte if not provided
        }

        // printf("[DEXED] loadPatchBlob: loaded %zu bytes, data[0]=%d data[134]=%d data[135]=%d\n",
        //        copySize, data[0], data[134], data[135]);

        // Update LFO with new patch
        lfo.reset(data + 137);

        // Signal that voices need updating with the new patch data
        voicesNeedUpdate = true;
        patchInitialized = true;

        return true;
    }

    int savePatchBlob(uint8_t* outBuf, size_t bufSize) {
        if (bufSize < 155) return 0;
        size_t copySize = (bufSize >= 156) ? 156 : 155;

        std::lock_guard<std::mutex> lock(patchMutex);
        std::memcpy(outBuf, data, copySize);
        return (int)copySize;
    }

    size_t serializeState(uint8_t* outBuf, size_t bufSize) {
        if (bufSize < 155) return 0;
        size_t copySize = (bufSize >= 156) ? 156 : 155;

        std::lock_guard<std::mutex> lock(patchMutex);
        std::memcpy(outBuf, data, copySize);
        return copySize;
    }

    bool restoreState(const uint8_t* data, size_t size) {
        return loadPatchBlob(data, size);
    }
};

// DexedAudio implementation
DexedAudio::DexedAudio() : pImpl(std::make_unique<Impl>()) {}
DexedAudio::~DexedAudio() = default;

void DexedAudio::prepareToPlay(double sampleRate, int samplesPerBlock) {
    pImpl->prepareToPlay(sampleRate, samplesPerBlock);
}

void DexedAudio::releaseResources() {
    pImpl->releaseResources();
}

void DexedAudio::render(float* leftOut, float* rightOut, int frameCount) {
    pImpl->render(leftOut, rightOut, frameCount);

}

void DexedAudio::midiNoteOn(uint8_t note, uint8_t velocity) {
    pImpl->midiNoteOn(note, velocity);
}

void DexedAudio::midiNoteOff(uint8_t note) {
    pImpl->midiNoteOff(note);
}

void DexedAudio::midiCC(uint8_t cc, uint8_t value) {
    pImpl->midiCC(cc, value);
}

void DexedAudio::panic() {
    pImpl->panic();
}

void DexedAudio::setDx7FactoryDefaults() {
    pImpl->setDx7FactoryDefaults();
}

void DexedAudio::resetToSafePatch() {
    pImpl->resetToSafePatch();
}

void DexedAudio::setParam(int param, float value) {
    pImpl->setParam(param, value);
}

float DexedAudio::getParam(int param) {
    return pImpl->getParam(param);
}

int DexedAudio::getActiveVoiceCount() {
    return pImpl->getActiveVoiceCount();
}

void* DexedAudio::getLatestVoice() {
    return pImpl->getLatestVoice();
}

bool DexedAudio::loadPatchBlob(const uint8_t* data, size_t size) {
    return pImpl->loadPatchBlob(data, size);
}

int DexedAudio::savePatchBlob(uint8_t* outBuf, size_t bufSize) {
    return pImpl->savePatchBlob(outBuf, bufSize);
}

size_t DexedAudio::serializeState(uint8_t* outBuf, size_t bufSize) {
    return pImpl->serializeState(outBuf, bufSize);
}

bool DexedAudio::restoreState(const uint8_t* data, size_t size) {
    return pImpl->restoreState(data, size);
}

void DexedAudio::init(float sr) {
    prepareToPlay(sr, 512); // blockSize (512) is arbitrary here, actual will come from render call
}

bool DexedAudio::queuePatchBlob(const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(pImpl->patchMutex);
    if (size < 155) return false;

    // Pad to 156 bytes for Dx7Note compatibility
    std::vector<uint8_t> patch(156, 0);
    std::memcpy(patch.data(), data, std::min(size, (size_t)156));
    pImpl->patchQueue.push(patch);
    return true;
}

void DexedAudio::setCurrentPreset(int index) {
    pImpl->currentPreset = index;
}

int DexedAudio::getCurrentPreset() {
    return pImpl->currentPreset;
}

void DexedAudio::setFilterCutoff(float value) {
    pImpl->fx.uiCutoff = value;
}

void DexedAudio::setFilterReso(float value) {
    pImpl->fx.uiReso = value;
}

void DexedAudio::setFilterGain(float value) {
    pImpl->fx.uiGain = value;
}

float DexedAudio::getFilterCutoff() {
    return pImpl->fx.uiCutoff;
}

float DexedAudio::getFilterReso() {
    return pImpl->fx.uiReso;
}

float DexedAudio::getFilterGain() {
    return pImpl->fx.uiGain;
}

void DexedAudio::setPortaTime(float value) {
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    pImpl->controllers.portamento_cc = (int32_t)lroundf(value * 127.0f);
    if (pImpl->controllers.portamento_cc < 0) pImpl->controllers.portamento_cc = 0;
    if (pImpl->controllers.portamento_cc > 127) pImpl->controllers.portamento_cc = 127;
}

float DexedAudio::getPortaTime() {
    return (float)pImpl->controllers.portamento_cc / 127.0f;
}

void DexedAudio::setPortaEnable(bool enable) {
    pImpl->controllers.portamento_enable_cc = enable;
}

bool DexedAudio::getPortaEnable() {
    return pImpl->controllers.portamento_enable_cc;
}

void DexedAudio::setMonoMode(bool enable) {
    pImpl->monoMode = enable;
}

bool DexedAudio::getMonoMode() {
    return pImpl->monoMode;
}
