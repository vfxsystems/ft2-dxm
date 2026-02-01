/*
 * DexedAudio interface - matches the current implementation
 */

#ifndef DEXED_AUDIO_H
#define DEXED_AUDIO_H

#include <cstdint>
#include <memory>
#include <vector>

class PluginFx;

// Forward declarations
class Controllers;
class EngineMkI;
class DexedCartridge;
struct VoiceStatus;

class DexedAudio {
public:
    DexedAudio();
    ~DexedAudio();

    // Initialization
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();

    // Audio processing
    void render(float* leftOut, float* rightOut, int frameCount);

    // MIDI handling
    void midiNoteOn(uint8_t note, uint8_t velocity);
    void midiNoteOff(uint8_t note);
    void midiCC(uint8_t cc, uint8_t value);

    // Patch management
    void panic();
    void setDx7FactoryDefaults();
    void resetToSafePatch();

    // Parameter access
    void setParam(int param, float value);
    float getParam(int param);

    // Voice management
    int getActiveVoiceCount();
    void* getLatestVoice();

    // Patch loading/saving
    bool loadPatchBlob(const uint8_t* data, size_t size);
    int savePatchBlob(uint8_t* outBuf, size_t bufSize);
    size_t serializeState(uint8_t* outBuf, size_t bufSize);
    bool restoreState(const uint8_t* data, size_t size);

    // Public initialization method
    void init(float sr);

    // Public patch queue method
    bool queuePatchBlob(const uint8_t* data, size_t size);

    // Preset management
    void setCurrentPreset(int index);
    int getCurrentPreset();

    // FX control
    void setFilterCutoff(float value);
    void setFilterReso(float value);
    void setFilterGain(float value);
    float getFilterCutoff();
    float getFilterReso();
    float getFilterGain();

    // Portamento + mono mode
    void setPortaTime(float value);
    float getPortaTime();
    void setPortaEnable(bool enable);
    bool getPortaEnable();
    void setMonoMode(bool enable);
    bool getMonoMode();


private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
    void sanitizePatch(uint8_t* patch);
};


#endif // DEXED_AUDIO_H
