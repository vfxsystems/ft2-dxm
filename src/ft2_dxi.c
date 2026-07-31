/* ft2_dxi.c - standalone "DXI" (DXM Instrument) synth patch file save/load.
**
** This is a pure container/dispatch layer: all per-engine (de)serialization is
** delegated to the same primitives the DXM module saver/loader already use
** (ft2_dx_get_patch_data/ft2_dx_load_patch_for_instrument, ft2_v2_serialize_state/
** ft2_v2_deserialize_state, ft2_ostirus_serialize_state/ft2_ostirus_deserialize_state,
** and direct instr_t::tf4Params access for TF4), so a .dxi payload is byte-identical
** to the matching DXM chunk's per-instrument payload.
*/

#include "ft2_dxi.h"
#include "ft2_replayer.h"
#include "ft2_dexed.h"
#include "ft2_v2.h"
#include "ft2_ostirus.h"
#include "ft2_synth.h"
#include "ft2_sysreqs.h"
#include "ft2_macromap.h" // ui_sync_from_instrument
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Declared extern rather than via a header, matching the existing convention used
** by ft2_module_saver.c / modloaders/ft2_load_dxm.c for this same function. */
extern void *createInstrumentInstance(int instrID);
extern void ft2_synth_set_all_persistent_params(int instrID, const float *params, int paramCount);

#define DXI_VERSION 1
#define DXI_NAME_LEN 32
#define DXI_TF4_PARAM_COUNT 128
#define DXI_DEXED_PATCH_SIZE 155
#define DXI_MAX_PAYLOAD_SIZE (16u * 1024u * 1024u) // sanity cap against corrupt/malicious files

typedef enum
{
    DXI_ENGINE_TF4 = 0,
    DXI_ENGINE_DEXED = 1,
    DXI_ENGINE_V2 = 2,
    DXI_ENGINE_OSTIRUS = 3
} dxiEngineTag_t;

static bool writeHeader(FILE *f, dxiEngineTag_t engineTag, const char *name, uint32_t payloadLen)
{
    char nameBuf[DXI_NAME_LEN];
    const uint16_t version = DXI_VERSION;
    const uint8_t tag = (uint8_t)engineTag;
    const uint8_t reserved = 0;

    memset(nameBuf, 0, sizeof (nameBuf));
    if (name != NULL)
        strncpy(nameBuf, name, sizeof (nameBuf) - 1);

    if (fwrite("DXI0", 1, 4, f) != 4) return false;
    if (fwrite(&version, sizeof (version), 1, f) != 1) return false;
    if (fwrite(&tag, sizeof (tag), 1, f) != 1) return false;
    if (fwrite(&reserved, sizeof (reserved), 1, f) != 1) return false;
    if (fwrite(nameBuf, 1, sizeof (nameBuf), f) != sizeof (nameBuf)) return false;
    if (fwrite(&payloadLen, sizeof (payloadLen), 1, f) != 1) return false;

    return true;
}

static bool readHeader(FILE *f, dxiEngineTag_t *outEngineTag, char *outName, uint32_t *outPayloadLen)
{
    char magic[4];
    uint16_t version = 0;
    uint8_t engineTag = 0, reserved = 0;
    uint32_t payloadLen = 0;

    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "DXI0", 4) != 0)
        return false;
    if (fread(&version, sizeof (version), 1, f) != 1 || version != DXI_VERSION)
        return false;
    if (fread(&engineTag, sizeof (engineTag), 1, f) != 1)
        return false;
    if (fread(&reserved, sizeof (reserved), 1, f) != 1)
        return false;
    (void)reserved;
    if (outName != NULL)
    {
        if (fread(outName, 1, DXI_NAME_LEN, f) != DXI_NAME_LEN)
            return false;
        outName[DXI_NAME_LEN - 1] = '\0';
    }
    else if (fseek(f, DXI_NAME_LEN, SEEK_CUR) != 0)
    {
        return false;
    }
    if (fread(&payloadLen, sizeof (payloadLen), 1, f) != 1)
        return false;

    if (engineTag > (uint8_t)DXI_ENGINE_OSTIRUS)
        return false;
    if (payloadLen == 0 || payloadLen > DXI_MAX_PAYLOAD_SIZE)
        return false;

    *outEngineTag = (dxiEngineTag_t)engineTag;
    *outPayloadLen = payloadLen;
    return true;
}

bool ft2_dxi_save_instrument(UNICHAR *filenameU, int instrID)
{
    if (filenameU == NULL || instrID < 1 || instrID > MAX_INST)
        return false;

    instr_t *ins = instr[instrID];
    if (ins == NULL)
        return false;

    dxiEngineTag_t engineTag;
    const uint8_t *payload = NULL;
    uint32_t payloadLen = 0;
    uint8_t *heapPayload = NULL;

    if (ins->useOsTirus)
    {
        engineTag = DXI_ENGINE_OSTIRUS;
        const size_t needed = ft2_ostirus_serialize_state(instrID, NULL, 0);
        if (needed == 0)
        {
            okBox(0, "System message", "Failed to read OsTIrus patch data for this instrument.", NULL);
            return false;
        }
        heapPayload = (uint8_t *)malloc(needed);
        if (heapPayload == NULL || ft2_ostirus_serialize_state(instrID, heapPayload, needed) != needed)
        {
            free(heapPayload);
            okBox(0, "System message", "Failed to serialize OsTIrus patch data.", NULL);
            return false;
        }
        payload = heapPayload;
        payloadLen = (uint32_t)needed;
    }
    else if (ins->useV2)
    {
        engineTag = DXI_ENGINE_V2;
        const size_t needed = ft2_v2_serialize_state(instrID, NULL, 0);
        if (needed == 0)
        {
            okBox(0, "System message", "Failed to read V2 patch data for this instrument.", NULL);
            return false;
        }
        heapPayload = (uint8_t *)malloc(needed);
        if (heapPayload == NULL || ft2_v2_serialize_state(instrID, heapPayload, needed) != needed)
        {
            free(heapPayload);
            okBox(0, "System message", "Failed to serialize V2 patch data.", NULL);
            return false;
        }
        payload = heapPayload;
        payloadLen = (uint32_t)needed;
    }
    else if (ins->useDexed)
    {
        engineTag = DXI_ENGINE_DEXED;
        heapPayload = (uint8_t *)malloc(DXI_DEXED_PATCH_SIZE);
        if (heapPayload == NULL || ft2_dx_get_patch_data(instrID, heapPayload, DXI_DEXED_PATCH_SIZE) <= 0)
        {
            free(heapPayload);
            okBox(0, "System message", "Failed to read Dexed patch data for this instrument.", NULL);
            return false;
        }
        payload = heapPayload;
        payloadLen = DXI_DEXED_PATCH_SIZE;
    }
    else if (ins->useTF4)
    {
        engineTag = DXI_ENGINE_TF4;
        // Refresh from the live engine, mirroring refreshAllTF4ParamsFromSynth() in
        // ft2_module_saver.c, so the saved patch reflects any just-tweaked parameters.
        for (int p = 0; p < DXI_TF4_PARAM_COUNT; p++)
            ins->tf4Params[p] = ft2_synth_get_param(instrID, p);
        payload = (const uint8_t *)ins->tf4Params;
        payloadLen = (uint32_t)(DXI_TF4_PARAM_COUNT * sizeof (float));
    }
    else
    {
        okBox(0, "System message", "This instrument isn't using a synth engine - nothing to save as a DXI patch.", NULL);
        return false;
    }

    FILE *f = UNICHAR_FOPEN(filenameU, "wb");
    if (f == NULL)
    {
        free(heapPayload);
        okBox(0, "System message", "General I/O error during saving! Is the file in use?", NULL);
        return false;
    }

    bool ok = writeHeader(f, engineTag, ins->smp[0].name, payloadLen);
    if (ok && fwrite(payload, 1, payloadLen, f) != payloadLen)
        ok = false;

    fclose(f);
    free(heapPayload);

    if (!ok)
        okBox(0, "System message", "General I/O error during saving! Disk full?", NULL);

    return ok;
}

bool ft2_dxi_load_instrument(UNICHAR *filenameU, int instrID)
{
    if (filenameU == NULL || instrID < 1 || instrID > MAX_INST)
        return false;

    instr_t *ins = instr[instrID];
    if (ins == NULL)
        return false;

    FILE *f = UNICHAR_FOPEN(filenameU, "rb");
    if (f == NULL)
    {
        okBox(0, "System message", "General I/O error during loading! Is the file in use?", NULL);
        return false;
    }

    dxiEngineTag_t engineTag;
    char name[DXI_NAME_LEN];
    uint32_t payloadLen = 0;
    if (!readHeader(f, &engineTag, name, &payloadLen))
    {
        fclose(f);
        okBox(0, "System message", "Not a valid/supported DXI patch file.", NULL);
        return false;
    }

    uint8_t *payload = (uint8_t *)malloc(payloadLen);
    if (payload == NULL || fread(payload, 1, payloadLen, f) != payloadLen)
    {
        free(payload);
        fclose(f);
        okBox(0, "System message", "General I/O error during loading! File truncated?", NULL);
        return false;
    }
    fclose(f);

    // Select the matching engine, clearing the others (mirrors normalizeSynthFlags()
    // in modloaders/ft2_load_dxm.c).
    ins->useTF4 = (engineTag == DXI_ENGINE_TF4);
    ins->useDexed = (engineTag == DXI_ENGINE_DEXED);
    ins->useV2 = (engineTag == DXI_ENGINE_V2);
    ins->useOsTirus = (engineTag == DXI_ENGINE_OSTIRUS);
    ins->isDXMInstrument = true;
    strncpy(ins->smp[0].name, name, sizeof (ins->smp[0].name) - 1);
    ins->smp[0].name[sizeof (ins->smp[0].name) - 1] = '\0';

    bool ok = true;
    switch (engineTag)
    {
        case DXI_ENGINE_TF4:
            if (payloadLen != DXI_TF4_PARAM_COUNT * sizeof (float))
            {
                ok = false;
            }
            else
            {
                memcpy(ins->tf4Params, payload, payloadLen);
                ft2_synth_set_all_persistent_params(instrID, ins->tf4Params, DXI_TF4_PARAM_COUNT);
                createInstrumentInstance(instrID);
            }
            break;

        case DXI_ENGINE_DEXED:
            if (payloadLen != DXI_DEXED_PATCH_SIZE)
            {
                ok = false;
            }
            else
            {
                memcpy(ins->dxParams, payload, payloadLen);
                ok = ft2_dx_load_patch_for_instrument(instrID, ins->dxParams, DXI_DEXED_PATCH_SIZE) != 0;
            }
            break;

        case DXI_ENGINE_V2:
            ok = ft2_v2_deserialize_state(instrID, payload, payloadLen) != 0;
            break;

        case DXI_ENGINE_OSTIRUS:
            ok = ft2_ostirus_deserialize_state(instrID, payload, payloadLen) != 0;
            break;
    }

    free(payload);

    if (!ok)
    {
        okBox(0, "System message", "DXI patch data was corrupt or incompatible with this build.", NULL);
        return false;
    }

    ui_sync_from_instrument();
    return true;
}
