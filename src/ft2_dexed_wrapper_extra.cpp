#include <cstdint>
#include <cstddef>
#include <cstring>

static uint8_t normparm(uint8_t value, uint8_t max)
{
    if (value <= max) return value;
    return (uint8_t)((value * (int)max) / 255);
}

extern "C" {

int dx_normalize_program(const uint8_t* unpacked155, uint8_t* outPacked128)
{
    if (!unpacked155 || !outPacked128) return 0;

    uint8_t bulk[128];
    memset(bulk, 0, sizeof(bulk));

    const char defaultOpSwitch[6] = {'1','1','1','1','1','1'};

    for (int op = 0; op < 6; ++op) {
        int pp = op * 17;
        int up = op * 21;

        for (int i = 0; i < 11; ++i)
            bulk[pp + i] = unpacked155[up + i] & 0x7F;

        bulk[pp + 11] = (unpacked155[up + 11] & 0x03) | ((unpacked155[up + 12] & 0x03) << 2);
        bulk[pp + 12] = (unpacked155[up + 13] & 0x07) | ((unpacked155[up + 20] & 0x0F) << 3);
        bulk[pp + 13] = (unpacked155[up + 14] & 0x03) | ((unpacked155[up + 15] & 0x07) << 2);

        if (defaultOpSwitch[op] == '0')
            bulk[pp + 14] = 0;
        else
            bulk[pp + 14] = unpacked155[up + 16] & 0x7F;

        bulk[pp + 15] = (unpacked155[up + 17] & 0x01) | ((unpacked155[up + 18] & 0x1F) << 1);
        bulk[pp + 16] = unpacked155[up + 19] & 0x7F;
    }

    for (int i = 0; i < 9; ++i)
        bulk[102 + i] = unpacked155[126 + i] & 0x7F;

    bulk[111] = (unpacked155[135] & 0x07) | ((unpacked155[136] & 0x01) << 3);

    for (int i = 0; i < 4; ++i)
        bulk[112 + i] = unpacked155[137 + i] & 0x7F;

    bulk[116] = (unpacked155[141] & 0x01) | (((unpacked155[142] & 0x07) << 1) | ((unpacked155[143] & 0x07) << 4));
    bulk[117] = unpacked155[144] & 0x7F;

    for (int i = 0; i < 10; ++i) {
        uint8_t c = unpacked155[145 + i] & 0x7F;
        if (c < 32 || c > 127) c = ' ';
        bulk[118 + i] = c;
    }

    memcpy(outPacked128, bulk, 128);
    return 1;
}

int dx_unpack_program_from_storage(const uint8_t* packed128, uint8_t* outUnpacked155)
{
    if (!packed128 || !outUnpacked155) return 0;

    const uint8_t* bulk = packed128;
    memset(outUnpacked155, 0, 155);

    for (int op = 0; op < 6; ++op) {
        for (int i = 0; i < 11; ++i) {
            uint8_t currparm = bulk[op * 17 + i] & 0x7F;
            outUnpacked155[op * 21 + i] = normparm(currparm, 99);
        }
        uint8_t leftrightcurves = bulk[op * 17 + 11] & 0x0F;
        outUnpacked155[op * 21 + 11] = leftrightcurves & 0x03;
        outUnpacked155[op * 21 + 12] = (leftrightcurves >> 2) & 0x03;
        uint8_t detune_rs = bulk[op * 17 + 12] & 0x7F;
        outUnpacked155[op * 21 + 13] = detune_rs & 0x07;
        uint8_t kvs_ams = bulk[op * 17 + 13] & 0x1F;
        outUnpacked155[op * 21 + 14] = kvs_ams & 0x03;
        outUnpacked155[op * 21 + 15] = (kvs_ams >> 2) & 0x07;
        outUnpacked155[op * 21 + 16] = bulk[op * 17 + 14] & 0x7F;
        uint8_t fcoarse_mode = bulk[op * 17 + 15] & 0x3F;
        outUnpacked155[op * 21 + 17] = fcoarse_mode & 0x01;
        outUnpacked155[op * 21 + 18] = (fcoarse_mode >> 1) & 0x1F;
        outUnpacked155[op * 21 + 19] = bulk[op * 17 + 16] & 0x7F;
        outUnpacked155[op * 21 + 20] = (detune_rs >> 3) & 0x7F;
    }

    for (int i = 0; i < 8; ++i) {
        uint8_t currparm = bulk[102 + i] & 0x7F;
        outUnpacked155[126 + i] = normparm(currparm, 99);
    }

    outUnpacked155[134] = normparm(bulk[110] & 0x1F, 31);

    uint8_t oks_fb = bulk[111] & 0x0F;
    outUnpacked155[135] = oks_fb & 0x07;
    outUnpacked155[136] = (oks_fb >> 3) & 0x01;

    outUnpacked155[137] = bulk[112] & 0x7F;
    outUnpacked155[138] = bulk[113] & 0x7F;
    outUnpacked155[139] = bulk[114] & 0x7F;
    outUnpacked155[140] = bulk[115] & 0x7F;

    uint8_t lpms_lfw_lks = bulk[116] & 0x7F;
    outUnpacked155[141] = lpms_lfw_lks & 0x01;
    outUnpacked155[142] = (lpms_lfw_lks >> 1) & 0x07;
    outUnpacked155[143] = (lpms_lfw_lks >> 4) & 0x0F;

    outUnpacked155[144] = bulk[117] & 0x7F;

    for (int name_idx = 0; name_idx < 10; ++name_idx)
        outUnpacked155[145 + name_idx] = bulk[118 + name_idx] & 0x7F;

    return 1;
}

} /* extern "C" */
