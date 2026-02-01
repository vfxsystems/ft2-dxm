#ifndef PLUGINDATA_H_INCLUDED
#define PLUGINDATA_H_INCLUDED

/*
  JUCE-free replacement for Dexed's PluginData.h

  Purpose:
  - Provide minimal sysex utility functions for the embedded Dexed engine
  - Avoid JUCE dependencies and redefinition issues
  - Support basic DX7 program pack/unpack operations
*/

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <array>

namespace DexedInternal {
    // DX7 sysex constants
    static constexpr int SYSEX_PAYLOAD = 4096;
    static constexpr int SYSEX_SIZE = SYSEX_PAYLOAD + 8; // header + payload + checksum + F7
}

/* Compute DX7-style sysex checksum: returns 7-bit checksum for `size` bytes. */
inline uint8_t sysexChecksum(const uint8_t *sysex, int size) {
    int sum = 0;
    for (int i = 0; i < size; ++i) sum -= sysex[i];
    return static_cast<uint8_t>(sum & 0x7F);
}

/* Export a single unpacked 155-byte program into a small sysex dump */
inline void exportSysexPgm(uint8_t *dest, const uint8_t *src155) {
    const uint8_t hdr[6] = { 0xF0, 0x43, 0x00, 0x00, 0x01, 0x1B };
    std::memcpy(dest, hdr, 6);
    std::memcpy(dest + 6, src155, 155);
    uint8_t chk = sysexChecksum(src155, 155);
    dest[6 + 155] = chk;
    dest[6 + 155 + 1] = 0xF7;
}

/* Simple DX7 Cartridge class for basic pack/unpack operations */
class DexedCartridge {
public:
    uint8_t voiceData[DexedInternal::SYSEX_SIZE]; // Full sysex with header
    uint8_t perfData[64]; // Performance data (unused in basic implementation)

    DexedCartridge() {
        std::memset(voiceData, 0, sizeof(voiceData));
        std::memset(perfData, 0, sizeof(perfData));
    }

    /* Load from buffer - simplified version for embedded use */
    int loadFromBuffer(const uint8_t *buffer, int size) {
        if (!buffer || size <= 0) return -1;
        
        if (buffer[0] == 0xF0 && size >= DexedInternal::SYSEX_SIZE) {
            // Looks like sysex - copy entire block
            std::memcpy(voiceData, buffer, DexedInternal::SYSEX_SIZE);
            return 0;
        } else if (size >= 4096) {
            // Raw cartridge data - add minimal header
            const uint8_t hdr[6] = { 0xF0, 0x43, 0x00, 0x09, 0x20, 0x00 };
            std::memcpy(voiceData, hdr, 6);
            std::memcpy(voiceData + 6, buffer, 4096);
            uint8_t chk = sysexChecksum(voiceData + 6, 4096);
            voiceData[4102] = chk;
            voiceData[4103] = 0xF7;
            return 0;
        }
        return -1;
    }

    /* Unpack a single program from the cartridge */
    void unpackProgram(uint8_t *dest155, int idx) const {
        if (!dest155 || idx < 0 || idx >= 32) return;
        
        const uint8_t *bulk = voiceData + 6 + (idx * 128);
        std::memset(dest155, 0, 155);

        // Unpack operators (simplified version)
        for (int op = 0; op < 6; ++op) {
            int pp = op * 17; // packed offset
            int up = op * 21; // unpacked offset
            
            // Copy operator parameters
            for (int i = 0; i < 11; ++i) {
                dest155[up + i] = bulk[pp + i];
            }
            for (int i = 11; i < 17; ++i) {
                dest155[up + i] = bulk[pp + i];
            }
            // Copy remaining operator data
            for (int i = 17; i < 21; ++i) {
                dest155[up + i] = (i < 17 + 4) ? bulk[pp + 11 + (i - 17)] : 0;
            }
        }

        // Copy global parameters (algorithm, feedback, etc.)
        for (int i = 0; i < 29; ++i) {
            dest155[126 + i] = bulk[102 + i];
        }
    }

    /* Pack a program into the cartridge */
    void packProgram(const uint8_t *src155, int idx, const std::string &name = "") {
        if (!src155 || idx < 0 || idx >= 32) return;
        
        uint8_t *bulk = voiceData + 6 + (idx * 128);
        
        // Pack operators (simplified version)
        for (int op = 0; op < 6; ++op) {
            int pp = op * 17; // packed offset  
            int up = op * 21; // unpacked offset
            
            // Pack operator parameters
            for (int i = 0; i < 17; ++i) {
                bulk[pp + i] = (i < 21) ? (src155[up + i] & 0x7F) : 0;
            }
        }

        // Pack global parameters
        for (int i = 0; i < 29; ++i) {
            bulk[102 + i] = (126 + i < 155) ? (src155[126 + i] & 0x7F) : 0;
        }

        // Set program name (padded/truncated to 10 chars)
        std::string pname = name.empty() ? "INIT VOICE" : name;
        for (int i = 0; i < 10; ++i) {
            char c = (i < (int)pname.length()) ? pname[i] : ' ';
            bulk[118 + i] = static_cast<uint8_t>(c) & 0x7F;
        }
    }

    /* Get program names from cartridge */
    void getProgramNames(std::vector<std::string> &out) const {
        out.clear();
        out.reserve(32);
        
        for (int idx = 0; idx < 32; ++idx) {
            const uint8_t *bulk = voiceData + 6 + idx * 128;
            std::string name;
            name.reserve(10);
            
            for (int i = 0; i < 10; ++i) {
                uint8_t c = bulk[118 + i] & 0x7F;
                if (c < 32 || c > 126) c = ' ';
                name.push_back(static_cast<char>(c));
            }
            
            // Trim trailing spaces
            while (!name.empty() && name.back() == ' ') {
                name.pop_back();
            }
            
            out.push_back(name.empty() ? "INIT VOICE" : name);
        }
    }

    /* Get raw voice data pointer */
    uint8_t* getRawVoice() { return voiceData + 6; }
    const uint8_t* getRawVoice() const { return voiceData + 6; }
};

#endif  // PLUGINDATA_H_INCLUDED