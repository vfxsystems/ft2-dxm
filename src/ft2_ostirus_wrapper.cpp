#include "ft2_ostirus.h"
#include "ft2_header.h"
#include "ft2_replayer.h"
#include "synthLib/os.h"

#include "gearmulator/source/baseLib/filesystem.h"
#include "gearmulator/source/synthLib/audioTypes.h"
#include "gearmulator/source/synthLib/device.h"
#include "gearmulator/source/synthLib/midiTypes.h"
#include "gearmulator/source/virusLib/device.h"
#include "gearmulator/source/virusLib/deviceModel.h"
#include "gearmulator/source/virusLib/microcontrollerTypes.h"
#include "gearmulator/source/virusLib/romfile.h"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <exception>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifndef MAX_INST
#define MAX_INST 128
#endif

#define OSTI_DEBUG_ENABLED 1

#if OSTI_DEBUG_ENABLED
#define OSTI_DEBUG(fmt, ...) do { std::printf("[OSTIRUS] " fmt "\n", ##__VA_ARGS__); std::fflush(stdout); } while (0)
#else
#define OSTI_DEBUG(fmt, ...)
#endif

struct OsTirusSlot
{
    std::unique_ptr<virusLib::Device> device;
    int instrID = 0;
    int midiChannel = 0;
    int currentPreset = -1;
    int activeNotes = 0;
    std::array<float, 256> paramCache{};

    OsTirusSlot()
    {
        paramCache.fill(0.5f);
    }
};

struct OsTirusPresetEntry
{
    int bank = -1;
    int program = -1;
    int category1 = 0;
    int category2 = 0;
    std::string name;
    std::string displayName;
};

static const char *const k_ostirus_category_names[FT2_OSTIRUS_MAX_CATEGORIES] =
{
    "--", "Lead", "Bass", "Pad", "Decay", "Pluck", "Acid", "Classic", "Arpeggiator", "Effects",
    "Drums", "Percussion", "Input", "Vocoder", "Favourite 1", "Favourite 2", "Favourite 3",
    "Organ", "Piano", "String", "FM", "Digital", "Atomizer"
};

static bool g_initialized = false;
static int g_sampleRate = 44100;
static std::vector<uint8_t> g_romData;
static std::vector<uint8_t> g_patchDbData;
static std::string g_romPath;
static virusLib::DeviceModel g_romModel = virusLib::DeviceModel::Invalid;
static std::vector<OsTirusPresetEntry> g_presetEntries;
static int g_defaultPresetIndex = -1;
static std::array<OsTirusSlot, FT2_OSTIRUS_MAX_SLOTS> g_slots;
static std::array<int, MAX_INST> g_instrToSlot;

static const uint32_t k_ostirus_state_magic = 0x5349544Fu; /* OTIS */
static const uint32_t k_ostirus_state_version = 1;

static bool read_file(const std::string &path, std::vector<uint8_t> &out);
static void scan_presets_if_needed(void);

static void format_rom_bank_name(int bank, char *out, size_t outSize)
{
    if (!out || outSize == 0)
        return;

    if (bank < 0)
    {
        std::snprintf(out, outSize, "ROM ?");
        return;
    }

    char suffix[16];
    int suffixLen = 0;
    int value = bank;

    do
    {
        suffix[suffixLen++] = (char)('A' + (value % 26));
        value = (value / 26) - 1;
    }
    while (value >= 0 && suffixLen < (int)(sizeof(suffix) - 1));

    for (int i = 0; i < suffixLen / 2; ++i)
    {
        const char tmp = suffix[i];
        suffix[i] = suffix[suffixLen - 1 - i];
        suffix[suffixLen - 1 - i] = tmp;
    }
    suffix[suffixLen] = '\0';

    std::snprintf(out, outSize, "ROM %s", suffix);
}

static int max_factory_bank_index(void)
{
    scan_presets_if_needed();

    int maxBank = 0;
    for (const OsTirusPresetEntry &entry : g_presetEntries)
    {
        if (entry.bank > maxBank)
            maxBank = entry.bank;
    }

    return maxBank;
}

namespace
{
    static uint16_t read_u16(const uint8_t *p)
    {
        return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
    }

    static uint32_t read_u32(const uint8_t *p)
    {
        return static_cast<uint32_t>(p[0]) |
               (static_cast<uint32_t>(p[1]) << 8) |
               (static_cast<uint32_t>(p[2]) << 16) |
               (static_cast<uint32_t>(p[3]) << 24);
    }

    static std::string to_lower_copy(std::string value)
    {
        for (char &ch : value)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return value;
    }

    static std::string basename_copy(const std::string &path)
    {
        const size_t pos = path.find_last_of("/\\");
        if (pos == std::string::npos)
            return path;
        return path.substr(pos + 1);
    }

    static bool inflate_raw_deflate(const uint8_t *input, size_t inputSize, std::vector<uint8_t> &out, size_t expectedSize)
    {
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

    static bool extract_zip_entry(const std::vector<uint8_t> &zipData, const std::string &wantedName, std::vector<uint8_t> &out)
    {
        if (zipData.size() < 22)
            return false;

        const std::string wantedLower = to_lower_copy(basename_copy(wantedName));

        const size_t maxBackSearch = std::min<size_t>(zipData.size(), 0x10000u + 22u);
        const size_t searchBegin = zipData.size() - maxBackSearch;
        size_t eocdPos = std::string::npos;

        for (size_t pos = zipData.size() - 22; pos >= searchBegin; --pos)
        {
            if (read_u32(&zipData[pos]) == 0x06054b50u)
            {
                eocdPos = pos;
                break;
            }

            if (pos == searchBegin)
                break;
        }

        if (eocdPos == std::string::npos)
            return false;

        const uint16_t totalEntries = read_u16(&zipData[eocdPos + 10]);
        const uint32_t centralDirSize = read_u32(&zipData[eocdPos + 12]);
        const uint32_t centralDirOffset = read_u32(&zipData[eocdPos + 16]);

        if (centralDirOffset > zipData.size() || centralDirSize > (zipData.size() - centralDirOffset))
            return false;

        size_t dirPos = centralDirOffset;
        for (uint16_t entryIdx = 0; entryIdx < totalEntries; ++entryIdx)
        {
            if (dirPos > zipData.size() || 46 > (zipData.size() - dirPos) || read_u32(&zipData[dirPos]) != 0x02014b50u)
                return false;

            const uint16_t method = read_u16(&zipData[dirPos + 10]);
            const uint32_t compSize = read_u32(&zipData[dirPos + 20]);
            const uint32_t uncompSize = read_u32(&zipData[dirPos + 24]);
            const uint16_t fileNameLen = read_u16(&zipData[dirPos + 28]);
            const uint16_t extraLen = read_u16(&zipData[dirPos + 30]);
            const uint16_t commentLen = read_u16(&zipData[dirPos + 32]);
            const uint32_t localHeaderOffset = read_u32(&zipData[dirPos + 42]);

            const size_t namePos = dirPos + 46;
            const size_t nextPos = namePos + fileNameLen + extraLen + commentLen;
            if (nextPos > zipData.size())
                return false;

            std::string entryName(reinterpret_cast<const char *>(&zipData[namePos]), fileNameLen);
            if (to_lower_copy(basename_copy(entryName)) == wantedLower)
            {
                if (localHeaderOffset > zipData.size() || 30 > (zipData.size() - localHeaderOffset) || read_u32(&zipData[localHeaderOffset]) != 0x04034b50u)
                    return false;

                const uint16_t localNameLen = read_u16(&zipData[localHeaderOffset + 26]);
                const uint16_t localExtraLen = read_u16(&zipData[localHeaderOffset + 28]);
                const size_t dataOffset = localHeaderOffset + 30 + localNameLen + localExtraLen;

                if (dataOffset > zipData.size() || compSize > (zipData.size() - dataOffset))
                    return false;

                const uint8_t *compressed = &zipData[dataOffset];
                if (method == 0)
                {
                    out.assign(compressed, compressed + compSize);
                    return out.size() == uncompSize || uncompSize == 0;
                }

                if (method == 8)
                    return inflate_raw_deflate(compressed, compSize, out, uncompSize);

                return false;
            }

            dirPos = nextPos;
        }

        return false;
    }

    static bool load_zip_payload(const std::string &zipPath, const std::string &entryName, std::vector<uint8_t> &out)
    {
        std::vector<uint8_t> zipBytes;
        if (!read_file(zipPath, zipBytes))
            return false;

        if (!extract_zip_entry(zipBytes, entryName, out))
            return false;

        return true;
    }

    static std::vector<std::string> build_asset_roots(void)
    {
        std::vector<std::string> roots;
        auto addRoot = [&](std::string root)
        {
            if (root.empty())
                return;

            for (char &ch : root)
            {
                if (ch == '\\')
                    ch = '/';
            }

            while (!root.empty() && root.back() == '/')
                root.pop_back();

            if (std::find(roots.begin(), roots.end(), root) == roots.end())
                roots.push_back(std::move(root));
        };

        addRoot(synthLib::getModulePath());
        addRoot(synthLib::getModulePath(false));
        addRoot(baseLib::filesystem::getCurrentDirectory());
        return roots;
    }

    static bool try_load_asset_zip(std::vector<uint8_t> &romOut, std::vector<uint8_t> &cacheOut, std::string &romPathOut)
    {
        const std::vector<std::string> roots = build_asset_roots();
        const char *zipCandidates[] = {
            "src/gearmulator/assets/otstirusdat.zip",
            "src/gearmulator/assets/ostirusdat.zip",
            "src/gearmulator/assets/OsTIrus/otstirusdat.zip",
            "src/gearmulator/assets/OsTIrus/ostirusdat.zip",
            "../../../src/gearmulator/assets/otstirusdat.zip",
            "../../../src/gearmulator/assets/ostirusdat.zip",
            "../../src/gearmulator/assets/otstirusdat.zip",
            "../../src/gearmulator/assets/ostirusdat.zip",
            "../src/gearmulator/assets/otstirusdat.zip",
            "../src/gearmulator/assets/ostirusdat.zip"
        };

        for (const std::string &root : roots)
        {
            for (const char *candidate : zipCandidates)
            {
                const std::string zipPath = root.empty() ? std::string(candidate) : (root + "/" + candidate);
                if (!load_zip_payload(zipPath, "rom.bin", romOut))
                    continue;

                (void)load_zip_payload(zipPath, "patchdb.cache", cacheOut);
                if (cacheOut.empty())
                    (void)load_zip_payload(zipPath, "patchmanagerdb.cache", cacheOut);

                romPathOut = zipPath + "::rom.bin";
                OSTI_DEBUG("Loaded embedded ROM from '%s' (%zu bytes)", zipPath.c_str(), romOut.size());
                if (!cacheOut.empty())
                    OSTI_DEBUG("Loaded embedded patch cache from '%s' (%zu bytes)", zipPath.c_str(), cacheOut.size());
                return true;
            }
        }

        return false;
    }
}

static inline int instr_index(int instrID)
{
    if (instrID < 1 || instrID > MAX_INST)
        return -1;
    return instrID - 1;
}

static bool read_file(const std::string &path, std::vector<uint8_t> &out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;

    f.seekg(0, std::ios::end);
    const std::streamoff len = f.tellg();
    if (len <= 0)
        return false;
    f.seekg(0, std::ios::beg);

    out.resize(static_cast<size_t>(len));
    f.read(reinterpret_cast<char *>(out.data()), len);
    return f.good();
}

static std::string to_upper_copy(std::string value)
{
    for (char &ch : value)
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return value;
}

static bool contains_case_insensitive(const std::string &haystack, const std::string &needle)
{
    if (needle.empty())
        return true;
    return to_upper_copy(haystack).find(to_upper_copy(needle)) != std::string::npos;
}

static bool load_rom_if_needed(void)
{
    if (!g_romData.empty() && g_romModel != virusLib::DeviceModel::Invalid)
        return true;

    std::vector<uint8_t> zipRom;
    std::vector<uint8_t> zipCache;
    std::string zipRomPath;
    if (try_load_asset_zip(zipRom, zipCache, zipRomPath))
    {
        static const virusLib::DeviceModel models[] = {
            virusLib::DeviceModel::TI,
            virusLib::DeviceModel::TI2,
            virusLib::DeviceModel::Snow
        };

        for (virusLib::DeviceModel model : models)
        {
            virusLib::ROMFile rom(zipRom, zipRomPath, model);
            if (!rom.isValid())
                continue;

            g_romData = std::move(zipRom);
            g_patchDbData = std::move(zipCache);
            g_romPath = zipRomPath;
            g_romModel = model;
            OSTI_DEBUG("Loaded ROM '%s' as model %s (%zu bytes)", zipRomPath.c_str(), virusLib::getModelName(model).c_str(), g_romData.size());
            return true;
        }
    }

    static const char *kRomCandidates[] = {
        "src/gearmulator/assets/OsTIrus/roms/rom.bin",
        "src/gearmulator/assets/OsTIrus/roms/Access Virus TI firmware - Copy.bin",
        "src/gearmulator/assets/OsTIrus/roms/Access Virus TI firmware.bin",
        "../src/gearmulator/assets/OsTIrus/roms/rom.bin",
        "../src/gearmulator/assets/OsTIrus/roms/Access Virus TI firmware - Copy.bin",
        "../src/gearmulator/assets/OsTIrus/roms/Access Virus TI firmware.bin",
        "../../src/gearmulator/assets/OsTIrus/roms/rom.bin",
        "../../src/gearmulator/assets/OsTIrus/roms/Access Virus TI firmware - Copy.bin",
        "../../src/gearmulator/assets/OsTIrus/roms/Access Virus TI firmware.bin",
        "OsTIrus/roms/rom.bin",
        "OsTIrus/roms/Access Virus TI firmware - Copy.bin",
        "OsTIrus/roms/Access Virus TI firmware.bin"
    };

    for (const char *path : kRomCandidates)
    {
        std::vector<uint8_t> data;
        if (!read_file(path, data))
            continue;

        static const virusLib::DeviceModel models[] = {
            virusLib::DeviceModel::TI,
            virusLib::DeviceModel::TI2,
            virusLib::DeviceModel::Snow
        };

        for (virusLib::DeviceModel model : models)
        {
            virusLib::ROMFile rom(data, path, model);
            if (!rom.isValid())
                continue;

            g_romData = std::move(data);
            g_romPath = path;
            g_romModel = model;
            OSTI_DEBUG("Loaded ROM '%s' as model %s (%zu bytes)", path, virusLib::getModelName(model).c_str(), g_romData.size());
            return true;
        }
    }

    OSTI_DEBUG("No valid OsTIrus ROM found");
    return false;
}

static virusLib::ROMFile make_rom(void)
{
    return virusLib::ROMFile(g_romData, g_romPath.empty() ? "Access Virus TI firmware.bin" : g_romPath, g_romModel);
}

static void scan_presets_if_needed(void)
{
    if (!g_presetEntries.empty())
        return;
    if (!load_rom_if_needed())
        return;

    virusLib::ROMFile rom = make_rom();
    if (!rom.isValid())
        return;

    const uint32_t bankCount = rom.getNumSingleBanks();
    const uint32_t presetsPerBank = rom.getPresetsPerBank();
    g_presetEntries.reserve(bankCount * presetsPerBank);
    g_defaultPresetIndex = -1;

    for (uint32_t bank = 0; bank < bankCount; ++bank)
    {
        for (uint32_t program = 0; program < presetsPerBank; ++program)
        {
            virusLib::ROMFile::TPreset preset;
            if (!rom.getSingle(static_cast<int>(bank), static_cast<int>(program), preset))
                continue;

            OsTirusPresetEntry entry;
            entry.bank = static_cast<int>(bank);
            entry.program = static_cast<int>(program);
            entry.category1 = (preset.size() > 123) ? preset[123] : 0;
            entry.category2 = (preset.size() > 124) ? preset[124] : 0;

            entry.name = virusLib::ROMFile::getSingleName(preset);
            if (entry.name.empty())
                entry.name = "Init";

            char bankLabel[32];
            char label[96];
            format_rom_bank_name((int)bank, bankLabel, sizeof(bankLabel));
            std::snprintf(label, sizeof(label), "%s %02u %s",
                          bankLabel,
                          static_cast<unsigned>(program + 1),
                          entry.name.c_str());
            entry.displayName = label;

            if (g_defaultPresetIndex < 0 && contains_case_insensitive(entry.name, "TI BC"))
                g_defaultPresetIndex = static_cast<int>(g_presetEntries.size());

            g_presetEntries.emplace_back(std::move(entry));
        }
    }

    if (g_defaultPresetIndex < 0 && !g_presetEntries.empty())
        g_defaultPresetIndex = 0;

    OSTI_DEBUG("Scanned %zu ROM presets, default preset index=%d", g_presetEntries.size(), g_defaultPresetIndex);
}

static const OsTirusPresetEntry *preset_entry_for_index(int index)
{
    scan_presets_if_needed();
    if (index < 0 || index >= static_cast<int>(g_presetEntries.size()))
        return nullptr;
    return &g_presetEntries[static_cast<size_t>(index)];
}

static int default_preset_index(void)
{
    scan_presets_if_needed();
    if (g_defaultPresetIndex >= 0 && g_defaultPresetIndex < static_cast<int>(g_presetEntries.size()))
        return g_defaultPresetIndex;
    return g_presetEntries.empty() ? -1 : 0;
}

static int param_raw_max(int paramId)
{
    switch (paramId)
    {
        case 17:
        case 19:
        case 22:
        case 24:
            return 127;

        case 32:
            return max_factory_bank_index();

        case 35:
            return 1;

        case 51:
            return 7;

        case 52:
        case 53:
            return 3;

        case 64:
        case 65:
        case 66:
            return 1;

        case 68:
        case 80:
            return 67;

        case 69:
        case 70:
        case 73:
        case 81:
        case 82:
        case 85:
            return 1;

        case 94:
            return 5;

        case 102:
            return 3;

        case 103:
            return 6;

        case 104:
            return 15;

        case 106:
            return 3;

        case 107:
            return 16;

        case 108:
            return 3;

        case 109:
            return 1;

        case 110:
        case 118:
            return 5;

        case 112:
            return 22;
    }

    if (paramId >= 192 && paramId <= 239)
    {
        const int slotParam = (paramId - 192) % 3;
        if (slotParam == 0)
            return 31; /* mod source */
        if (slotParam == 2)
            return 35; /* mod destination */
    }

    return 127;
}

static float normalize_param_value_from_raw(int paramId, uint8_t rawValue)
{
    const int rawMax = param_raw_max(paramId);
    if (rawMax <= 0)
        return 0.0f;

    float value = (float)rawValue / (float)rawMax;
    if (value < 0.0f)
        value = 0.0f;
    else if (value > 1.0f)
        value = 1.0f;
    return value;
}

static uint8_t denormalize_param_value_to_raw(int paramId, float value)
{
    if (value < 0.0f)
        value = 0.0f;
    else if (value > 1.0f)
        value = 1.0f;

    const int rawMax = param_raw_max(paramId);
    const int raw = (int)std::lround(value * (float)rawMax);
    return static_cast<uint8_t>(std::clamp(raw, 0, rawMax));
}

static void populate_param_cache_from_preset(OsTirusSlot *slot, const virusLib::ROMFile::TPreset &preset)
{
    if (!slot)
        return;

    slot->paramCache.fill(0.0f);

    const size_t paramCount = slot->paramCache.size();
    for (size_t paramId = 0; paramId < paramCount; ++paramId)
    {
        size_t presetOffset = paramId;
        if (presetOffset >= preset.size())
            break;

        slot->paramCache[paramId] = normalize_param_value_from_raw((int)paramId, preset[presetOffset]);
    }
}

static void push_midi(std::vector<synthLib::SMidiEvent> &events, int chan, uint8_t status, uint8_t data1, uint8_t data2)
{
    const uint8_t type = status & 0xF0;
    events.emplace_back(synthLib::MidiEventSource::Host,
                        static_cast<uint8_t>(type | (chan & 0x0F)),
                        data1, data2, 0);
}

static uint8_t page_for_param_id(int paramId)
{
    switch (paramId / 128)
    {
        case 0: return virusLib::PAGE_A;
        case 1: return virusLib::PAGE_B;
        case 2: return virusLib::PAGE_6E;
        case 3: return virusLib::PAGE_6F;
        default: return virusLib::PAGE_A;
    }
}

static void push_param_change_sysex(std::vector<synthLib::SMidiEvent> &events, int part, int paramId, uint8_t value)
{
    synthLib::SMidiEvent ev(synthLib::MidiEventSource::Internal);
    ev.sysex = {
        synthLib::M_STARTOFSYSEX,
        0x00, 0x20, 0x33, 0x01,
        virusLib::OMNI_DEVICE_ID,
        page_for_param_id(paramId),
        static_cast<uint8_t>(part & 0x7F),
        static_cast<uint8_t>(paramId & 0x7F),
        value,
        synthLib::M_ENDOFSYSEX
    };
    events.emplace_back(std::move(ev));
}

static void process_slot_events(OsTirusSlot *slot, const std::vector<synthLib::SMidiEvent> &events, std::vector<synthLib::SMidiEvent> &response)
{
    if (!slot || !slot->device)
        return;

    std::array<float, 1> zeroInL{};
    std::array<float, 1> zeroInR{};
    std::array<std::array<float, 1>, 12> zeroOut{};

    synthLib::TAudioInputs inputs = {
        zeroInL.data(),
        zeroInR.data(),
        nullptr,
        nullptr
    };

    synthLib::TAudioOutputs outputs = {
        zeroOut[0].data(), zeroOut[1].data(),
        zeroOut[2].data(), zeroOut[3].data(),
        zeroOut[4].data(), zeroOut[5].data(),
        zeroOut[6].data(), zeroOut[7].data(),
        zeroOut[8].data(), zeroOut[9].data(),
        zeroOut[10].data(), zeroOut[11].data()
    };

    slot->device->process(inputs, outputs, 1, events, response);
}

static bool apply_cached_params_to_slot(OsTirusSlot *slot, int instrID)
{
    if (!slot || !slot->device)
        return false;

    for (int paramId = 0; paramId < static_cast<int>(slot->paramCache.size()); ++paramId)
    {
        const float value = std::clamp(slot->paramCache[static_cast<size_t>(paramId)], 0.0f, 1.0f);
        slot->paramCache[static_cast<size_t>(paramId)] = value;

        if (paramId >= 240 && paramId <= 243)
            continue;

        const uint8_t rawValue = denormalize_param_value_to_raw(paramId, value);
        std::vector<synthLib::SMidiEvent> events;
        std::vector<synthLib::SMidiEvent> response;

        if (paramId <= 127)
            push_midi(events, slot->midiChannel, 0xB0, static_cast<uint8_t>(paramId & 0x7F), rawValue);
        else
            push_param_change_sysex(events, 0, paramId, rawValue);

        process_slot_events(slot, events, response);
    }

    if (instrID >= 1 && instrID <= MAX_INST && instr[instrID])
    {
        instr[instrID]->useOsTirus = true;
        instr[instrID]->osTirusSlot = static_cast<uint8_t>(slot->midiChannel);
    }

    return true;
}

static OsTirusSlot *slot_for_instr(int instrID, bool create)
{
    const int idx = instr_index(instrID);
    if (idx < 0 || !g_initialized)
        return nullptr;

    const int mappedSlot = g_instrToSlot[idx];
    if (mappedSlot >= 0 && mappedSlot < FT2_OSTIRUS_MAX_SLOTS)
    {
        OsTirusSlot &slot = g_slots[static_cast<size_t>(mappedSlot)];
        if (slot.device && slot.instrID == instrID)
            return &slot;
    }

    for (int i = 0; i < FT2_OSTIRUS_MAX_SLOTS; ++i)
    {
        OsTirusSlot &slot = g_slots[static_cast<size_t>(i)];
        if (slot.instrID == instrID && slot.device)
        {
            g_instrToSlot[idx] = i;
            return &slot;
        }
    }

    if (!create || !load_rom_if_needed())
        return nullptr;

    int preferredSlot = -1;
    if (instr[instrID] && instr[instrID]->osTirusSlot != 0xFF)
    {
        const int preferred = static_cast<int>(instr[instrID]->osTirusSlot);
        if (preferred >= 0 && preferred < FT2_OSTIRUS_MAX_SLOTS)
            preferredSlot = preferred;
    }

    for (int pass = 0; pass < 2; ++pass)
    {
        const int start = (pass == 0 && preferredSlot >= 0) ? preferredSlot : 0;
        const int end = (pass == 0 && preferredSlot >= 0) ? (preferredSlot + 1) : FT2_OSTIRUS_MAX_SLOTS;

        for (int i = start; i < end; ++i)
        {
            OsTirusSlot &slot = g_slots[static_cast<size_t>(i)];
            if (slot.instrID != 0 || slot.device)
                continue;

            synthLib::DeviceCreateParams params;
            params.hostSamplerate = static_cast<float>(g_sampleRate);
            params.preferredSamplerate = static_cast<float>(g_sampleRate);
            params.romName = g_romPath;
            params.romData = g_romData;
            params.customData = static_cast<uint32_t>(g_romModel);
            params.homePath = "OsTIrus";

            try
            {
                slot.device = std::make_unique<virusLib::Device>(params, false);
            }
            catch (const std::exception &e)
            {
                OSTI_DEBUG("Device creation failed for instr %d: %s", instrID, e.what());
                return nullptr;
            }
            catch (...)
            {
                OSTI_DEBUG("Device creation failed for instr %d: unknown exception", instrID);
                return nullptr;
            }

            if (!slot.device || !slot.device->isValid())
            {
                slot.device.reset();
                OSTI_DEBUG("Device invalid for instr %d", instrID);
                return nullptr;
            }

            slot.instrID = instrID;
            slot.midiChannel = i & 0x0F;
            slot.currentPreset = -1;
            slot.activeNotes = 0;
            slot.paramCache.fill(0.5f);
            g_instrToSlot[idx] = i;

            OSTI_DEBUG("Created slot %d for FT2 instrument %d", i, instrID);

            int presetIndex = -1;
            if (instr[instrID] && instr[instrID]->osTirusPreset != 0xFFFF)
                presetIndex = static_cast<int>(instr[instrID]->osTirusPreset);
            else
                presetIndex = default_preset_index();

            if (presetIndex >= 0)
                ft2_ostirus_load_factory_preset_for_instrument(instrID, presetIndex);

            if (instr[instrID])
                instr[instrID]->osTirusSlot = static_cast<uint8_t>(slot.midiChannel);

            return &slot;
        }
    }

    OSTI_DEBUG("No free OsTIrus slots for instr %d", instrID);
    return nullptr;
}

static bool apply_preset_to_slot(OsTirusSlot *slot, int instrID, int presetIndex)
{
    scan_presets_if_needed();
    if (!slot || !slot->device)
        return false;
    if (presetIndex < 0 || presetIndex >= static_cast<int>(g_presetEntries.size()))
        return false;

    const OsTirusPresetEntry &entry = g_presetEntries[static_cast<size_t>(presetIndex)];
    virusLib::ROMFile::TPreset preset{};
    virusLib::ROMFile rom = make_rom();
    if (!rom.isValid() || !rom.getSingle(entry.bank, entry.program, preset))
        return false;

    std::vector<synthLib::SMidiEvent> events;
    std::vector<synthLib::SMidiEvent> midiOut;
    push_midi(events, slot->midiChannel, 0xB0, 0, 0);
    push_midi(events, slot->midiChannel, 0xB0, 32, static_cast<uint8_t>(entry.bank & 0x7F));
    push_midi(events, slot->midiChannel, 0xC0, static_cast<uint8_t>(entry.program & 0x7F), 0);
    process_slot_events(slot, events, midiOut);
    populate_param_cache_from_preset(slot, preset);
    slot->currentPreset = presetIndex;

    if (instrID >= 1 && instrID <= MAX_INST && instr[instrID])
    {
        instr[instrID]->osTirusPreset = static_cast<uint16_t>(presetIndex);
        instr[instrID]->osTirusSlot = static_cast<uint8_t>(slot->midiChannel);
    }

    return true;
}

static int current_preset_index_for_instr(int instrID)
{
    const OsTirusSlot *slot = slot_for_instr(instrID, false);
    if (slot && slot->currentPreset >= 0)
        return slot->currentPreset;

    if (instrID >= 1 && instrID <= MAX_INST && instr[instrID] && instr[instrID]->osTirusPreset != 0xFFFF)
        return static_cast<int>(instr[instrID]->osTirusPreset);

    return -1;
}

extern "C" {

void ft2_ostirus_init(int samplerate)
{
    g_sampleRate = samplerate > 0 ? samplerate : 44100;
    g_patchDbData.clear();
    g_initialized = load_rom_if_needed();
    g_presetEntries.clear();
    g_defaultPresetIndex = -1;
    g_instrToSlot.fill(-1);

    for (auto &slot : g_slots)
        slot = OsTirusSlot();

    if (g_initialized)
        scan_presets_if_needed();
}

void ft2_ostirus_shutdown(void)
{
    ft2_ostirus_panic();
    for (auto &slot : g_slots)
        slot = OsTirusSlot();
    g_instrToSlot.fill(-1);
    g_presetEntries.clear();
    g_defaultPresetIndex = -1;
    g_romData.clear();
    g_patchDbData.clear();
    g_romPath.clear();
    g_romModel = virusLib::DeviceModel::Invalid;
    g_initialized = false;
}

bool ft2_ostirus_is_initialized(void)
{
    return g_initialized;
}

void ft2_ostirus_render_for_channel(int instrID, float *bufL, float *bufR, int nsamples, int add)
{
    if (!bufL || !bufR || nsamples <= 0)
        return;

    if (!add)
    {
        std::memset(bufL, 0, sizeof(float) * static_cast<size_t>(nsamples));
        std::memset(bufR, 0, sizeof(float) * static_cast<size_t>(nsamples));
    }

    OsTirusSlot *slot = slot_for_instr(instrID, true);
    if (!slot || !slot->device)
        return;

    std::vector<float> zero(static_cast<size_t>(nsamples), 0.0f);
    std::array<std::vector<float>, 12> outStorage;
    for (auto &out : outStorage)
        out.assign(static_cast<size_t>(nsamples), 0.0f);

    synthLib::TAudioInputs inputs = { zero.data(), zero.data(), nullptr, nullptr };
    synthLib::TAudioOutputs outputs = {
        outStorage[0].data(), outStorage[1].data(),
        outStorage[2].data(), outStorage[3].data(),
        outStorage[4].data(), outStorage[5].data(),
        outStorage[6].data(), outStorage[7].data(),
        outStorage[8].data(), outStorage[9].data(),
        outStorage[10].data(), outStorage[11].data()
    };

    std::vector<synthLib::SMidiEvent> midiIn;
    std::vector<synthLib::SMidiEvent> midiOut;
    slot->device->process(inputs, outputs, static_cast<size_t>(nsamples), midiIn, midiOut);

    for (int i = 0; i < nsamples; ++i)
    {
        float l = 0.0f;
        float r = 0.0f;
        for (int pair = 0; pair < 6; ++pair)
        {
            l += outStorage[static_cast<size_t>(pair * 2)][static_cast<size_t>(i)];
            r += outStorage[static_cast<size_t>(pair * 2 + 1)][static_cast<size_t>(i)];
        }
        bufL[i] += l;
        bufR[i] += r;
    }
}

void ft2_ostirus_render(float *bufL, float *bufR, int nsamples, int add)
{
    if (!bufL || !bufR || nsamples <= 0)
        return;

    if (!add)
    {
        std::memset(bufL, 0, sizeof(float) * static_cast<size_t>(nsamples));
        std::memset(bufR, 0, sizeof(float) * static_cast<size_t>(nsamples));
    }

    for (const auto &slot : g_slots)
    {
        if (slot.instrID > 0 && slot.device && instr[slot.instrID] && instr[slot.instrID]->useOsTirus)
            ft2_ostirus_render_for_channel(slot.instrID, bufL, bufR, nsamples, 1);
    }
}

void ft2_ostirus_send_midi_to_instrument(int instrID, uint8_t status, uint8_t data1, uint8_t data2)
{
    OsTirusSlot *slot = slot_for_instr(instrID, true);
    if (!slot || !slot->device)
        return;

    std::vector<synthLib::SMidiEvent> events;
    std::vector<synthLib::SMidiEvent> response;
    push_midi(events, slot->midiChannel, status, data1, data2);

    const uint8_t type = status & 0xF0;
    if (type == 0x90 && data2 != 0)
        slot->activeNotes++;
    else if (type == 0x80 || (type == 0x90 && data2 == 0))
    {
        if (slot->activeNotes > 0)
            slot->activeNotes--;
    }

    process_slot_events(slot, events, response);
}

void ft2_ostirus_panic(void)
{
    for (auto &slot : g_slots)
    {
        if (!slot.device || slot.instrID <= 0)
            continue;

        ft2_ostirus_send_midi_to_instrument(slot.instrID, static_cast<uint8_t>(0xB0 | slot.midiChannel), 123, 0);
        ft2_ostirus_send_midi_to_instrument(slot.instrID, static_cast<uint8_t>(0xB0 | slot.midiChannel), 120, 0);
        slot.activeNotes = 0;
    }
}

int ft2_ostirus_get_factory_preset_count(void)
{
    scan_presets_if_needed();
    return static_cast<int>(g_presetEntries.size());
}

const char *ft2_ostirus_get_factory_preset_name(int index)
{
    scan_presets_if_needed();
    if (index < 0 || index >= static_cast<int>(g_presetEntries.size()))
        return nullptr;
    return g_presetEntries[static_cast<size_t>(index)].displayName.c_str();
}

const char *ft2_ostirus_get_factory_preset_plain_name(int index)
{
    scan_presets_if_needed();
    if (index < 0 || index >= static_cast<int>(g_presetEntries.size()))
        return nullptr;
    return g_presetEntries[static_cast<size_t>(index)].name.c_str();
}

int ft2_ostirus_get_factory_preset_bank(int index)
{
    const OsTirusPresetEntry *entry = preset_entry_for_index(index);
    return entry ? entry->bank : -1;
}

int ft2_ostirus_get_factory_preset_program(int index)
{
    const OsTirusPresetEntry *entry = preset_entry_for_index(index);
    return entry ? entry->program : -1;
}

int ft2_ostirus_get_factory_preset_category1(int index)
{
    const OsTirusPresetEntry *entry = preset_entry_for_index(index);
    return entry ? entry->category1 : 0;
}

int ft2_ostirus_get_factory_preset_category2(int index)
{
    const OsTirusPresetEntry *entry = preset_entry_for_index(index);
    return entry ? entry->category2 : 0;
}

const char *ft2_ostirus_get_category_name(int categoryIndex)
{
    if (categoryIndex < 0 || categoryIndex >= FT2_OSTIRUS_MAX_CATEGORIES)
        return k_ostirus_category_names[0];
    return k_ostirus_category_names[categoryIndex];
}

const char *ft2_ostirus_get_rom_model_name(void)
{
    static std::string modelName;
    modelName = virusLib::getModelName(g_romModel);
    if (modelName.empty())
        modelName = "No ROM";
    return modelName.c_str();
}

int ft2_ostirus_find_factory_preset(int bank, int program)
{
    scan_presets_if_needed();

    for (size_t i = 0; i < g_presetEntries.size(); ++i)
    {
        const OsTirusPresetEntry &entry = g_presetEntries[i];
        if (entry.bank == bank && entry.program == program)
            return static_cast<int>(i);
    }

    return -1;
}

int ft2_ostirus_get_default_factory_preset_index(void)
{
    return default_preset_index();
}

int ft2_ostirus_load_factory_preset_for_instrument(int instrID, int index)
{
    OsTirusSlot *slot = slot_for_instr(instrID, true);
    if (!slot || !slot->device)
        return 0;
    return apply_preset_to_slot(slot, instrID, index) ? 1 : 0;
}

int ft2_ostirus_get_current_preset_for_instrument(int instrID)
{
    return current_preset_index_for_instr(instrID);
}

const char *ft2_ostirus_get_current_preset_name_for_instrument(int instrID)
{
    const OsTirusPresetEntry *entry = preset_entry_for_index(current_preset_index_for_instr(instrID));
    return entry ? entry->displayName.c_str() : nullptr;
}

void ft2_ostirus_set_param_for_instrument(int instrID, int paramId, float value)
{
    OsTirusSlot *slot = slot_for_instr(instrID, true);
    if (!slot || !slot->device)
        return;

    if (paramId < 0 || paramId >= static_cast<int>(slot->paramCache.size()))
        return;

    if (value < 0.0f)
        value = 0.0f;
    else if (value > 1.0f)
        value = 1.0f;

    slot->paramCache[static_cast<size_t>(paramId)] = value;

    if (paramId >= 240 && paramId <= 243)
        return;

    const uint8_t ccValue = denormalize_param_value_to_raw(paramId, value);
    std::vector<synthLib::SMidiEvent> events;
    std::vector<synthLib::SMidiEvent> response;
    if (paramId <= 127)
    {
        push_midi(events, slot->midiChannel, 0xB0, static_cast<uint8_t>(paramId & 0x7F), ccValue);
    }
    else
    {
        push_param_change_sysex(events, 0, paramId, ccValue);
    }
    process_slot_events(slot, events, response);
}

float ft2_ostirus_get_param_for_instrument(int instrID, int paramId)
{
    OsTirusSlot *slot = slot_for_instr(instrID, true);
    if (!slot || !slot->device)
        return 0.5f;

    if (paramId < 0 || paramId >= static_cast<int>(slot->paramCache.size()))
        return 0.5f;

    return slot->paramCache[static_cast<size_t>(paramId)];
}

int ft2_ostirus_get_active_voice_count(int instrID)
{
    OsTirusSlot *slot = slot_for_instr(instrID, false);
    return slot ? slot->activeNotes : 0;
}

int ft2_ostirus_assign_slot_for_instrument(int instrID)
{
    OsTirusSlot *slot = slot_for_instr(instrID, true);
    return slot ? slot->midiChannel : -1;
}

int ft2_ostirus_get_slot_for_instrument(int instrID)
{
    OsTirusSlot *slot = slot_for_instr(instrID, false);
    if (slot)
        return slot->midiChannel;

    if (instrID >= 1 && instrID <= MAX_INST && instr[instrID] && instr[instrID]->osTirusSlot != 0xFF)
        return static_cast<int>(instr[instrID]->osTirusSlot);

    return -1;
}

void ft2_ostirus_release_instrument(int instrID)
{
    const int idx = instr_index(instrID);
    if (idx < 0)
        return;

    const int mappedSlot = g_instrToSlot[static_cast<size_t>(idx)];
    if (mappedSlot >= 0 && mappedSlot < FT2_OSTIRUS_MAX_SLOTS)
    {
        OsTirusSlot &slot = g_slots[static_cast<size_t>(mappedSlot)];
        if (slot.instrID == instrID)
            slot = OsTirusSlot();
    }

    for (OsTirusSlot &slot : g_slots)
    {
        if (slot.instrID == instrID)
            slot = OsTirusSlot();
    }

    g_instrToSlot[static_cast<size_t>(idx)] = -1;

    if (instrID >= 1 && instrID <= MAX_INST && instr[instrID])
        instr[instrID]->osTirusSlot = 0xFF;
}

size_t ft2_ostirus_serialize_state(int instrID, uint8_t *outBuf, size_t bufSize)
{
    OsTirusSlot *slot = slot_for_instr(instrID, true);
    if (!slot)
        return 0;

    const uint32_t paramCount = static_cast<uint32_t>(slot->paramCache.size());
    const size_t requiredSize = (sizeof(uint32_t) * 3) + (sizeof(float) * paramCount) + sizeof(int32_t);
    if (!outBuf)
        return requiredSize;
    if (bufSize < requiredSize)
        return 0;

    uint8_t *p = outBuf;
    auto write_u32 = [&p](uint32_t v)
    {
        p[0] = static_cast<uint8_t>(v & 0xFF);
        p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
        p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
        p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
        p += 4;
    };

    write_u32(k_ostirus_state_magic);
    write_u32(k_ostirus_state_version);
    write_u32(paramCount);
    std::memcpy(p, slot->paramCache.data(), sizeof(float) * paramCount);
    p += sizeof(float) * paramCount;

    const int32_t presetIndex = slot->currentPreset;
    std::memcpy(p, &presetIndex, sizeof(presetIndex));
    return requiredSize;
}

int ft2_ostirus_deserialize_state(int instrID, const uint8_t *data, size_t size)
{
    if (!data || size < ((sizeof(uint32_t) * 3) + (sizeof(float) * 256) + sizeof(int32_t)))
        return 0;

    const uint8_t *p = data;
    const uint32_t magic = read_u32(p); p += 4;
    const uint32_t version = read_u32(p); p += 4;
    const uint32_t paramCount = read_u32(p); p += 4;

    if (magic != k_ostirus_state_magic || version != k_ostirus_state_version || paramCount != 256)
        return 0;

    const size_t requiredSize = (sizeof(uint32_t) * 3) + (sizeof(float) * paramCount) + sizeof(int32_t);
    if (size < requiredSize)
        return 0;

    OsTirusSlot *slot = slot_for_instr(instrID, true);
    if (!slot)
        return 0;

    std::memcpy(slot->paramCache.data(), p, sizeof(float) * paramCount);
    p += sizeof(float) * paramCount;

    int32_t presetIndex = -1;
    std::memcpy(&presetIndex, p, sizeof(presetIndex));

    if (presetIndex >= 0)
    {
        if (!ft2_ostirus_load_factory_preset_for_instrument(instrID, presetIndex))
            return 0;

        slot = slot_for_instr(instrID, false);
        if (!slot)
            return 0;

        std::memcpy(slot->paramCache.data(), data + (sizeof(uint32_t) * 3), sizeof(float) * paramCount);
    }
    else
    {
        slot->currentPreset = -1;
        if (instrID >= 1 && instrID <= MAX_INST && instr[instrID])
            instr[instrID]->osTirusPreset = 0xFFFF;
    }

    return apply_cached_params_to_slot(slot, instrID) ? 1 : 0;
}

} /* extern "C" */
