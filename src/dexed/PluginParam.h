#ifndef DEXED_PLUGIN_PARAM_H
#define DEXED_PLUGIN_PARAM_H

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cmath>

namespace dexed {

/* Lightweight String alias to replace JUCE's String where used in plugin code */
using String = std::string;

/* Minimal GUI type stubs to avoid pulling JUCE into builds that only need
 * compile-time references. These are intentionally tiny and have no rendering
 * behavior. They exist so source files that referenced JUCE widget types will
 * still compile when building the embedded host UI.
 */
struct Slider {
    float value = 0.0f;
    void setValue(float v) { value = v; }
    float getValue() const { return value; }
};

struct Button {
    bool toggled = false;
    void setToggleState(bool s) { toggled = s; }
    bool getToggleState() const { return toggled; }
};

struct ComboBox {
    std::vector<String> items;
    int selected = -1;
    void addItem(const String& s) { items.push_back(s); }
    void setSelected(int idx) { selected = idx; }
    int getSelected() const { return selected; }
};

/* Parameter type */
enum class ParamType : uint8_t {
    Float = 0,
    Bool  = 1,
    Int   = 2,
    Choice= 3,
    Text  = 4
};

/* PluginParam - minimal JUCE-free parameter representation
 *
 * Provides:
 *  - normalized value accessor (0..1)
 *  - typed accessors for float/int/bool/choice
 *  - simple compact serialization to/from bytes
 *  - metadata: id, name, label, choices (for Choice type)
 *
 * The serialization format used by `serialize()` is intentionally compact and
 * stable for this embedded host's needs:
 *
 *   [1 byte] ParamType
 *   [2 bytes] name length (LE)
 *   [N bytes] name UTF-8
 *   [4 bytes] normalized value (float, IEEE-754 LE)
 *   [2 bytes] choices count (LE) if type == Choice
 *   [... choices ...] each choice prefixed by 2-byte length + bytes
 *
 * This is sufficient for storing/restoring per-parameter state in instrument
 * blobs or small host side state caches. If you need a different format, tell me.
 */
class PluginParam {
public:
    PluginParam() = default;
    PluginParam(int id, const String& name, const String& label,
                ParamType type = ParamType::Float,
                float minVal = 0.0f, float maxVal = 1.0f, float def = 0.0f);

    /* Metadata */
    int getId() const { return id_; }
    const String& getName() const { return name_; }
    const String& getLabel() const { return label_; }
    ParamType getType() const { return type_; }

    /* Normalized access (0..1) */
    float getNormalized() const { return normalized_; }
    void setNormalized(float n);

    /* Typed accessors */
    float getValueFloat() const;        /* maps normalized to [min,max] */
    void  setValueFloat(float v);

    int   getValueInt() const;         /* for Int or Choice: maps normalized to int range */
    void  setValueInt(int v);

    bool  getValueBool() const;
    void  setValueBool(bool b);

    int   getChoiceIndex() const;
    void  setChoiceIndex(int idx);
    void  setChoices(const std::vector<String>& choices) { choices_ = choices; }

    /* Human-readable representation */
    String toString() const;

    /* Serialization */
    void serialize(std::vector<uint8_t>& out) const;
    bool deserialize(const uint8_t* data, size_t len);

private:
    int id_ = -1;
    String name_;
    String label_;
    ParamType type_ = ParamType::Float;

    /* numeric range for float/int parameters */
    float minValue_ = 0.0f;
    float maxValue_ = 1.0f;
    float defaultValue_ = 0.0f;

    /* core stored state: normalized 0..1 */
    float normalized_ = 0.0f;

    /* Choice items for enum-like parameters */
    std::vector<String> choices_;

    /* helpers */
    static void writeLE16(std::vector<uint8_t>& out, uint16_t v);
    static uint16_t readLE16(const uint8_t* p);
};

} // namespace dexed

#endif // DEXED_PLUGIN_PARAM_H