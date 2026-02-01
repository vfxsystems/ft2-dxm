#include "PluginParam.h"
#include <algorithm>
#include <cassert>
#include <iostream>

namespace dexed {

PluginParam::PluginParam(int id, const String& name, const String& label,
                         ParamType type, float minVal, float maxVal, float def)
    : id_(id), name_(name), label_(label), type_(type),
      minValue_(minVal), maxValue_(maxVal), defaultValue_(def)
{
    /* default normalized mapping */
    if (maxValue_ > minValue_) {
        normalized_ = (defaultValue_ - minValue_) / (maxValue_ - minValue_);
    } else {
        normalized_ = 0.0f;
    }
    if (normalized_ < 0.0f) normalized_ = 0.0f;
    if (normalized_ > 1.0f) normalized_ = 1.0f;
}

/* Set normalized, clamp to [0..1] */
void PluginParam::setNormalized(float n)
{
    if (std::isnan(n)) n = 0.0f;
    if (n < 0.0f) n = 0.0f;
    if (n > 1.0f) n = 1.0f;
    normalized_ = n;
}

/* Float accessors map normalized to [minValue_, maxValue_] */
float PluginParam::getValueFloat() const
{
    return minValue_ + normalized_ * (maxValue_ - minValue_);
}

void PluginParam::setValueFloat(float v)
{
    if (v < minValue_) v = minValue_;
    if (v > maxValue_) v = maxValue_;
    if (maxValue_ > minValue_)
        normalized_ = (v - minValue_) / (maxValue_ - minValue_);
    else
        normalized_ = 0.0f;
}

/* Int accessor: map normalized to integer in [min,max] rounded */
int PluginParam::getValueInt() const
{
    int mi = (int)lroundf(minValue_);
    int ma = (int)lroundf(maxValue_);
    if (ma <= mi) return mi;
    int v = mi + (int)lroundf(normalized_ * (float)(ma - mi));
    if (v < mi) v = mi;
    if (v > ma) v = ma;
    return v;
}

void PluginParam::setValueInt(int v)
{
    int mi = (int)lroundf(minValue_);
    int ma = (int)lroundf(maxValue_);
    if (ma <= mi) {
        normalized_ = 0.0f;
    } else {
        if (v < mi) v = mi;
        if (v > ma) v = ma;
        normalized_ = (float)(v - mi) / (float)(ma - mi);
    }
}

/* Bool is simple threshold at 0.5 */
bool PluginParam::getValueBool() const
{
    return normalized_ > 0.5f;
}

void PluginParam::setValueBool(bool b)
{
    normalized_ = b ? 1.0f : 0.0f;
}

/* Choice index maps to choices_ vector; normalized -> index */
int PluginParam::getChoiceIndex() const
{
    if (choices_.empty()) return -1;
    int idx = (int)lroundf(normalized_ * (float)(choices_.size() - 1));
    if (idx < 0) idx = 0;
    if (idx >= (int)choices_.size()) idx = (int)choices_.size() - 1;
    return idx;
}

void PluginParam::setChoiceIndex(int idx)
{
    if (choices_.empty()) { normalized_ = 0.0f; return; }
    if (idx < 0) idx = 0;
    if (idx >= (int)choices_.size()) idx = (int)choices_.size() - 1;
    if (choices_.size() == 1) normalized_ = 0.0f;
    else normalized_ = (float)idx / (float)(choices_.size() - 1);
}

/* Human readable string */
String PluginParam::toString() const
{
    switch (type_) {
        case ParamType::Float: {
            char buf[64];
            float v = getValueFloat();
            snprintf(buf, sizeof(buf), "%g", (double)v);
            return String(buf);
        }
        case ParamType::Bool:
            return getValueBool() ? String("true") : String("false");
        case ParamType::Int: {
            int i = getValueInt();
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", i);
            return String(buf);
        }
        case ParamType::Choice: {
            int idx = getChoiceIndex();
            if (idx >= 0 && idx < (int)choices_.size()) return choices_[idx];
            return String();
        }
        case ParamType::Text:
            return String(); /* fallback */
        default:
            return String();
    }
}

/* Serialization helpers */
void PluginParam::writeLE16(std::vector<uint8_t>& out, uint16_t v)
{
    out.push_back((uint8_t)(v & 0xFF));
    out.push_back((uint8_t)((v >> 8) & 0xFF));
}

uint16_t PluginParam::readLE16(const uint8_t* p)
{
    return (uint16_t)p[0] | (uint16_t(p[1]) << 8);
}

/* Serialize into vector<uint8_t> using compact format described in header */
void PluginParam::serialize(std::vector<uint8_t>& out) const
{
    out.clear();
    out.push_back(static_cast<uint8_t>(type_));

    /* name */
    uint16_t nlen = (uint16_t)std::min<size_t>(name_.size(), 0xFFFF);
    writeLE16(out, nlen);
    out.insert(out.end(), name_.data(), name_.data() + nlen);

    /* normalized float (IEEE-754 LE) */
    union { float f; uint8_t b[4]; } u;
    u.f = normalized_;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    out.insert(out.end(), u.b, u.b + 4);
#else
    /* ensure LE ordering */
    out.push_back(u.b[3]);
    out.push_back(u.b[2]);
    out.push_back(u.b[1]);
    out.push_back(u.b[0]);
#endif

    /* choices if any */
    if (type_ == ParamType::Choice) {
        uint16_t ccount = (uint16_t)std::min<size_t>(choices_.size(), 0xFFFF);
        writeLE16(out, ccount);
        for (uint16_t i = 0; i < ccount; ++i) {
            const String& s = choices_[i];
            uint16_t slen = (uint16_t)std::min<size_t>(s.size(), 0xFFFF);
            writeLE16(out, slen);
            out.insert(out.end(), s.data(), s.data() + slen);
        }
    }
}

/* Deserialize from buffer. Returns true on success. */
bool PluginParam::deserialize(const uint8_t* data, size_t len)
{
    if (!data || len < 1 + 2 + 4) return false;
    const uint8_t* p = data;
    const uint8_t* end = data + len;

    type_ = static_cast<ParamType>(p[0]);
    p += 1;

    if (p + 2 > end) return false;
    uint16_t nlen = readLE16(p); p += 2;
    if (p + nlen > end) return false;
    name_.assign((const char*)p, nlen); p += nlen;

    if (p + 4 > end) return false;
    union { float f; uint8_t b[4]; } u;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    memcpy(u.b, p, 4);
#else
    /* input is LE; convert to host if necessary */
    u.b[0] = p[3];
    u.b[1] = p[2];
    u.b[2] = p[1];
    u.b[3] = p[0];
#endif
    normalized_ = u.f;
    p += 4;

    if (normalized_ < 0.0f) normalized_ = 0.0f;
    if (normalized_ > 1.0f) normalized_ = 1.0f;

    choices_.clear();
    if (type_ == ParamType::Choice) {
        if (p + 2 > end) return false;
        uint16_t ccount = readLE16(p); p += 2;
        for (uint16_t i = 0; i < ccount; ++i) {
            if (p + 2 > end) return false;
            uint16_t slen = readLE16(p); p += 2;
            if (p + slen > end) return false;
            choices_.emplace_back((const char*)p, slen);
            p += slen;
        }
    }

    return true;
}

} // namespace dexed
