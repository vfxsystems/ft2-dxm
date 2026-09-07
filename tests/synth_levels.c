#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ft2_header.h"
#include "ft2_replayer.h"
#include "ft2_unified_synth.h"
#include "ft2_ostirus.h"

/* Measurement, not preset normalization: retain patch dynamics and report overloads. */
bool runSynthLevelTests(void)
{
    const int rates[] = {44100, 48000, 96000};
    const int blocks[] = {64, 256, 1024};
    float left[1024], right[1024];
    bool ok = allocateInstr(1);
    if (!ok) return false;
    puts("LEVEL,engine,rate,block,preset,peak,rms,dc,over_unity_samples");
    for (int rate = 0; rate < 3; rate++)
    {
        ft2_unified_synth_init(rates[rate]);
        for (int type = SYNTH_TYPE_TUNEFISH4; type <= SYNTH_TYPE_OSTIRUS; type++)
        {
            const UnifiedSynthInterface *engine = ft2_unified_synth_get_engine((SynthEngineType)type);
            if (!engine || !engine->render_for_channel || !engine->load_preset) continue;
            if (type == SYNTH_TYPE_OSTIRUS && !ft2_ostirus_is_initialized())
            {
                puts("SKIP,OsTIrus levels: no external ROM loaded");
                continue;
            }
            instr[1]->useTF4 = type == SYNTH_TYPE_TUNEFISH4;
            instr[1]->useDexed = type == SYNTH_TYPE_DEXED;
            instr[1]->useV2 = type == SYNTH_TYPE_V2;
            instr[1]->useOsTirus = type == SYNTH_TYPE_OSTIRUS;
            const int count = engine->get_preset_count ? engine->get_preset_count() : 0;
            if (count == 0) { printf("SKIP,%s levels: no factory presets\n", engine->engineName); continue; }
            for (int preset = 0; preset < count && preset < 3; preset++)
            {
                engine->panic();
                if (!engine->load_preset(1, preset)) { ok = false; continue; }
                const int notes[] = {48, 55, 60, 64};
                for (int i = 0; i < 4; i++)
                {
                    MidiMessage on = {0x90, notes[i], 127};
                    engine->send_midi(1, &on);
                }
                double sum = 0.0, squares = 0.0;
                float peak = 0.0f;
                int samples = 0, overs = 0;
                for (int frame = 0; frame < rates[rate]; frame += blocks[rate])
                {
                    int n = rates[rate] - frame;
                    if (n > blocks[rate]) n = blocks[rate];
                    ft2_unified_synth_render_channel(1, left, right, n, 0);
                    for (int i = 0; i < n; i++)
                    {
                        float values[] = {left[i], right[i]};
                        for (int ch = 0; ch < 2; ch++)
                        {
                            float value = values[ch];
                            if (!isfinite(value)) { ok = false; continue; }
                            if (fabsf(value) > peak) peak = fabsf(value);
                            if (fabsf(value) > 1.0f) overs++;
                            sum += value;
                            squares += (double)value * value;
                            samples++;
                        }
                    }
                }
                printf("LEVEL,%s,%d,%d,%d,%.8g,%.8g,%.8g,%d\n", engine->engineName,
                    rates[rate], blocks[rate], preset, peak, sqrt(squares / samples), sum / samples, overs);
                /* Some hardware ROM presets are intentionally very quiet. A peak
                   floor catches a silent render without imposing a loudness policy. */
                if (peak < 1.0e-6f) {
                    fprintf(stderr, "Inaudible factory fixture: %s preset %d at %d Hz\n",
                        engine->engineName, preset, rates[rate]);
                    ok = false;
                }
                engine->panic();
            }
            if (engine->clear_state) engine->clear_state(1);
        }
        ft2_unified_synth_shutdown();
    }
    freeInstr(1);
    return ok;
}
