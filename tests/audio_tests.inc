/* Included in ft2_audio.c only in test builds to exercise the actual callback and export path. */
bool runSynthMuteRegressionTest(void)
{
    float output[512];
    if (!setupAudioBuffers()) { freeAudioBuffers(); return false; }
    mixerInit();
    setAudioAmp(32, 256, true);
    memset(voice, 0, sizeof(voice));
    memset(channel, 0, sizeof(channel));
    playTone(0, 1, 60, 64, 0, 0);
    mixReplayerTickToBuffer(256, output, 32);
    float peak = 0.0f;
    for (int i = 0; i < 512; i++) peak = MAX(peak, fabsf(output[i]));
    bool ok = peak > 0.0f;
    if (scope[0].volume != SCOPE_HEIGHT * 4)
    {
        fprintf(stderr, "synth scope regression: volume=%u expected=%u\n",
            scope[0].volume, SCOPE_HEIGHT * 4);
        ok = false;
    }
    /* Routing must advance the engine, while a muted route contributes silence. */
    channel[0].channelOff = true;
    mixReplayerTickToBuffer(256, output, 32);
    for (int i = 0; i < 512; i++) if (output[i] != 0.0f) ok = false;
    channel[0].channelOff = false;
    playTone(0, 1, NOTE_OFF, 0, 0, 0);
    freeAudioBuffers();
    return ok;
}

bool runAudioRegressionTests(void)
{
    float live[64], offline[64];
    const int8_t sample[1] = {32};
    int failures = 0;
    if (!setupAudioBuffers()) { freeAudioBuffers(); return false; }
    mixerInit();
    memset(channel, 0, sizeof(channel));
    memset(voice, 0, sizeof(voice));
    song.numChannels = 2;
    audio.freq = 44100;
    editor.dPerfFreq = (double)SDL_GetPerformanceFrequency();
    config.specialFlags = BITDEPTH_32;
    smpShiftValue = 3;
    voice[0].active = true;
    voice[0].base8 = voice[0].base8R = sample;
    voice[0].sampleEnd = 1;
    voice[0].fCurrVolumeL = voice[0].fCurrVolumeR = 1.0f;
    /* delta=0 deliberately provides an exact DC reference, independent of resampler phase. */
    for (int test = 0; test < 5; test++)
    {
        mixerMasterGain = test == 0 ? 0.0f : 0.5f;
        setAudioAmp(32, 256, true);
        if (test == 2)
        {
            if (!dspInitEffect(&masterEffects[0], DSP_TYPE_GAINER, audio.freq)) { failures++; break; }
            masterEffects[0].enabled = true;
            masterEffects[0].params.gainer.gain = 0.25f;
        }
        if (test == 3) setAudioAmp(32, 0, true);
        if (test == 4) setAudioAmp(16, 256, true);
        audio.tickSampleCounter = 32; /* exercise callback without advancing the replayer */
        audioCallback(NULL, (Uint8 *)live, sizeof(live));
        mixReplayerTickToBuffer(32, offline, 32);
        float expected = tanhf(tanhf(0.25f) * mixerMasterGain *
            (test >= 2 ? 0.25f : 1.0f) * (test == 3 ? 0.0f : test == 4 ? 0.5f : 1.0f));
        for (int i = 0; i < 64; i++)
        {
            if (!isfinite(live[i]) || !isfinite(offline[i]) ||
                fabsf(live[i] - offline[i]) > 1e-6f || fabsf(live[i] - expected) > 1e-6f)
            {
                fprintf(stderr, "audio regression %d: live=%g export=%g expected=%g\n", test, live[i], offline[i], expected);
                failures++;
                break;
            }
        }
    }
    for (int i = 0; i < 8193; i++) audio.fMixBufferL[i] = audio.fMixBufferR[i] = 1.0f;
    dspProcessChain(masterEffects, audio.fMixBufferL, audio.fMixBufferR, 8193);
    if (audio.fMixBufferL[0] != 0.25f || audio.fMixBufferL[8192] != 0.25f)
    {
        fprintf(stderr, "audio regression: large offline blocks bypassed DSP\n");
        failures++;
    }
    dspFreeEffect(&masterEffects[0]);
    mixerMasterGain = 1.0f;
    setAudioAmp(32, 256, true);
    /* Preserve the existing nonlinear transfer curve at unity. */
    audio.tickSampleCounter = 32;
    audioCallback(NULL, (Uint8 *)live, sizeof(live));
    if (fabsf(live[0] - tanhf(tanhf(0.25f))) > 1e-6f) failures++;
    int16_t pcm[64];
    mixReplayerTickToBuffer(32, pcm, 16);
    if (abs(pcm[0] - (int)lrintf(live[0] * 32767.0f)) > 1) failures++;
    memset(voice, 0, sizeof(voice));
    freeAudioBuffers();
    if (!failures) puts("ft2-dxm audio regression tests passed");
    return failures == 0;
}
