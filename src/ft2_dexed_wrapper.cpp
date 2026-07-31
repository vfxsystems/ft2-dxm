#include "ft2_dexed.h"
#include "dexed/dexed_audio.h"
#include "dexed/msfa/env.h"
#include "dexed/msfa/lfo.h"

#include "dexed/msfa/freqlut.h"
#include "synthLib/os.h"
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <cstdio>
#include <cmath>
#include <cctype>
#include <limits>

#include <fstream>
#include <array>
#include <zlib.h>

#ifdef FT2_DEXED_EMBEDDED_BUILTIN_ZIP
extern "C" {
extern const unsigned char ft2_dexed_builtin_pgm_zip[];
extern const size_t ft2_dexed_builtin_pgm_zip_len;
}
#endif


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

static uint16_t read_u16(const uint8_t *p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

static uint32_t read_u32(const uint8_t *p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

static std::string to_lower_copy(std::string value) {
    for (char &ch : value)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return value;
}

static std::string basename_copy(const std::string &path) {
    const size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos)
        return path;
    return path.substr(pos + 1);
}

static bool inflate_raw_deflate(const uint8_t *input, size_t inputSize, std::vector<uint8_t> &out, size_t expectedSize) {
    if (!input || inputSize == 0)
        return false;

    out.assign(expectedSize, 0);

    z_stream stream{};
    stream.next_in = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(input));
    stream.avail_in = static_cast<uInt>(std::min(inputSize, static_cast<size_t>(std::numeric_limits<uInt>::max())));
    stream.next_out = reinterpret_cast<Bytef *>(out.data());
    stream.avail_out = static_cast<uInt>(std::min(expectedSize, static_cast<size_t>(std::numeric_limits<uInt>::max())));

    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        return false;

    const int ret = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);

    if (ret != Z_STREAM_END)
        return false;

    out.resize(static_cast<size_t>(stream.total_out));
    return true;
}

static bool extract_zip_entry(const std::vector<uint8_t> &zipData, const std::string &wantedName, std::vector<uint8_t> &out) {
    if (zipData.size() < 22)
        return false;

    const std::string wantedLower = to_lower_copy(basename_copy(wantedName));

    const size_t maxBackSearch = std::min<size_t>(zipData.size(), 0x10000u + 22u);
    const size_t searchBegin = zipData.size() - maxBackSearch;
    size_t eocdPos = std::string::npos;

    for (size_t pos = zipData.size() - 22; pos >= searchBegin; --pos) {
        if (read_u32(&zipData[pos]) == 0x06054b50u) {
            eocdPos = pos;
            break;
        }
        if (pos == searchBegin)
            break;
    }

    if (eocdPos == std::string::npos)
        return false;

    const uint32_t cdOffset = read_u32(&zipData[eocdPos + 16]);
    const uint16_t cdEntries = read_u16(&zipData[eocdPos + 10]);
    if (cdOffset >= zipData.size())
        return false;

    size_t cdPos = cdOffset;
    for (uint16_t i = 0; i < cdEntries; ++i) {
        if (cdPos + 46 > zipData.size() || read_u32(&zipData[cdPos]) != 0x02014b50u)
            return false;

        const uint16_t compression = read_u16(&zipData[cdPos + 10]);
        const uint32_t compressedSize = read_u32(&zipData[cdPos + 20]);
        const uint32_t uncompressedSize = read_u32(&zipData[cdPos + 24]);
        const uint16_t nameLen = read_u16(&zipData[cdPos + 28]);
        const uint16_t extraLen = read_u16(&zipData[cdPos + 30]);
        const uint16_t commentLen = read_u16(&zipData[cdPos + 32]);
        const uint32_t localOffset = read_u32(&zipData[cdPos + 42]);

        if (cdPos + 46 + nameLen + extraLen + commentLen > zipData.size())
            return false;

        const std::string entryName(reinterpret_cast<const char *>(&zipData[cdPos + 46]), nameLen);
        if (to_lower_copy(basename_copy(entryName)) == wantedLower) {
            if (localOffset + 30 > zipData.size() || read_u32(&zipData[localOffset]) != 0x04034b50u)
                return false;

            const uint16_t localNameLen = read_u16(&zipData[localOffset + 26]);
            const uint16_t localExtraLen = read_u16(&zipData[localOffset + 28]);
            const size_t dataOffset = localOffset + 30u + localNameLen + localExtraLen;

            if (dataOffset + compressedSize > zipData.size())
                return false;

            const uint8_t *payload = &zipData[dataOffset];
            if (compression == 0) {
                out.assign(payload, payload + compressedSize);
                return true;
            }
            if (compression == 8)
                return inflate_raw_deflate(payload, compressedSize, out, uncompressedSize);
            return false;
        }

        cdPos += 46u + nameLen + extraLen + commentLen;
    }

    return false;
}

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

static int load_dx7_cartridge_bytes(const char* label, const uint8_t* sysex, size_t size, std::vector<std::array<uint8_t, 155>>& patches, std::vector<std::string>& names) {
    if (!sysex || size < SYSEX_SIZE) {
        DX_DEBUG("File too small for cartridge %s: %zu bytes", label ? label : "(memory)", size);
        return -1;
    }

    if (sysex[0] != 0xF0 || sysex[1] != 0x43) {
        DX_DEBUG("Invalid SysEx header in %s", label ? label : "(memory)");
        return -1;
    }

    DX_DEBUG("Valid DX7 cartridge: %s", label ? label : "(memory)");

    const uint8_t* voiceData = sysex + SYSEX_HEADER_SIZE;

    int loadedCount = 0;
    for (int i = 0; i < NUM_CARTRIDGE_VOICES; i++) {
        const uint8_t* packedVoice = voiceData + (i * CARTRIDGE_VOICE_SIZE);

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

    DX_DEBUG("Loaded %d presets from %s", loadedCount, label ? label : "(memory)");
    return loadedCount;
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

    return load_dx7_cartridge_bytes(filename, sysex, readSize, patches, names);
}

static void scan_factory_presets_if_needed(void)
{
    if (g_factory_presets_scanned) return;

    DX_DEBUG("Scanning factory presets (lazy load)...");
    g_factory_presets_scanned = true;
    g_factory_patches.clear();
    g_factory_patch_names.clear();

#ifdef FT2_DEXED_EMBEDDED_BUILTIN_ZIP
    {
        static const char *const kEmbeddedCartridges[] = {
            "Dexed_01.syx",
            "SynprezFM_01.syx", "SynprezFM_02.syx", "SynprezFM_03.syx", "SynprezFM_04.syx",
            "SynprezFM_05.syx", "SynprezFM_06.syx", "SynprezFM_07.syx", "SynprezFM_08.syx",
            "SynprezFM_09.syx", "SynprezFM_10.syx", "SynprezFM_11.syx", "SynprezFM_12.syx",
            "SynprezFM_13.syx", "SynprezFM_14.syx", "SynprezFM_15.syx", "SynprezFM_16.syx",
            "SynprezFM_17.syx", "SynprezFM_18.syx", "SynprezFM_19.syx", "SynprezFM_20.syx",
            "SynprezFM_21.syx", "SynprezFM_22.syx", "SynprezFM_23.syx", "SynprezFM_24.syx",
            "SynprezFM_25.syx", "SynprezFM_26.syx", "SynprezFM_27.syx", "SynprezFM_28.syx",
            "SynprezFM_29.syx", "SynprezFM_30.syx", "SynprezFM_31.syx", "SynprezFM_32.syx"
        };

        std::vector<uint8_t> zipData(ft2_dexed_builtin_pgm_zip, ft2_dexed_builtin_pgm_zip + ft2_dexed_builtin_pgm_zip_len);
        for (const char *name : kEmbeddedCartridges) {
            std::vector<uint8_t> syxData;
            if (extract_zip_entry(zipData, name, syxData))
                load_dx7_cartridge_bytes(name, syxData.data(), syxData.size(), g_factory_patches, g_factory_patch_names);
        }

        if (!g_factory_patches.empty()) {
            DX_DEBUG("Loaded %zu factory presets from embedded builtin_pgm.zip", g_factory_patches.size());
            return;
        }
    }
#endif

    const std::string modulePath = synthLib::getModulePath();
    const std::string runtimeDir = modulePath.empty() ? std::string() : (modulePath + "/Dexed/");
    const char* listPath = "src/dexed/patches/patches.txt";
    std::ifstream listifs(listPath);
    if (!listifs && !runtimeDir.empty())
        listifs.open(runtimeDir + "patches.txt");

    if (listifs) {
        DX_DEBUG("Found patches.txt, loading cartridges...");
        std::string line;
        std::string baseDir = "src/dexed/patches/";
        if (!runtimeDir.empty() && std::ifstream(runtimeDir + "patches.txt"))
            baseDir = runtimeDir;

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
        const char* syxPath = "src/dexed/patches/Dexed_01.syx";
        DX_DEBUG("Loading bundled %s", syxPath);
        load_dx7_cartridge(syxPath, g_factory_patches, g_factory_patch_names);
        if (g_factory_patches.empty() && !runtimeDir.empty()) {
            const std::string runtimeSyx = runtimeDir + "Dexed_01.syx";
            DX_DEBUG("Loading runtime bundled %s", runtimeSyx.c_str());
            load_dx7_cartridge(runtimeSyx.c_str(), g_factory_patches, g_factory_patch_names);
        }
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
