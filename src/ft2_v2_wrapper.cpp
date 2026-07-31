#include "ft2_v2.h"

#include "ft2_replayer.h"
#include "ft2_structs.h"
#include "v2/v2defs.h"
#include "v2/synth.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <mutex>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#else
#include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
extern editor_t editor;
extern instr_t *instr[128 + 4];
#ifdef __cplusplus
}
#endif

namespace
{
constexpr int kMaxInst = MAX_INST;
constexpr size_t kPatchHeaderSize = 128u * sizeof(uint32_t);
static size_t g_currentPatchSize = 0;
static size_t g_currentBankPatchBytes = 0;

struct V2BankData
{
    std::vector<uint8_t> patchMap;
    std::array<std::array<char, 32>, 128> patchNames{};
    std::vector<uint8_t> globals;
    bool valid = false;
};

struct V2InstrumentState
{
    V2BankData bank;
    std::vector<uint8_t> synthMem;
    int currentPreset = 0;
    int sampleRate = 0;
    bool synthReady = false;
};

static void sanitize_bank(V2BankData& bank);
static bool reinit_state_synth(V2InstrumentState* st);

static bool g_defsReady = false;
static bool g_factoryReady = false;
static int g_currentSampleRate = 0;
static V2BankData g_factoryBank;
static std::vector<Ft2V2ParamInfo> g_paramInfos;
static std::vector<ParameterRange> g_paramRanges;
static std::vector<Ft2V2ParamInfo> g_globalParamInfos;
static std::vector<ParameterRange> g_globalParamRanges;
static std::vector<Ft2V2TopicInfo> g_topicInfos;
static std::vector<int> g_topicStarts;
static std::vector<Ft2V2TopicInfo> g_globalTopicInfos;
static std::vector<int> g_globalTopicStarts;
static std::vector<std::string> g_modDestNames;
static std::vector<int> g_modDestParamIds;
static std::array<V2InstrumentState*, kMaxInst + 1> g_states{};
static std::recursive_mutex g_v2Mutex;

#define V2_LOCK_GUARD() std::lock_guard<std::recursive_mutex> v2Lock(g_v2Mutex)

static std::string join_path(const std::string& a, const std::string& b)
{
    if (a.empty()) return b;
    if (b.empty()) return a;
    char sep = '/';
#ifdef _WIN32
    sep = '\\';
#endif
    if (a.back() == '/' || a.back() == '\\')
        return a + b;
    return a + sep + b;
}

static std::string get_exe_dir()
{
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return std::string();
    std::string path(buf, len);
    size_t pos = path.find_last_of("\\/");
    if (pos != std::string::npos) path.erase(pos);
    return path;
#else
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) return std::string();
    buf[len] = '\0';
    std::string path(buf);
    size_t pos = path.find_last_of('/');
    if (pos != std::string::npos) path.erase(pos);
    return path;
#endif
}

static std::string get_current_dir()
{
#ifdef _WIN32
    DWORD len = GetCurrentDirectoryA(0, nullptr);
    if (len == 0) return std::string();
    std::vector<char> buf(static_cast<size_t>(len));
    DWORD written = GetCurrentDirectoryA(len, buf.data());
    if (written == 0 || written >= len) return std::string();
    return std::string(buf.data(), static_cast<size_t>(written));
#else
    char buf[4096];
    if (!getcwd(buf, sizeof(buf))) return std::string();
    return std::string(buf);
#endif
}

static std::string parent_dir(std::string path)
{
    while (!path.empty() && (path.back() == '/' || path.back() == '\\'))
        path.pop_back();

    if (path.empty()) return std::string();

    size_t pos = path.find_last_of("\\/");
    if (pos == std::string::npos) return std::string();

#ifdef _WIN32
    if (pos <= 2 && path.size() >= 2 && path[1] == ':')
        return std::string();
#endif

    if (pos == 0)
        return path.substr(0, 1);

    return path.substr(0, pos);
}

static void add_bank_candidates_from_base(std::vector<std::string>& candidates, const std::string& base)
{
    std::string dir = base;
    for (int depth = 0; depth < 8 && !dir.empty(); ++depth) {
        candidates.push_back(join_path(dir, "presets.v2b"));
        candidates.push_back(join_path(dir, join_path("v2", "presets.v2b")));
        candidates.push_back(join_path(dir, join_path("src", join_path("v2", "presets.v2b"))));

        std::string parent = parent_dir(dir);
        if (parent.empty() || parent == dir)
            break;
        dir = parent;
    }
}

static std::vector<uint8_t> read_file_bytes(const std::string& path)
{
    std::vector<uint8_t> data;
#ifdef _WIN32
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0)
        f = nullptr;
#else
    FILE* f = fopen(path.c_str(), "rb");
#endif
    if (!f) return data;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return data;
    }

    long size = ftell(f);
    if (size <= 0) {
        fclose(f);
        return data;
    }
    rewind(f);

    data.resize(static_cast<size_t>(size));
    if (fread(data.data(), 1, data.size(), f) != data.size()) {
        data.clear();
    }
    fclose(f);
    return data;
}

static bool write_u32(std::vector<uint8_t>& out, uint32_t value)
{
    uint8_t b[4];
    b[0] = static_cast<uint8_t>(value & 0xFFu);
    b[1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
    b[2] = static_cast<uint8_t>((value >> 16) & 0xFFu);
    b[3] = static_cast<uint8_t>((value >> 24) & 0xFFu);
    out.insert(out.end(), b, b + 4);
    return true;
}

static bool read_u32(const uint8_t*& p, const uint8_t* end, uint32_t& value)
{
    if (static_cast<size_t>(end - p) < 4) return false;
    value = static_cast<uint32_t>(p[0]) |
            (static_cast<uint32_t>(p[1]) << 8) |
            (static_cast<uint32_t>(p[2]) << 16) |
            (static_cast<uint32_t>(p[3]) << 24);
    p += 4;
    return true;
}

static void clamp_char_name(char* dst, size_t dstSize, const std::string& src)
{
    if (!dst || dstSize == 0) return;
    std::memset(dst, 0, dstSize);
    if (src.empty()) return;
    const size_t count = (std::min)(dstSize - 1, src.size());
    std::memcpy(dst, src.data(), count);
    dst[count] = '\0';
}

static int normalize_from_raw(int raw, int minValue, int maxValue)
{
    if (maxValue <= minValue) return 0;
    const float f = static_cast<float>(raw - minValue) / static_cast<float>(maxValue - minValue);
    int v = static_cast<int>(std::lroundf(std::clamp(f, 0.0f, 1.0f) * 127.0f));
    return std::clamp(v, 0, 127);
}

static int raw_from_normalized(float value, int minValue, int maxValue)
{
    if (maxValue <= minValue) return minValue;
    value = std::clamp(value, 0.0f, 1.0f);
    const float raw = static_cast<float>(minValue) + value * static_cast<float>(maxValue - minValue);
    int v = static_cast<int>(std::lroundf(raw));
    if (v < minValue) v = minValue;
    if (v > maxValue) v = maxValue;
    return v;
}

static int find_version_for_patch_size(size_t patchSize)
{
    for (int i = 0; i <= V2::Version; ++i) {
        if (V2::SoundSizes[i] == static_cast<int>(patchSize))
            return i;
    }
    return -1;
}

static void build_info_tables()
{
    if (g_defsReady) return;

    V2::InitDefs();

    g_currentPatchSize = static_cast<size_t>(V2::SoundSize);
    g_currentBankPatchBytes = 128u * g_currentPatchSize;

    g_paramInfos.resize(static_cast<size_t>(V2::nParams));
    g_paramRanges.resize(static_cast<size_t>(V2::nParams));
    g_globalParamInfos.resize(static_cast<size_t>(V2::nGParams));
    g_globalParamRanges.resize(static_cast<size_t>(V2::nGParams));
    g_topicInfos.resize(static_cast<size_t>(V2::nTopics));
    g_topicStarts.resize(static_cast<size_t>(V2::nTopics));
    g_globalTopicInfos.resize(static_cast<size_t>(V2::nGTopics));
    g_globalTopicStarts.resize(static_cast<size_t>(V2::nGTopics));

    for (int i = 0; i < V2::nParams; ++i) {
        const V2::Param& p = V2::Params[i];
        g_paramInfos[i] = {
            p.version,
            p.name,
            static_cast<Ft2V2CtlType>(p.ctltype),
            p.offset,
            p.min,
            p.max,
            p.isdest,
            p.ctlstr
        };
        const float defaultRaw = static_cast<float>(V2::InitSound[i]);
        const float minRaw = static_cast<float>(p.min);
        const float maxRaw = static_cast<float>(p.max);
        float normalizedDefault = 0.0f;
        if (maxRaw > minRaw) {
            normalizedDefault = (defaultRaw - minRaw) / (maxRaw - minRaw);
        }
        g_paramRanges[i] = {0.0f, 1.0f, std::clamp(normalizedDefault, 0.0f, 1.0f)};
    }

    for (int i = 0; i < V2::nGParams; ++i) {
        const V2::Param& p = V2::GParams[i];
        g_globalParamInfos[i] = {
            p.version,
            p.name,
            static_cast<Ft2V2CtlType>(p.ctltype),
            p.offset,
            p.min,
            p.max,
            p.isdest,
            p.ctlstr
        };
        const float defaultRaw = static_cast<float>(V2::InitGlobals[i]);
        const float minRaw = static_cast<float>(p.min);
        const float maxRaw = static_cast<float>(p.max);
        float normalizedDefault = 0.0f;
        if (maxRaw > minRaw) {
            normalizedDefault = (defaultRaw - minRaw) / (maxRaw - minRaw);
        }
        g_globalParamRanges[i] = {0.0f, 1.0f, std::clamp(normalizedDefault, 0.0f, 1.0f)};
    }

    int start = 0;
    for (int i = 0; i < V2::nTopics; ++i) {
        g_topicInfos[i] = {V2::Topics[i].no, V2::Topics[i].name, V2::Topics[i].name2};
        g_topicStarts[i] = start;
        start += V2::Topics[i].no;
    }

    start = 0;
    for (int i = 0; i < V2::nGTopics; ++i) {
        g_globalTopicInfos[i] = {V2::GTopics[i].no, V2::GTopics[i].name, V2::GTopics[i].name2};
        g_globalTopicStarts[i] = start;
        start += V2::GTopics[i].no;
    }

    g_modDestNames.clear();
    g_modDestParamIds.clear();
    for (int i = 0; i < V2::nParams; ++i) {
        const V2::Param& p = V2::Params[i];
        if (!p.isdest) continue;

        std::string label;
        const char* topicName = nullptr;
        int topicStart = 0;
        for (int t = 0; t < V2::nTopics; ++t) {
            const int startIdx = g_topicStarts[t];
            const int endIdx = (t + 1 < V2::nTopics) ? g_topicStarts[t + 1] : V2::nParams;
            if (i >= startIdx && i < endIdx) {
                topicName = V2::Topics[t].name2 ? V2::Topics[t].name2 : V2::Topics[t].name;
                topicStart = startIdx;
                break;
            }
        }
        (void)topicStart;
        if (topicName && *topicName) {
            label = std::string(topicName) + " " + p.name;
        } else {
            label = p.name ? p.name : "";
        }
        g_modDestNames.push_back(label);
        g_modDestParamIds.push_back(i);
    }

    g_defsReady = true;
}

static void build_default_bank(V2BankData& bank)
{
    build_info_tables();

    bank.patchMap.assign(kPatchHeaderSize + g_currentBankPatchBytes, 0);
    auto* offsets = reinterpret_cast<uint32_t*>(bank.patchMap.data());
    uint8_t* raw = bank.patchMap.data() + kPatchHeaderSize;
    for (int i = 0; i < 128; ++i) {
        offsets[i] = static_cast<uint32_t>(kPatchHeaderSize + (i * g_currentPatchSize));
        std::memcpy(raw + (i * g_currentPatchSize), V2::InitSound, g_currentPatchSize);
        char tmp[32];
        std::snprintf(tmp, sizeof(tmp), "Init Patch #%03d", i);
        clamp_char_name(bank.patchNames[static_cast<size_t>(i)].data(), bank.patchNames[static_cast<size_t>(i)].size(), tmp);
    }
    bank.globals.assign(V2::InitGlobals, V2::InitGlobals + V2::nGParams);
    bank.valid = true;
}

static bool load_patch_versioned(const uint8_t* src, size_t srcSize, int fver, uint8_t* dst)
{
    if (!src || !dst) return false;
    if (fver < 0 || fver > V2::Version) return false;

    std::memcpy(dst, V2::InitSound, g_currentPatchSize);

    size_t pos = 0;
    for (int i = 0; i < V2::nParams; ++i) {
        if (V2::Params[i].version <= fver) {
            if (pos >= srcSize) return false;
            dst[i] = src[pos++];
        }
    }

    if (pos >= srcSize) return false;
    uint8_t modnum = src[pos++];
    dst[V2::nParams] = modnum;

    for (int i = 0; i < modnum; ++i) {
        if (pos + 3 > srcSize) return false;
        uint8_t mod[3];
        mod[0] = src[pos++];
        mod[1] = src[pos++];
        mod[2] = src[pos++];

        for (int k = 0; k <= mod[2] && k < V2::nParams; ++k) {
            if (V2::Params[k].version > fver)
                ++mod[2];
        }

        const size_t outPos = static_cast<size_t>(V2::nParams + 1 + (i * 3));
        if (outPos + 3 > g_currentPatchSize) return false;
        dst[outPos + 0] = mod[0];
        dst[outPos + 1] = mod[1];
        dst[outPos + 2] = mod[2];
    }

    // Preserve remaining bytes as default values.
    return true;
}

static bool load_bank_blob(const uint8_t* data, size_t size, V2BankData& out)
{
    build_info_tables();

    if (!data || size < 4 + (128u * 32u) + 4 + 4 + 4) return false;
    const uint8_t* p = data;
    const uint8_t* end = data + size;

    if (std::memcmp(p, "v2p0", 4) != 0)
        return false;
    p += 4;

    std::vector<std::array<char, 32>> names(128);
    for (int i = 0; i < 128; ++i) {
        if (static_cast<size_t>(end - p) < 32) return false;
        std::memcpy(names[static_cast<size_t>(i)].data(), p, 32);
        names[static_cast<size_t>(i)][31] = '\0';
        p += 32;
    }

    uint32_t patchBlobSize = 0;
    if (!read_u32(p, end, patchBlobSize)) return false;
    if (patchBlobSize == 0 || (patchBlobSize % 128u) != 0) return false;

    const size_t patchSize = static_cast<size_t>(patchBlobSize / 128u);
    const int fver = find_version_for_patch_size(patchSize);
    if (fver < 0) return false;
    if (static_cast<size_t>(end - p) < patchBlobSize) return false;

    V2BankData bank;
    bank.patchMap.assign(kPatchHeaderSize + g_currentBankPatchBytes, 0);
    auto* offsets = reinterpret_cast<uint32_t*>(bank.patchMap.data());
    uint8_t* raw = bank.patchMap.data() + kPatchHeaderSize;

    for (int i = 0; i < 128; ++i) {
        offsets[i] = static_cast<uint32_t>(kPatchHeaderSize + (i * g_currentPatchSize));
        const uint8_t* patchSrc = p + (static_cast<size_t>(i) * patchSize);
        uint8_t* patchDst = raw + (static_cast<size_t>(i) * g_currentPatchSize);
        if (patchSize == g_currentPatchSize) {
            std::memcpy(patchDst, patchSrc, g_currentPatchSize);
        } else {
            if (!load_patch_versioned(patchSrc, patchSize, fver, patchDst))
                return false;
        }
        std::memcpy(bank.patchNames[static_cast<size_t>(i)].data(), names[static_cast<size_t>(i)].data(), 32);
        bank.patchNames[static_cast<size_t>(i)][31] = '\0';
    }
    p += patchBlobSize;

    uint32_t globalsSize = 0;
    if (!read_u32(p, end, globalsSize)) return false;
    if (static_cast<size_t>(end - p) < globalsSize) return false;

    bank.globals.assign(V2::InitGlobals, V2::InitGlobals + V2::nGParams);
    if (globalsSize == static_cast<uint32_t>(V2::nGParams)) {
        std::memcpy(bank.globals.data(), p, V2::nGParams);
    } else {
        size_t pos = 0;
        for (int i = 0; i < V2::nGParams && pos < globalsSize; ++i) {
            if (V2::GParams[i].version <= fver) {
                bank.globals[static_cast<size_t>(i)] = p[pos++];
            }
        }
    }
    p += globalsSize;

    uint32_t speechSize = 0;
    if (read_u32(p, end, speechSize)) {
        if (static_cast<size_t>(end - p) < speechSize) return false;
        p += speechSize;
    }

    out = std::move(bank);
    out.valid = true;
    sanitize_bank(out);
    return true;
}

static bool save_bank_blob(const V2BankData& bank, std::vector<uint8_t>& out)
{
    build_info_tables();
    if (!bank.valid || bank.patchMap.size() < kPatchHeaderSize + g_currentBankPatchBytes) return false;

    out.clear();
    out.reserve(4 + (128u * 32u) + 4 + g_currentBankPatchBytes + 4 + V2::nGParams + 4);

    out.insert(out.end(), {'v','2','p','0'});
    for (int i = 0; i < 128; ++i) {
        const auto& name = bank.patchNames[static_cast<size_t>(i)];
        out.insert(out.end(), name.begin(), name.end());
    }

    write_u32(out, static_cast<uint32_t>(g_currentBankPatchBytes));
    const uint8_t* raw = bank.patchMap.data() + kPatchHeaderSize;
    out.insert(out.end(), raw, raw + g_currentBankPatchBytes);

    write_u32(out, static_cast<uint32_t>(V2::nGParams));
    out.insert(out.end(), bank.globals.begin(), bank.globals.begin() + V2::nGParams);

    write_u32(out, 0);
    return true;
}

static bool load_state_blob(const uint8_t* data, size_t size, V2InstrumentState& state)
{
    if (!data || size < 8) return false;
    if (std::memcmp(data, "V2ST", 4) != 0) return false;

    const uint8_t* p = data + 4;
    const uint8_t* end = data + size;
    uint32_t version = 0;
    uint32_t currentPreset = 0;
    if (!read_u32(p, end, version)) return false;
    if (version != 1) return false;
    if (!read_u32(p, end, currentPreset)) return false;

    V2BankData bank;
    if (!load_bank_blob(p, static_cast<size_t>(end - p), bank)) return false;

    state.bank = std::move(bank);
    state.currentPreset = std::clamp(static_cast<int>(currentPreset), 0, 127);
    return true;
}

static size_t save_state_blob(const V2InstrumentState& state, std::vector<uint8_t>& out)
{
    std::vector<uint8_t> bankBytes;
    if (!save_bank_blob(state.bank, bankBytes)) return 0;

    out.clear();
    out.reserve(12 + bankBytes.size());
    out.insert(out.end(), {'V','2','S','T'});
    write_u32(out, 1);
    write_u32(out, static_cast<uint32_t>(std::clamp(state.currentPreset, 0, 127)));
    out.insert(out.end(), bankBytes.begin(), bankBytes.end());
    return out.size();
}

static V2InstrumentState* get_state(int instrID, bool createIfMissing, bool requireEnabled)
{
    if (instrID < 1 || instrID > MAX_INST) return nullptr;

    if (requireEnabled) {
        instr_t* ins = instr[instrID];
        if (!ins || !ins->useV2)
            return nullptr;
    }

    V2InstrumentState*& st = g_states[instrID];
    if (!st && createIfMissing) {
        st = new V2InstrumentState();
        st->bank = g_factoryBank;
        if (!st->bank.valid) {
            build_default_bank(st->bank);
        }
        st->synthMem.resize(synthGetSize());
        st->currentPreset = 0;
        st->sampleRate = g_currentSampleRate > 0 ? g_currentSampleRate : 44100;
        st->synthReady = false;
    }

    if (!st) return nullptr;

    bool bankStorageChanged = false;
    if (st->bank.patchMap.empty() || !st->bank.valid) {
        st->bank = g_factoryBank;
        bankStorageChanged = true;
    }

    bool globalsChanged = false;
    if (st->bank.globals.empty() || static_cast<int>(st->bank.globals.size()) != V2::nGParams) {
        st->bank.globals.assign(V2::InitGlobals, V2::InitGlobals + V2::nGParams);
        globalsChanged = true;
    }

    if (st->synthMem.empty())
        st->synthMem.resize(synthGetSize());

    if (!st->synthReady || st->sampleRate != g_currentSampleRate || bankStorageChanged)
        reinit_state_synth(st);
    else if (globalsChanged)
        synthSetGlobals(st->synthMem.data(), st->bank.globals.data());

    return st;
}

static const Ft2V2ParamInfo* param_info_for_id(int paramId, bool global)
{
    build_info_tables();

    if (global) {
        if (paramId < 0 || paramId >= V2::nGParams) return nullptr;
        return &g_globalParamInfos[static_cast<size_t>(paramId)];
    }
    if (paramId < 0 || paramId >= V2::nParams) return nullptr;
    return &g_paramInfos[static_cast<size_t>(paramId)];
}

static const ParameterRange* param_range_for_id(int paramId, bool global)
{
    build_info_tables();

    if (global) {
        if (paramId < 0 || paramId >= V2::nGParams) return nullptr;
        return &g_globalParamRanges[static_cast<size_t>(paramId)];
    }
    if (paramId < 0 || paramId >= V2::nParams) return nullptr;
    return &g_paramRanges[static_cast<size_t>(paramId)];
}

static uint8_t* patch_ptr(V2InstrumentState* st, int patchIndex)
{
    if (!st || patchIndex < 0 || patchIndex >= 128) return nullptr;
    if (st->bank.patchMap.size() < kPatchHeaderSize + g_currentBankPatchBytes) return nullptr;
    return st->bank.patchMap.data() + kPatchHeaderSize + (static_cast<size_t>(patchIndex) * g_currentPatchSize);
}

static const uint8_t* patch_ptr_const(const V2InstrumentState* st, int patchIndex)
{
    if (!st || patchIndex < 0 || patchIndex >= 128) return nullptr;
    if (st->bank.patchMap.size() < kPatchHeaderSize + g_currentBankPatchBytes) return nullptr;
    return st->bank.patchMap.data() + kPatchHeaderSize + (static_cast<size_t>(patchIndex) * g_currentPatchSize);
}

static uint8_t* current_patch_ptr(V2InstrumentState* st)
{
    return patch_ptr(st, st ? std::clamp(st->currentPreset, 0, 127) : 0);
}

static const uint8_t* current_patch_ptr_const(const V2InstrumentState* st)
{
    return patch_ptr_const(st, st ? std::clamp(st->currentPreset, 0, 127) : 0);
}

static int max_mod_slots_for_patch(void)
{
    if (g_currentPatchSize <= static_cast<size_t>(V2::nParams))
        return 0;

    const size_t modBytes = g_currentPatchSize - static_cast<size_t>(V2::nParams) - 1;
    return static_cast<int>(modBytes / 3u);
}

static void sanitize_patch(uint8_t* raw)
{
    if (!raw)
        return;

    const int maxSlots = max_mod_slots_for_patch();
    raw[V2::nParams] = static_cast<uint8_t>(std::clamp(static_cast<int>(raw[V2::nParams]), 0, maxSlots));

    for (int slot = 0; slot < maxSlots; ++slot) {
        const size_t base = static_cast<size_t>(V2::nParams + 1 + (slot * 3));
        if (base + 2 >= g_currentPatchSize)
            break;

        raw[base + 0] = static_cast<uint8_t>(std::clamp(static_cast<int>(raw[base + 0]), 0, V2::nModSources - 1));
        raw[base + 1] = static_cast<uint8_t>(std::clamp(static_cast<int>(raw[base + 1]), 0, 127));
        raw[base + 2] = static_cast<uint8_t>(std::clamp(static_cast<int>(raw[base + 2]), 0, V2::nParams - 1));
    }
}

static void sanitize_bank(V2BankData& bank)
{
    if (bank.patchMap.size() < kPatchHeaderSize + g_currentBankPatchBytes)
        return;

    for (int i = 0; i < 128; ++i)
        sanitize_patch(bank.patchMap.data() + kPatchHeaderSize + (static_cast<size_t>(i) * g_currentPatchSize));
}

static void set_patch_param_raw(V2InstrumentState* st, int paramId, int rawValue)
{
    if (!st || paramId < 0 || paramId >= V2::nParams) return;
    uint8_t* raw = current_patch_ptr(st);
    if (!raw) return;
    const Ft2V2ParamInfo& info = g_paramInfos[static_cast<size_t>(paramId)];
    raw[static_cast<size_t>(paramId)] = static_cast<uint8_t>(std::clamp(rawValue, info.min, info.max));
}

static int get_patch_param_raw(const V2InstrumentState* st, int paramId)
{
    if (!st || paramId < 0 || paramId >= V2::nParams) return 0;
    const uint8_t* raw = current_patch_ptr_const(st);
    if (!raw) return 0;
    return raw[static_cast<size_t>(paramId)];
}

static void set_global_param_raw(V2InstrumentState* st, int paramId, int rawValue)
{
    if (!st || paramId < 0 || paramId >= V2::nGParams) return;
    if (static_cast<size_t>(paramId) >= st->bank.globals.size()) return;
    const Ft2V2ParamInfo& info = g_globalParamInfos[static_cast<size_t>(paramId)];
    st->bank.globals[static_cast<size_t>(paramId)] = static_cast<uint8_t>(std::clamp(rawValue, info.min, info.max));
    if (st->synthReady) {
        synthSetGlobals(st->synthMem.data(), st->bank.globals.data());
    }
}

static int get_global_param_raw(const V2InstrumentState* st, int paramId)
{
    if (!st || paramId < 0 || paramId >= V2::nGParams) return 0;
    if (static_cast<size_t>(paramId) >= st->bank.globals.size()) return 0;
    return st->bank.globals[static_cast<size_t>(paramId)];
}

static int get_dest_raw_param_id(int listIndex)
{
    if (listIndex < 0 || static_cast<size_t>(listIndex) >= g_modDestParamIds.size()) return -1;
    return g_modDestParamIds[static_cast<size_t>(listIndex)];
}

static int get_dest_list_index_from_raw(int paramId)
{
    for (size_t i = 0; i < g_modDestParamIds.size(); ++i) {
        if (g_modDestParamIds[i] == paramId)
            return static_cast<int>(i);
    }
    return -1;
}

static void send_program_change(V2InstrumentState* st, int presetIndex)
{
    if (!st) return;
    st->currentPreset = std::clamp(presetIndex, 0, 127);
    if (st->synthMem.empty()) return;

    for (uint8_t ch = 0; ch < 16; ++ch) {
        uint8_t msg[3] = {
            static_cast<uint8_t>(0xC0 | ch),
            static_cast<uint8_t>(st->currentPreset),
            0xFD
        };
        synthProcessMIDI(st->synthMem.data(), msg);

        uint8_t volumeMsg[4] = {
            static_cast<uint8_t>(0xB0 | ch),
            7,
            127,
            0xFD
        };
        synthProcessMIDI(st->synthMem.data(), volumeMsg);
    }
}

static void silence_state(V2InstrumentState* st)
{
    if (!st || !st->synthReady || st->synthMem.empty())
        return;

    for (int ch = 0; ch < 16; ++ch) {
        uint8_t msg[4] = {static_cast<uint8_t>(0xB0 | ch), 123, 0, 0xFD};
        synthProcessMIDI(st->synthMem.data(), msg);
        msg[1] = 120;
        synthProcessMIDI(st->synthMem.data(), msg);
    }
}

static bool reinit_state_synth(V2InstrumentState* st)
{
    if (!st || !st->bank.valid || st->bank.patchMap.size() < kPatchHeaderSize + g_currentBankPatchBytes)
        return false;

    if (st->synthMem.empty())
        st->synthMem.resize(synthGetSize());

    st->sampleRate = g_currentSampleRate > 0 ? g_currentSampleRate : 44100;
    synthInit(st->synthMem.data(), st->bank.patchMap.data(), st->sampleRate);
    synthSetGlobals(st->synthMem.data(), st->bank.globals.data());
    send_program_change(st, st->currentPreset);
    st->synthReady = true;
    return true;
}

static void destroy_state(V2InstrumentState*& st)
{
    if (!st) return;
    delete st;
    st = nullptr;
}

static void ensure_factory_bank_loaded(void)
{
    if (g_factoryReady) return;

    build_info_tables();

    std::string envBank;
#ifdef _WIN32
    {
        DWORD envLen = GetEnvironmentVariableA("FT2_V2_BANK_PATH", nullptr, 0);
        if (envLen > 1) {
            std::vector<char> envBuf(static_cast<size_t>(envLen));
            DWORD written = GetEnvironmentVariableA("FT2_V2_BANK_PATH", envBuf.data(), envLen);
            if (written > 0)
                envBank.assign(envBuf.data(), static_cast<size_t>(written));
        }
    }
#else
    if (const char* envPtr = std::getenv("FT2_V2_BANK_PATH")) {
        if (*envPtr)
            envBank.assign(envPtr);
    }
#endif

    std::vector<std::string> candidates;
    candidates.reserve(64);
    if (!envBank.empty())
        candidates.push_back(envBank);

    candidates.push_back("presets.v2b");
    candidates.push_back(join_path("v2", "presets.v2b"));
    candidates.push_back(join_path("src", join_path("v2", "presets.v2b")));
    add_bank_candidates_from_base(candidates, get_current_dir());
    add_bank_candidates_from_base(candidates, get_exe_dir());

    for (const auto& path : candidates) {
        if (path.empty()) continue;
        const auto bytes = read_file_bytes(path);
        if (bytes.empty()) continue;
        if (load_bank_blob(bytes.data(), bytes.size(), g_factoryBank)) {
            g_factoryReady = true;
            return;
        }
    }

    build_default_bank(g_factoryBank);
    g_factoryReady = true;
}

static void init_or_reinit_all_states(void)
{
    for (int i = 1; i <= MAX_INST; ++i) {
        V2InstrumentState* st = g_states[i];
        if (!st || st->bank.patchMap.empty())
            continue;
        if (st->synthMem.empty())
            st->synthMem.resize(synthGetSize());
        (void)reinit_state_synth(st);
    }
}
} // namespace

extern "C" {

void ft2_v2_init(int samplerate)
{
    V2_LOCK_GUARD();
    build_info_tables();
    ensure_factory_bank_loaded();

    if (samplerate <= 0) samplerate = 44100;
    g_currentSampleRate = samplerate;
    init_or_reinit_all_states();
}

void ft2_v2_shutdown(void)
{
    V2_LOCK_GUARD();
    for (int i = 1; i <= MAX_INST; ++i) {
        V2InstrumentState* st = g_states[i];
        if (!st) continue;
        st->synthMem.clear();
        st->synthMem.shrink_to_fit();
        st->synthReady = false;
        st->sampleRate = 0;
    }
    g_currentSampleRate = 0;
}

bool ft2_v2_is_initialized(void)
{
    V2_LOCK_GUARD();
    return g_factoryReady;
}

int ft2_v2_get_patch_size(void)
{
    V2_LOCK_GUARD();
    return g_currentPatchSize ? static_cast<int>(g_currentPatchSize) : V2::SoundSize;
}

int ft2_v2_get_param_count(void)
{
    V2_LOCK_GUARD();
    return V2::nParams;
}

const char* ft2_v2_get_param_name(int paramId)
{
    V2_LOCK_GUARD();
    const Ft2V2ParamInfo* info = param_info_for_id(paramId, false);
    return info ? info->name : nullptr;
}

const Ft2V2ParamInfo* ft2_v2_get_param_info(int paramId)
{
    V2_LOCK_GUARD();
    return param_info_for_id(paramId, false);
}

const ParameterRange* ft2_v2_get_param_range(int paramId)
{
    V2_LOCK_GUARD();
    return param_range_for_id(paramId, false);
}

int ft2_v2_get_global_param_count(void)
{
    V2_LOCK_GUARD();
    return V2::nGParams;
}

const char* ft2_v2_get_global_param_name(int paramId)
{
    V2_LOCK_GUARD();
    const Ft2V2ParamInfo* info = param_info_for_id(paramId, true);
    return info ? info->name : nullptr;
}

const Ft2V2ParamInfo* ft2_v2_get_global_param_info(int paramId)
{
    V2_LOCK_GUARD();
    return param_info_for_id(paramId, true);
}

const ParameterRange* ft2_v2_get_global_param_range(int paramId)
{
    V2_LOCK_GUARD();
    return param_range_for_id(paramId, true);
}

int ft2_v2_get_topic_count(void)
{
    V2_LOCK_GUARD();
    return V2::nTopics;
}

const Ft2V2TopicInfo* ft2_v2_get_topic_info(int topicIndex)
{
    V2_LOCK_GUARD();
    build_info_tables();
    if (topicIndex < 0 || topicIndex >= V2::nTopics) return nullptr;
    return &g_topicInfos[static_cast<size_t>(topicIndex)];
}

int ft2_v2_get_topic_param_start(int topicIndex)
{
    V2_LOCK_GUARD();
    build_info_tables();
    if (topicIndex < 0 || topicIndex >= V2::nTopics) return 0;
    return g_topicStarts[static_cast<size_t>(topicIndex)];
}

int ft2_v2_get_topic_param_count(int topicIndex)
{
    V2_LOCK_GUARD();
    if (topicIndex < 0 || topicIndex >= V2::nTopics) return 0;
    return V2::Topics[topicIndex].no;
}

int ft2_v2_get_global_topic_count(void)
{
    V2_LOCK_GUARD();
    return V2::nGTopics;
}

const Ft2V2TopicInfo* ft2_v2_get_global_topic_info(int topicIndex)
{
    V2_LOCK_GUARD();
    build_info_tables();
    if (topicIndex < 0 || topicIndex >= V2::nGTopics) return nullptr;
    return &g_globalTopicInfos[static_cast<size_t>(topicIndex)];
}

int ft2_v2_get_global_topic_param_start(int topicIndex)
{
    V2_LOCK_GUARD();
    build_info_tables();
    if (topicIndex < 0 || topicIndex >= V2::nGTopics) return 0;
    return g_globalTopicStarts[static_cast<size_t>(topicIndex)];
}

int ft2_v2_get_global_topic_param_count(int topicIndex)
{
    V2_LOCK_GUARD();
    if (topicIndex < 0 || topicIndex >= V2::nGTopics) return 0;
    return V2::GTopics[topicIndex].no;
}

int ft2_v2_get_mod_source_count(void)
{
    V2_LOCK_GUARD();
    return V2::nModSources;
}

const char* ft2_v2_get_mod_source_name(int index)
{
    V2_LOCK_GUARD();
    if (index < 0 || index >= V2::nModSources) return nullptr;
    return V2::ModSources[index];
}

int ft2_v2_get_mod_dest_count(void)
{
    V2_LOCK_GUARD();
    build_info_tables();
    return static_cast<int>(g_modDestNames.size());
}

const char* ft2_v2_get_mod_dest_name(int index)
{
    V2_LOCK_GUARD();
    build_info_tables();
    if (index < 0 || static_cast<size_t>(index) >= g_modDestNames.size()) return nullptr;
    return g_modDestNames[static_cast<size_t>(index)].c_str();
}

int ft2_v2_get_mod_dest_param_index(int index)
{
    V2_LOCK_GUARD();
    build_info_tables();
    return get_dest_raw_param_id(index);
}

int ft2_v2_find_mod_dest_list_index(int paramId)
{
    V2_LOCK_GUARD();
    build_info_tables();
    return get_dest_list_index_from_raw(paramId);
}

int ft2_v2_get_factory_preset_count(void)
{
    V2_LOCK_GUARD();
    return 128;
}

const char* ft2_v2_get_factory_preset_name(int index)
{
    V2_LOCK_GUARD();
    ensure_factory_bank_loaded();
    if (index < 0 || index >= 128) return nullptr;
    return g_factoryBank.patchNames[static_cast<size_t>(index)].data();
}

const char* ft2_v2_get_preset_name_for_instrument(int instrID, int index)
{
    V2_LOCK_GUARD();
    V2InstrumentState* st = get_state(instrID, false, false);
    if (st && st->bank.valid && index >= 0 && index < 128) {
        return st->bank.patchNames[static_cast<size_t>(index)].data();
    }
    return ft2_v2_get_factory_preset_name(index);
}

int ft2_v2_get_current_preset_for_instrument(int instrID)
{
    V2_LOCK_GUARD();
    V2InstrumentState* st = get_state(instrID, false, false);
    if (!st) return 0;
    return std::clamp(st->currentPreset, 0, 127);
}

const char* ft2_v2_get_current_preset_name_for_instrument(int instrID)
{
    V2_LOCK_GUARD();
    V2InstrumentState* st = get_state(instrID, false, false);
    if (!st) return ft2_v2_get_factory_preset_name(0);
    return st->bank.patchNames[static_cast<size_t>(std::clamp(st->currentPreset, 0, 127))].data();
}

int ft2_v2_load_preset_for_instrument(int instrID, int index)
{
    V2_LOCK_GUARD();
    V2InstrumentState* st = get_state(instrID, true, true);
    if (!st) return 0;
    if (index < 0 || index >= 128) return 0;
    silence_state(st);
    send_program_change(st, index);
    return 1;
}

int ft2_v2_load_factory_preset_for_instrument(int instrID, int index)
{
    V2_LOCK_GUARD();
    return ft2_v2_load_preset_for_instrument(instrID, index);
}

int ft2_v2_load_factory_preset_for_current_instrument(int index)
{
    V2_LOCK_GUARD();
    return ft2_v2_load_preset_for_instrument(editor.curInstr, index);
}

static uint8_t* current_patch_ptr(V2InstrumentState* st, int patchIndex)
{
    return patch_ptr(st, patchIndex);
}

int ft2_v2_load_patch_for_instrument(int instrID, const uint8_t* data, size_t size)
{
    V2_LOCK_GUARD();
    V2InstrumentState* st = get_state(instrID, true, false);
    if (!st || !data || size < g_currentPatchSize) return 0;
    uint8_t* dst = current_patch_ptr(st, st->currentPreset);
    if (!dst) return 0;
    silence_state(st);
    std::memcpy(dst, data, g_currentPatchSize);
    sanitize_patch(dst);
    if (st->synthReady)
        (void)reinit_state_synth(st);
    return 1;
}

int ft2_v2_get_patch_data(int instrID, uint8_t* buffer, int32_t bufferSize)
{
    V2_LOCK_GUARD();
    V2InstrumentState* st = get_state(instrID, true, true);
    if (!st || !buffer || bufferSize < static_cast<int32_t>(g_currentPatchSize)) return 0;
    uint8_t* src = current_patch_ptr(st, st->currentPreset);
    if (!src) return 0;
    std::memcpy(buffer, src, g_currentPatchSize);
    return static_cast<int>(g_currentPatchSize);
}

size_t ft2_v2_serialize_state(int instrID, uint8_t* outBuf, size_t bufSize)
{
    V2_LOCK_GUARD();
    V2InstrumentState* st = get_state(instrID, true, true);
    if (!st) return 0;
    std::vector<uint8_t> blob;
    const size_t size = save_state_blob(*st, blob);
    if (size == 0) return 0;
    if (!outBuf) return size;
    if (bufSize < size) return 0;
    std::memcpy(outBuf, blob.data(), size);
    return size;
}

int ft2_v2_deserialize_state(int instrID, const uint8_t* data, size_t size)
{
    V2_LOCK_GUARD();
    if (instrID < 1 || instrID > MAX_INST || !data || size == 0) return 0;
    V2InstrumentState tmp;
    tmp.bank = g_factoryBank.valid ? g_factoryBank : V2BankData{};
    if (!tmp.bank.valid) build_default_bank(tmp.bank);
    if (!load_state_blob(data, size, tmp))
        return 0;

    if (tmp.bank.patchMap.empty())
        return 0;
    sanitize_bank(tmp.bank);
    tmp.synthMem.resize(synthGetSize());
    if (!reinit_state_synth(&tmp))
        return 0;

    V2InstrumentState* newState = new V2InstrumentState(std::move(tmp));
    silence_state(g_states[instrID]);
    destroy_state(g_states[instrID]);
    g_states[instrID] = newState;
    return 1;
}

void ft2_v2_set_param_for_instrument(int instrID, int paramId, float value)
{
    V2_LOCK_GUARD();
    V2InstrumentState* st = get_state(instrID, true, true);
    if (!st) return;
    const Ft2V2ParamInfo* info = ft2_v2_get_param_info(paramId);
    if (!info) return;

    const int raw = raw_from_normalized(value, info->min, info->max);
    set_patch_param_raw(st, paramId, raw);
}

float ft2_v2_get_param_for_instrument(int instrID, int paramId)
{
    V2_LOCK_GUARD();
    V2InstrumentState* st = get_state(instrID, true, true);
    if (!st) return 0.0f;
    const Ft2V2ParamInfo* info = ft2_v2_get_param_info(paramId);
    if (!info) return 0.0f;
    const int raw = get_patch_param_raw(st, paramId);
    if (info->max <= info->min) return 0.0f;
    const float norm = static_cast<float>(raw - info->min) / static_cast<float>(info->max - info->min);
    return std::clamp(norm, 0.0f, 1.0f);
}

void ft2_v2_set_global_param_for_instrument(int instrID, int paramId, float value)
{
    V2_LOCK_GUARD();
    V2InstrumentState* st = get_state(instrID, true, true);
    if (!st) return;
    const Ft2V2ParamInfo* info = ft2_v2_get_global_param_info(paramId);
    if (!info) return;
    const int raw = raw_from_normalized(value, info->min, info->max);
    set_global_param_raw(st, paramId, raw);
}

float ft2_v2_get_global_param_for_instrument(int instrID, int paramId)
{
    V2_LOCK_GUARD();
    V2InstrumentState* st = get_state(instrID, true, true);
    if (!st) return 0.0f;
    const Ft2V2ParamInfo* info = ft2_v2_get_global_param_info(paramId);
    if (!info) return 0.0f;
    const int raw = get_global_param_raw(st, paramId);
    if (info->max <= info->min) return 0.0f;
    return std::clamp(static_cast<float>(raw - info->min) / static_cast<float>(info->max - info->min), 0.0f, 1.0f);
}

int ft2_v2_get_mod_count_for_instrument(int instrID)
{
    V2_LOCK_GUARD();
    V2InstrumentState* st = get_state(instrID, true, true);
    if (!st) return 0;
    const uint8_t* raw = current_patch_ptr_const(st);
    if (!raw) return 0;

    const int maxSlots = max_mod_slots_for_patch();
    return std::clamp(static_cast<int>(raw[V2::nParams]), 0, maxSlots);
}

void ft2_v2_set_mod_count_for_instrument(int instrID, int count)
{
    V2_LOCK_GUARD();
    V2InstrumentState* st = get_state(instrID, true, true);
    if (!st) return;
    uint8_t* raw = current_patch_ptr(st);
    if (!raw) return;

    const int maxSlots = max_mod_slots_for_patch();
    count = std::clamp(count, 0, maxSlots);
    const int old = raw[V2::nParams];
    raw[V2::nParams] = static_cast<uint8_t>(count);
    if (count > old) {
        for (int i = old; i < count; ++i) {
            const size_t base = static_cast<size_t>(V2::nParams + 1 + (i * 3));
            if (base + 2 >= g_currentPatchSize) break;
            raw[base + 0] = 0;
            raw[base + 1] = 0;
            raw[base + 2] = 0;
        }
    }
}

void ft2_v2_get_mod_slot_for_instrument(int instrID, int slot, int* source, int* amount, int* dest)
{
    V2_LOCK_GUARD();
    V2InstrumentState* st = get_state(instrID, true, true);
    const int maxSlots = max_mod_slots_for_patch();
    if (!st || slot < 0 || slot >= maxSlots) {
        if (source) *source = 0;
        if (amount) *amount = 0;
        if (dest) *dest = 0;
        return;
    }
    const uint8_t* raw = current_patch_ptr_const(st);
    if (!raw) {
        if (source) *source = 0;
        if (amount) *amount = 0;
        if (dest) *dest = 0;
        return;
    }
    const int modCount = std::clamp(static_cast<int>(raw[V2::nParams]), 0, maxSlots);
    if (slot >= modCount) {
        if (source) *source = 0;
        if (amount) *amount = 0;
        if (dest) *dest = 0;
        return;
    }
    const size_t base = static_cast<size_t>(V2::nParams + 1 + (slot * 3));
    if (base + 2 >= g_currentPatchSize) {
        if (source) *source = 0;
        if (amount) *amount = 0;
        if (dest) *dest = 0;
        return;
    }
    if (source) *source = raw[base + 0];
    if (amount) *amount = raw[base + 1];
    if (dest) *dest = raw[base + 2];
}

void ft2_v2_set_mod_slot_for_instrument(int instrID, int slot, int source, int amount, int dest)
{
    V2_LOCK_GUARD();
    V2InstrumentState* st = get_state(instrID, true, true);
    const int maxSlots = max_mod_slots_for_patch();
    if (!st || slot < 0 || slot >= maxSlots) return;
    uint8_t* raw = current_patch_ptr(st);
    if (!raw) return;

    const int modCount = std::clamp(static_cast<int>(raw[V2::nParams]), 0, maxSlots);
    if (slot >= modCount)
        raw[V2::nParams] = static_cast<uint8_t>(slot + 1);

    const size_t base = static_cast<size_t>(V2::nParams + 1 + (slot * 3));
    if (base + 2 >= g_currentPatchSize) return;
    raw[base + 0] = static_cast<uint8_t>(std::clamp(source, 0, V2::nModSources - 1));
    raw[base + 1] = static_cast<uint8_t>(std::clamp(amount, 0, 127));

    int destRaw = dest;
    if (dest >= 0 && dest < ft2_v2_get_mod_dest_count()) {
        destRaw = get_dest_raw_param_id(dest);
    }
    if (destRaw < 0) destRaw = 0;
    raw[base + 2] = static_cast<uint8_t>(std::clamp(destRaw, 0, 255));
}

int ft2_v2_get_active_voice_count(int instrID)
{
    V2_LOCK_GUARD();
    V2InstrumentState* st = get_state(instrID, false, true);
    if (!st || !st->synthReady) return 0;

    int poly[17] = {0};
    synthGetPoly(st->synthMem.data(), poly);
    return poly[16];
}

void ft2_v2_send_midi_to_instrument(int instrID, uint8_t status, uint8_t data1, uint8_t data2)
{
    V2_LOCK_GUARD();
    V2InstrumentState* st = get_state(instrID, true, true);
    if (!st || !st->synthReady) return;

    if ((status & 0xF0) == 0xC0) {
        return;
    }

    if ((status & 0xF0) == 0x90 && data2 != 0) {
        uint8_t volumeMsg[4] = {
            static_cast<uint8_t>(0xB0 | (status & 0x0F)),
            7,
            127,
            0xFD
        };
        synthProcessMIDI(st->synthMem.data(), volumeMsg);
    }

    uint8_t msg[4];
    msg[0] = status;
    msg[1] = data1;
    if ((status & 0xF0) == 0xC0 || (status & 0xF0) == 0xD0) {
        msg[2] = 0xFD;
    } else {
        msg[2] = data2;
        msg[3] = 0xFD;
    }
    synthProcessMIDI(st->synthMem.data(), msg);
}

void ft2_v2_panic(void)
{
    V2_LOCK_GUARD();
    for (int i = 1; i <= MAX_INST; ++i) {
        V2InstrumentState* st = get_state(i, false, true);
        if (!st || !st->synthReady) continue;
        for (int ch = 0; ch < 16; ++ch) {
            uint8_t msg[4] = {static_cast<uint8_t>(0xB0 | ch), 123, 0, 0xFD};
            synthProcessMIDI(st->synthMem.data(), msg);
            msg[1] = 120;
            synthProcessMIDI(st->synthMem.data(), msg);
        }
    }
}

void ft2_v2_store_instrument_state(int instrID)
{
    V2_LOCK_GUARD();
    (void)get_state(instrID, true, true);
}

void ft2_v2_restore_instrument_state(int instrID)
{
    V2_LOCK_GUARD();
    V2InstrumentState* st = get_state(instrID, true, true);
    if (!st) return;
    if (!st->synthReady) {
        (void)reinit_state_synth(st);
    }
}

bool ft2_v2_has_persistent_state(int instrID)
{
    V2_LOCK_GUARD();
    return get_state(instrID, false, false) != nullptr;
}

void ft2_v2_clear_persistent_state(int instrID)
{
    V2_LOCK_GUARD();
    if (instrID < 1 || instrID > MAX_INST) return;
    silence_state(g_states[instrID]);
    destroy_state(g_states[instrID]);
}

void ft2_v2_render(float* bufL, float* bufR, int nsamples, int add)
{
    V2_LOCK_GUARD();
    if (!bufL || !bufR || nsamples <= 0) return;
    if (!g_factoryReady) ft2_v2_init(g_currentSampleRate > 0 ? g_currentSampleRate : 44100);

    if (!add) {
        std::memset(bufL, 0, static_cast<size_t>(nsamples) * sizeof(float));
        std::memset(bufR, 0, static_cast<size_t>(nsamples) * sizeof(float));
    }

    bool first = true;
    for (int i = 1; i <= MAX_INST; ++i) {
        instr_t* ins = instr[i];
        if (!ins || !ins->useV2) continue;
        V2InstrumentState* st = get_state(i, true, true);
        if (!st || !st->synthReady) continue;

        if (first && !add) {
            synthRender(st->synthMem.data(), bufL, nsamples, bufR, 1);
            first = false;
        } else {
            synthRender(st->synthMem.data(), bufL, nsamples, bufR, 1);
        }
    }
}

void ft2_v2_render_for_channel(int instrID, float* bufL, float* bufR, int nsamples, int add)
{
    V2_LOCK_GUARD();
    if (!bufL || !bufR || nsamples <= 0) return;
    if (!g_factoryReady) ft2_v2_init(g_currentSampleRate > 0 ? g_currentSampleRate : 44100);

    V2InstrumentState* st = get_state(instrID, true, true);
    if (!st || !st->synthReady) {
        if (!add) {
            std::memset(bufL, 0, static_cast<size_t>(nsamples) * sizeof(float));
            std::memset(bufR, 0, static_cast<size_t>(nsamples) * sizeof(float));
        }
        return;
    }

    if (!add) {
        std::memset(bufL, 0, static_cast<size_t>(nsamples) * sizeof(float));
        std::memset(bufR, 0, static_cast<size_t>(nsamples) * sizeof(float));
    }

    synthRender(st->synthMem.data(), bufL, nsamples, bufR, 1);
}

} // extern "C"
