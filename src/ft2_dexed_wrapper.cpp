#include "ft2_dexed.h"
#include "dexed/dexed_audio.h"
#include "dexed/msfa/env.h"
#include "dexed/msfa/lfo.h"

#include "dexed/msfa/freqlut.h"
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <cstdio>
#include <cmath>

#include <fstream>
#include <array>


#define DEBUG_DX_WRAPPER 1

#if DEBUG_DX_WRAPPER
#define DX_DEBUG(fmt, ...) printf("[DX_WRAPPER] " fmt "\n", ##__VA_ARGS__)
#else
#define DX_DEBUG(fmt, ...)
#endif

/* =============================================================================
 * Minimal Factory Preset System (Lazy Loaded)
 * ============================================================================= */

struct DxFactory {
    float sampleRate;
    DxFactory() : sampleRate(44100.0f) {}
};

static DxFactory* g_factory = nullptr;

/* Factory preset cache */
static std::vector<std::array<uint8_t, 155>> g_factory_patches;
static std::vector<std::string> g_factory_patch_names;
static bool g_factory_presets_scanned = false;

/* DX7 cartridge format constants */
#define SYSEX_HEADER_SIZE 6
#define SYSEX_SIZE 4104
#define VOICE_DATA_SIZE 4096
#define CARTRIDGE_VOICE_SIZE 128
#define NUM_CARTRIDGE_VOICES 32

/* Normalize a 7-bit value (0-127) to DX7 parameter range (0-99) */
static uint8_t normparm(uint8_t value, int id) {
    if (value <= 99)
        return value;
    /* if this is beyond the max, we expect a 0-255 range, normalize this */
    return (uint8_t)(((float)value) / 127.0f * 99.0f);
}

/* Get program name from packed cartridge voice data */
static void get_voice_name(const uint8_t* packed, char* nameOut) {
    for (int i = 0; i < 10; i++) {
        uint8_t c = packed[118 + i] & 0x7F;
        if (c < 32 || c > 127) c = 32;
        nameOut[i] = (char)c;
    }
    nameOut[10] = '\0';
}

/* Load all 32 voices from a DX7 cartridge file */
static int load_dx7_cartridge(const char* filename, std::vector<std::array<uint8_t, 155>>& patches, std::vector<std::string>& names) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        DX_DEBUG("Cannot open %s", filename);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);

    if (fsize < SYSEX_SIZE) {
        DX_DEBUG("File too small: %ld bytes", fsize);
        fclose(f);
        return -1;
    }

    uint8_t sysex[SYSEX_SIZE];
    size_t readSize = fread(sysex, 1, SYSEX_SIZE, f);
    fclose(f);

    if (readSize < SYSEX_SIZE) {
        DX_DEBUG("Failed to read full file: %zu bytes", readSize);
        return -1;
    }

    if (sysex[0] != 0xF0 || sysex[1] != 0x43) {
        DX_DEBUG("Invalid SysEx header in %s", filename);
        return -1;
    }

    DX_DEBUG("Valid DX7 cartridge: %s", filename);

    uint8_t* voiceData = sysex + SYSEX_HEADER_SIZE;

    int loadedCount = 0;
    for (int i = 0; i < NUM_CARTRIDGE_VOICES; i++) {
        uint8_t* packedVoice = voiceData + (i * CARTRIDGE_VOICE_SIZE);

        std::array<uint8_t, 155> unpackedVoice;
        dx_unpack_program_from_storage(packedVoice, unpackedVoice.data());

        char name[11];
        get_voice_name(packedVoice, name);

        patches.push_back(unpackedVoice);
        names.push_back(std::string(name) + " #" + std::to_string(i + 1));

        loadedCount++;
        DX_DEBUG("  Program %d: algo=%d fb=%d name='%s'",
                 i, unpackedVoice[134], unpackedVoice[135], name);
    }

    DX_DEBUG("Loaded %d presets from %s", loadedCount, filename);
    return loadedCount;
}

static void scan_factory_presets_if_needed(void)
{
    if (g_factory_presets_scanned) return;

    DX_DEBUG("Scanning factory presets (lazy load)...");
    g_factory_presets_scanned = true;
    g_factory_patches.clear();
    g_factory_patch_names.clear();

    const char* listPath = "/home/user/ft2-dxm/src/dexed/patches/patches.txt";
    std::ifstream listifs(listPath);

    if (listifs) {
        DX_DEBUG("Found patches.txt, loading cartridges...");
        std::string line;
        const std::string baseDir = "/home/user/ft2-dxm/src/dexed/patches/";

        while (std::getline(listifs, line)) {
            auto start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) continue;
            auto end = line.find_last_not_of(" \t\r\n");
            std::string filename = line.substr(start, end - start + 1);

            if (filename.empty()) continue;
            if (filename[0] == '#') continue;

            std::string fullpath = baseDir + filename;
            load_dx7_cartridge(fullpath.c_str(), g_factory_patches, g_factory_patch_names);
        }
        listifs.close();
    }

    if (g_factory_patches.empty()) {
        const char* syxPath = "/home/user/ft2-dxm/src/dexed/patches/Dexed_01.syx";
        DX_DEBUG("Loading bundled %s", syxPath);
        load_dx7_cartridge(syxPath, g_factory_patches, g_factory_patch_names);
    }

    DX_DEBUG("Factory scan complete: %zu presets", g_factory_patches.size());
}

extern "C" {

void* dx_synth_create(void)
{
    g_factory = new DxFactory();
    return static_cast<void*>(g_factory);
}

void dx_synth_destroy(void* synth)
{
    if (!synth) return;
    DxFactory* f = static_cast<DxFactory*>(synth);
    delete f;
    if (g_factory == f) g_factory = nullptr;
    g_factory_presets_scanned = false;
}

void dx_synth_init(void* synth, unsigned int sampleRate)
{
    if (!synth) return;
    DxFactory* f = static_cast<DxFactory*>(synth);
    f->sampleRate = static_cast<float>(sampleRate);
    Freqlut::init(f->sampleRate);
    Env::init_sr(f->sampleRate);
}

void* dx_instrument_create(void* synth, int instrID)
{
    float sr = 44100.0f;
    if (synth) {
        DxFactory* f = static_cast<DxFactory*>(synth);
        sr = f->sampleRate;
    }

    DexedAudio* aud = new DexedAudio();
    aud->init(sr);

    DX_DEBUG("dx_instrument_create: instr=%d, aud=%p", instrID, (void*)aud);
    return static_cast<void*>(aud);
}

void dx_instrument_destroy(void* inst)
{
    if (!inst) return;
    DexedAudio* aud = static_cast<DexedAudio*>(inst);
    delete aud;
}

void dx_instrument_process(void* synth, void* inst, float** outputs, unsigned int frameSize)
{
    if (!inst || !outputs) return;
    DexedAudio* aud = static_cast<DexedAudio*>(inst);
    static unsigned int counter = 0;
    if ((counter % 8192) == 0) {
        DX_DEBUG("process: voices=%d", aud->getActiveVoiceCount());
    }
    counter += frameSize;
    aud->render(outputs[0], outputs[1], static_cast<int>(frameSize));
}

void dx_instrument_send_midi(void* inst, uint8_t status, uint8_t data1, uint8_t data2)
{
    if (!inst) return;
    DexedAudio* aud = static_cast<DexedAudio*>(inst);
    uint8_t msgType = status & 0xF0;
    if (msgType == 0x90 && data2 != 0) {
        aud->midiNoteOn(data1, data2);
    } else if (msgType == 0x80 || (msgType == 0x90 && data2 == 0)) {
        aud->midiNoteOff(data1);
    } else if (msgType == 0xB0) {
        aud->midiCC(data1, data2);
    }
}

void dx_instrument_set_param(void* inst, int param, float value)
{
    if (!inst) return;
    DexedAudio* aud = static_cast<DexedAudio*>(inst);

    switch (param) {
        case 1002:
            aud->setFilterCutoff(value);
            break;
        case 1003:
            aud->setFilterReso(value);
            break;
        case 1004:
            aud->setFilterGain(value);
            break;
        case 1005:
            aud->setPortaTime(value);
            // Enable portamento if time > 0
            aud->setPortaEnable(value > 0.0f);
            break;
        case 1006:
            aud->setMonoMode(value > 0.5f);
            break;
        default:
            aud->setParam(param, value);
            break;
    }
}

float dx_instrument_get_param(void* inst, int param)
{
    if (!inst) return 0.0f;
    DexedAudio* aud = static_cast<DexedAudio*>(inst);

    switch (param) {
        case 1002:
            return aud->getFilterCutoff();
        case 1003:
            return aud->getFilterReso();
        case 1004:
            return aud->getFilterGain();
        case 1005:
            return aud->getPortaTime();
        case 1006:
            return aud->getMonoMode() ? 1.0f : 0.0f;
        default:
            return aud->getParam(param);
    }
}

int dx_instrument_get_params(void* inst, float* out, int maxCount)
{
    if (!inst || !out || maxCount <= 0) return 0;
    DexedAudio* aud = static_cast<DexedAudio*>(inst);
    int count = maxCount;
    if (count > 156) count = 156;
    aud->getParams(out, count);
    return count;
}

int dx_get_factory_preset_count(void)
{
    scan_factory_presets_if_needed();
    return (int)g_factory_patches.size();
}

const char* dx_get_factory_preset_name(int index)
{
    scan_factory_presets_if_needed();
    if (index < 0 || index >= (int)g_factory_patch_names.size()) return nullptr;
    return g_factory_patch_names[index].c_str();
}

int dx_instrument_load_factory_preset(void* inst, int index)
{
    if (!inst) return 0;
    scan_factory_presets_if_needed();
    if (index < 0 || index >= (int)g_factory_patches.size()) return 0;

    DexedAudio* aud = static_cast<DexedAudio*>(inst);
    const uint8_t* data = g_factory_patches[index].data();

    DX_DEBUG("Loading factory preset %d: algo=%d fb=%d", index, data[134], data[135]);

    if (aud->loadPatchBlob(data, 155)) {
        aud->setCurrentPreset(index);
        return 1;
    }
    return 0;
}

int dx_instrument_get_current_preset_index(void* inst)
{
    if (!inst) return -1;
    DexedAudio* aud = static_cast<DexedAudio*>(inst);
    return aud->getCurrentPreset();
}

int dx_instrument_load_patch(void* inst, const uint8_t* data, size_t size)
{
    if (!inst || !data || size < 155) return 0;
    DexedAudio* aud = static_cast<DexedAudio*>(inst);
    return aud->loadPatchBlob(data, 155) ? 1 : 0;
}

int dx_instrument_load_packed_patch(void* inst, const uint8_t* data, size_t size)
{
    if (!inst || !data || size < 128) return 0;
    uint8_t unpacked[155];
    if (!dx_unpack_program_from_storage(data, unpacked)) return 0;
    return dx_instrument_load_patch(inst, unpacked, 155);
}

int dx_instrument_save_patch(void* inst, uint8_t* outBuffer, size_t outBufferSize)
{
    if (!inst || !outBuffer || outBufferSize < 155) return 0;
    DexedAudio* aud = static_cast<DexedAudio*>(inst);
    return aud->savePatchBlob(outBuffer, 155);
}

int dx_instrument_save_packed_patch(void* inst, uint8_t* outBuffer, size_t outBufferSize)
{
    if (!inst || !outBuffer || outBufferSize < 128) return 0;
    uint8_t unpacked[155];
    int written = dx_instrument_save_patch(inst, unpacked, 155);
    if (written != 155) return 0;
    return dx_normalize_program(unpacked, outBuffer) ? 128 : 0;
}

size_t dx_instrument_serialize_state(void* inst, uint8_t* outBuf, size_t bufSize)
{
    if (!inst || !outBuf) return 0;
    DexedAudio* aud = static_cast<DexedAudio*>(inst);
    return aud->serializeState(outBuf, bufSize);
}

int dx_instrument_deserialize_state(void* inst, const uint8_t* data, size_t size)
{
    if (!inst || !data) return 0;
    DexedAudio* aud = static_cast<DexedAudio*>(inst);
    return aud->restoreState(data, size) ? 1 : 0;
}

int dx_instrument_get_active_voice_count(void* inst)
{
    if (!inst) return 0;
    DexedAudio* aud = static_cast<DexedAudio*>(inst);
    return aud->getActiveVoiceCount();
}

void dx_instrument_panic(void* inst)
{
    if (!inst) return;
    DexedAudio* aud = static_cast<DexedAudio*>(inst);
    aud->panic();
}

void* dx_instrument_get_latest_voice(void* inst)
{
    if (!inst) return nullptr;
    DexedAudio* aud = static_cast<DexedAudio*>(inst);
    return aud->getLatestVoice();
}

} /* extern "C" */
