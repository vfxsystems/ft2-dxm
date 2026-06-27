#include "ft2_ostirus.h"
#include "ft2_header.h"
#include "ft2_replayer.h"

#include "gearmulator/source/synthLib/audioTypes.h"
#include "gearmulator/source/synthLib/device.h"
#include "gearmulator/source/synthLib/midiTypes.h"
#include "gearmulator/source/virusLib/device.h"
#include "gearmulator/source/virusLib/deviceModel.h"
#include "gearmulator/source/virusLib/romfile.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <exception>
#include <cstdio>
#include <cstring>
#include <fstream>
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
};

struct OsTirusPresetEntry
{
    int bank = -1;
    int program = -1;
    std::string name;
    std::string displayName;
};

static bool g_initialized = false;
static int g_sampleRate = 44100;
static std::vector<uint8_t> g_romData;
static std::string g_romPath;
static virusLib::DeviceModel g_romModel = virusLib::DeviceModel::Invalid;
static std::vector<OsTirusPresetEntry> g_presetEntries;
static int g_defaultPresetIndex = -1;
static std::array<OsTirusSlot, FT2_OSTIRUS_MAX_SLOTS> g_slots;
static std::array<int, MAX_INST> g_instrToSlot;

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

            entry.name = virusLib::ROMFile::getSingleName(preset);
            if (entry.name.empty())
                entry.name = "Init";

            char label[96];
            std::snprintf(label, sizeof(label), "%c%02u %s",
                          static_cast<char>('A' + (bank % 26)),
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

static void push_midi(std::vector<synthLib::SMidiEvent> &events, int chan, uint8_t status, uint8_t data1, uint8_t data2)
{
    const uint8_t type = status & 0xF0;
    events.emplace_back(synthLib::MidiEventSource::Host,
                        static_cast<uint8_t>(type | (chan & 0x0F)),
                        data1, data2, 0);
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

    for (int i = 0; i < FT2_OSTIRUS_MAX_SLOTS; ++i)
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
    std::vector<synthLib::SMidiEvent> events;
    std::vector<synthLib::SMidiEvent> midiOut;
    push_midi(events, slot->midiChannel, 0xB0, 0, 0);
    push_midi(events, slot->midiChannel, 0xB0, 32, static_cast<uint8_t>(entry.bank & 0x7F));
    push_midi(events, slot->midiChannel, 0xC0, static_cast<uint8_t>(entry.program & 0x7F), 0);

    synthLib::TAudioInputs inputs = { nullptr, nullptr, nullptr, nullptr };
    synthLib::TAudioOutputs outputs = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    slot->device->process(inputs, outputs, 0, events, midiOut);
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

    synthLib::TAudioInputs inputs = { nullptr, nullptr, nullptr, nullptr };
    synthLib::TAudioOutputs outputs = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    slot->device->process(inputs, outputs, 0, events, response);
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
    (void)instrID;
    (void)paramId;
    (void)value;
}

float ft2_ostirus_get_param_for_instrument(int instrID, int paramId)
{
    (void)instrID;
    (void)paramId;
    return 0.0f;
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
    g_instrToSlot[static_cast<size_t>(idx)] = -1;
}

} /* extern "C" */
