/* Included by ft2_load_dxm.c to exercise its private bounded chunk parser. */
bool runDxmChunkRegressionTests(void)
{
    FILE *f = tmpfile();
    if (!f) return false;

    const uint16_t patterns = 1, channels = MAX_STEREO_PAIRS, modeMask = 0x8001, rows = 2;
    fwrite(&patterns, sizeof(patterns), 1, f);
    fwrite(&channels, sizeof(channels), 1, f);
    fwrite(&modeMask, sizeof(modeMask), 1, f);
    fwrite(&rows, sizeof(rows), 1, f);
    for (uint16_t row = 0; row < rows; ++row) {
        for (uint16_t channelIndex = 0; channelIndex < channels; ++channelIndex) {
            const macroNote_t cell = {(uint8_t)channelIndex, (uint8_t)(row + 3), (uint8_t)(channelIndex + row)};
            fwrite(&cell.slot, 1, 1, f);
            fwrite(&cell.mode, 1, 1, f);
            fwrite(&cell.value, 1, 1, f);
        }
    }
    const long length = ftell(f);
    rewind(f);
    free(macroPatternTmp[0]);
    macroPatternTmp[0] = NULL;
    bool ok = restoreMacroPatternDXM(f, (uint32_t)length);
    ok = ok && ftell(f) == length && editor.macroMode[0] && editor.macroMode[15];
    for (uint16_t row = 0; ok && row < rows; ++row) {
        for (uint16_t channelIndex = 0; channelIndex < channels; ++channelIndex) {
            const macroNote_t *cell = &macroPatternTmp[0][row * channels + channelIndex];
            if (cell->slot != channelIndex || cell->mode != row + 3 || cell->value != channelIndex + row)
                ok = false;
        }
    }
    fclose(f);
    free(macroPatternTmp[0]);
    macroPatternTmp[0] = NULL;

    f = tmpfile();
    if (!f) return false;
    fwrite(&patterns, sizeof(patterns), 1, f);
    fwrite(&channels, sizeof(channels), 1, f);
    fwrite(&modeMask, sizeof(modeMask), 1, f);
    fwrite(&rows, sizeof(rows), 1, f); /* payload deliberately missing */
    const long truncatedLength = ftell(f);
    rewind(f);
    ok = ok && !restoreMacroPatternDXM(f, (uint32_t)truncatedLength);
    fclose(f);

    /* Synth-state records must be complete, uniquely addressed, and bounded. */
    f = tmpfile();
    if (!f) return false;
    const uint8_t one = 1, instrumentIndex = MAX_INST;
    const uint32_t blobLength = 3;
    const uint8_t blob[3] = { 0x12, 0x34, 0x56 };
    fwrite(&one, sizeof(one), 1, f);
    fwrite(&instrumentIndex, sizeof(instrumentIndex), 1, f);
    fwrite(&blobLength, sizeof(blobLength), 1, f);
    fwrite(blob, sizeof(blob), 1, f);
    const long synthLength = ftell(f);
    rewind(f);
    free(instrTmp[instrumentIndex]);
    instrTmp[instrumentIndex] = NULL;
    clearPendingDXMSynthLoadState();
    ok = ok && restoreAllV2StatesDXM(f, (uint32_t)synthLength);
    ok = ok && instrTmp[instrumentIndex] != NULL && instrTmp[instrumentIndex]->useV2;
    ok = ok && g_pendingV2States[instrumentIndex].size == sizeof(blob) &&
        !memcmp(g_pendingV2States[instrumentIndex].data, blob, sizeof(blob));
    fclose(f);
    clearPendingDXMSynthLoadState();
    free(instrTmp[instrumentIndex]);
    instrTmp[instrumentIndex] = NULL;

    f = tmpfile();
    if (!f) return false;
    fwrite(&one, sizeof(one), 1, f);
    fwrite(&instrumentIndex, sizeof(instrumentIndex), 1, f);
    fwrite(&blobLength, sizeof(blobLength), 1, f); /* blob deliberately missing */
    const long truncatedSynthLength = ftell(f);
    rewind(f);
    ok = ok && !restoreAllOsTirusStatesDXM(f, (uint32_t)truncatedSynthLength);
    fclose(f);

    /* Metadata has a variable three-byte OsTIrus suffix and must consume it exactly. */
    f = tmpfile();
    if (!f) return false;
    const uint8_t metadataFlags = 16;
    const uint16_t preset = 321;
    const uint8_t slot = 7;
    fwrite(&one, sizeof(one), 1, f);
    fwrite(&instrumentIndex, sizeof(instrumentIndex), 1, f);
    fwrite(&metadataFlags, sizeof(metadataFlags), 1, f);
    fwrite(&preset, sizeof(preset), 1, f);
    fwrite(&slot, sizeof(slot), 1, f);
    const long metadataLength = ftell(f);
    rewind(f);
    ok = ok && restoreDXMInstrumentMetadata(f, (uint32_t)metadataLength);
    ok = ok && instrTmp[instrumentIndex] != NULL && instrTmp[instrumentIndex]->useOsTirus &&
        instrTmp[instrumentIndex]->osTirusPreset == preset && instrTmp[instrumentIndex]->osTirusSlot == slot;
    fclose(f);
    free(instrTmp[instrumentIndex]);
    instrTmp[instrumentIndex] = NULL;

    /* Arpeggiator records round-trip all three arrays and reject partial tails. */
    f = tmpfile();
    if (!f) return false;
    uint8_t arpValues[16];
    for (int i = 0; i < 16; ++i) arpValues[i] = (uint8_t)(i + 1);
    fwrite(&one, sizeof(one), 1, f);
    fwrite(&instrumentIndex, sizeof(instrumentIndex), 1, f);
    fwrite(arpValues, sizeof(arpValues), 1, f);
    fwrite(arpValues, sizeof(arpValues), 1, f);
    fwrite(arpValues, sizeof(arpValues), 1, f);
    const long arpLength = ftell(f);
    rewind(f);
    ok = ok && restoreOsTirusArpStateDXM(f, (uint32_t)arpLength);
    ok = ok && instrTmp[instrumentIndex] != NULL &&
        !memcmp(instrTmp[instrumentIndex]->osTirusArpStepGate, arpValues, sizeof(arpValues)) &&
        !memcmp(instrTmp[instrumentIndex]->osTirusArpStepVelocity, arpValues, sizeof(arpValues)) &&
        !memcmp(instrTmp[instrumentIndex]->osTirusArpStepLength, arpValues, sizeof(arpValues));
    fclose(f);
    free(instrTmp[instrumentIndex]);
    instrTmp[instrumentIndex] = NULL;

    f = tmpfile();
    if (!f) return false;
    fwrite(&one, sizeof(one), 1, f);
    fwrite(&instrumentIndex, sizeof(instrumentIndex), 1, f);
    fwrite(arpValues, sizeof(arpValues) - 1, 1, f);
    const long truncatedArpLength = ftell(f);
    rewind(f);
    ok = ok && !restoreOsTirusArpStateDXM(f, (uint32_t)truncatedArpLength);
    fclose(f);
    return ok;
}
