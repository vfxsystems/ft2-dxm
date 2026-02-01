/*
 * Render selected tracker song range (and channel mask) directly into the
 * currently selected sample slot. This re-uses the WAV renderer's internal
 * mixing path, but instead of writing to a WAV file we copy the generated
 * audio into an in-memory buffer which is finally assigned to the sample.
 *
 * NOTE 1: Only 16-bit stereo output is generated for now – the tracker uses
 *         16-bit sample buffers internally, so 32-bit float rendering is
 *         down-converted.
 * NOTE 2: This is intended as a first workable implementation. Optimisation
 *         (e.g. direct float → 32-bit PCM) can follow later.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "ft2_header.h"
#include "ft2_audio.h"
#include "ft2_sample_ed.h"
#include "ft2_replayer.h"
#include "ft2_gui.h"
#include "ft2_wav_renderer.h"
#include "ft2_structs.h" /* for extern editor_t etc. */

/* forward declarations of internal wav renderer helpers */
bool dump_Init(uint32_t frq, int16_t amp, int16_t songPos);
void dump_TickReplayer(void);
bool dump_EndOfTune(int16_t endSongPos);
void dump_Close(FILE *f, uint32_t totalSamples);

#define TEMP_GROW_SAMPLES  (TICKS_PER_RENDER_CHUNK * 4096) /* grows ~12kB per chunk */

/* Allocate/realloc helper that keeps total sample count */
static int16_t *growBuffer(int16_t *buf, size_t *capSamples, size_t needed)
{
    if (needed <= *capSamples)
        return buf;

    size_t newCap = *capSamples;
    while (newCap < needed)
        newCap += TEMP_GROW_SAMPLES;

    int16_t *newBuf = (int16_t *)realloc(buf, newCap * sizeof(int16_t) * 2); /* *2 for stereo */
    if (newBuf == NULL)
        return NULL;

    *capSamples = newCap;
    return newBuf;
}

/* Public API called from wav renderer UI */
bool renderSelectionToSlot(uint32_t channelMask, uint16_t startPos, uint16_t stopPos, uint8_t bitDepth)
{
    (void)bitDepth; /* currently always 16-bit */

    /* Backup and apply channel mute mask */
    bool oldMute[MAX_CHANNELS];
    for (int i = 0; i < MAX_CHANNELS && i < 16; i++)
    {
        oldMute[i] = editor.channelMuted[i];
        editor.channelMuted[i] = ((channelMask & (1U << i)) == 0);
    }

    pauseAudio();

    /* Re-use the WAV renderer init (makes copies of current mute flags) */
    uint8_t oldBitDepth = WDBitDepth;
    WDBitDepth = 16;

    if (!dump_Init(WDFrequency, WDAmp, startPos))
    {
        fprintf(stderr, "[RENDER-SLOT] dump_Init() failed (freq=%u, amp=%d, startPos=%u)\n", WDFrequency, WDAmp, startPos);
        resumeAudio();
        memcpy(editor.channelMuted, oldMute, sizeof(oldMute));
        WDBitDepth = oldBitDepth;
        return false;
    }

    int16_t *mixBuf = NULL;
    size_t  capSamples = 0;   /* per channel */
    size_t  totalSamples = 0; /* per channel */

    bool renderDone = false;
    uint64_t tickFrac = 0;

    while (!renderDone)
    {
        uint8_t *ptr8 = wavRenderBuffer; /* points into temp chunk */
        uint32_t samplesInChunk = 0;     /* *stereo* (L+R) samples */

        for (uint32_t i = 0; i < TICKS_PER_RENDER_CHUNK; i++)
        {
            if (!editor.wavIsRendering || dump_EndOfTune(stopPos))
            {
                renderDone = true;
                break;
            }

            dump_TickReplayer();
            uint32_t tickSmp = audio.samplesPerTickInt;
            if (!useLegacyBPM)
            {
                tickFrac += audio.samplesPerTickFrac;
                if (tickFrac >= BPM_FRAC_SCALE)
                {
                    tickFrac &= BPM_FRAC_MASK;
                    tickSmp++;
                }
            }

            mixReplayerTickToBuffer(tickSmp, ptr8, 16);

            tickSmp *= 2; /* stereo */
            samplesInChunk += tickSmp;

            ptr8 += tickSmp * sizeof(int16_t);
        }

        if (samplesInChunk > 0)
        {
            /* Ensure destination buffer can hold new samples */
            size_t needed = totalSamples + samplesInChunk;
            mixBuf = growBuffer(mixBuf, &capSamples, needed);
            if (mixBuf == NULL)
            {
                fprintf(stderr, "[RENDER-SLOT] Failed to grow mix buffer to %zu samples (OOM)\n", needed);
                dump_Close(NULL, 0); /* free buffers */
                resumeAudio();
                memcpy(editor.channelMuted, oldMute, sizeof(oldMute));
                WDBitDepth = oldBitDepth;
                return false;
            }

            /* Append chunk to big buffer */
            memcpy(&mixBuf[totalSamples], wavRenderBuffer, samplesInChunk * sizeof(int16_t));
            totalSamples += samplesInChunk;
        }
    }

    /* Close dump (frees internal temp buffer and restores mixer) */
    dump_Close(NULL, 0);
    resumeAudio();

    /* Restore previous mute/bitdepth */
    memcpy(editor.channelMuted, oldMute, sizeof(oldMute));
    WDBitDepth = oldBitDepth;

    /* Total samples per channel = totalSamples / 2 */
    size_t frames = totalSamples / 2;
    if (frames == 0 || mixBuf == NULL)
    {
        fprintf(stderr, "[RENDER-SLOT] Render produced zero frames (frames=%zu, buf=%p) – aborting\n", frames, (void*)mixBuf);
        free(mixBuf);
        return false;
    }

    /* Target sample pointer */
    instr_t *ins = instr[editor.curInstr];
    if (ins == NULL)
    {
        /* Try to allocate the instrument slot on-the-fly */
        if (!allocateInstr(editor.curInstr))
        {
            fprintf(stderr, "[RENDER-SLOT] allocateInstr(%d) failed – OOM\n", editor.curInstr);
            free(mixBuf);
            return false; /* out of memory */
        }
        ins = instr[editor.curInstr];
        if (ins == NULL)
        {
            free(mixBuf);
            return false; /* unexpected */
        }
    }
    sample_t *s = &ins->smp[editor.curSmp];

    /* Allocate sample memory (stereo, 16bit) */
    if (!allocateSmpData(s, (int32_t)frames, true, true))
    {
        fprintf(stderr, "[RENDER-SLOT] allocateSmpData() failed for %zu frames (stereo 16bit)\n", frames);
        free(mixBuf);
        return false;
    }

    s->flags |= (SAMPLE_16BIT | SAMPLE_STEREO);
    s->length = (int32_t)frames;
    s->loopStart = 0;
    s->loopLength = 0;
    s->volume = 64;
    s->panning = 128;
    setSampleC4Hz(s, WDFrequency);

    /* De-interleave L/R to tracker layout (separate buffers) */
    int16_t *dstL = (int16_t *)s->dataPtrL;
    int16_t *dstR = (int16_t *)s->dataPtrR;
    for (size_t i = 0; i < frames; i++)
    {
        dstL[i] = mixBuf[i * 2 + 0];
        dstR[i] = mixBuf[i * 2 + 1];
    }

    /* Prepare sample for playback by adding interpolation tap samples */
    fixSample(s);

    free(mixBuf);

    /* Trigger UI update */
    editor.updateCurSmp = true;
    editor.updateCurInstr = true;

    return true;
}
