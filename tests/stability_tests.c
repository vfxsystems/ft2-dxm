#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ft2_header.h"
#include "ft2_structs.h"
#include "ft2_config.h"
#include "ft2_edit.h"
#include "ft2_keyboard.h"
#include "ft2_events.h"
#include "ft2_audio.h"
#include "ft2_bmp.h"
#include "ft2_gui.h"
#include "ft2_v2.h"
#include "ft2_dexed.h"
#include "ft2_unified_synth.h"
#include "ft2_v2_complete_layout.h"
#include "ft2_video.h"

static int failures;
static int retriggerCount;
static void constantSynth(int instrument, float *left, float *right, int count, int add)
{
    (void)instrument;
    for (int i = 0; i < count; ++i) {
        left[i] = (add ? left[i] : 0) + 1.2f;
        right[i] = (add ? right[i] : 0) + 1.2f;
    }
}
static void countNoteOn(int instrument, const MidiMessage *message)
{
    (void)instrument;
    if ((message->status & 0xF0) == 0x90 && message->data2 != 0) retriggerCount++;
}
extern bool runAudioRegressionTests(void);
extern bool runDexedRegressionTests(void);
extern bool runDxmChunkRegressionTests(void);
extern bool runSampleEditorRegressionTests(void);
extern bool runTunefishLayoutRegressionTests(void);
extern bool runRenderSettingsRegressionTests(void);
extern bool runS3MLoaderRegressionTests(void);
extern bool runDigiLoaderRegressionTests(void);
extern bool runBEMLoaderRegressionTests(void);
extern bool runITLoaderRegressionTests(void);
extern bool runXMLoaderRegressionTests(void);
extern bool runBRRLoaderRegressionTests(void);
extern bool runIFFLoaderRegressionTests(void);
extern bool runWAVLoaderRegressionTests(void);
extern bool runAIFFLoaderRegressionTests(void);
#ifdef HAS_MIDI
extern bool runMidiRegressionTests(void);
#endif
extern bool runSynthMuteRegressionTest(void);
#define CHECK(expr, message) do { if (!(expr)) { fprintf(stderr, "stability: %s\n", message); failures++; } } while (0)

static void renderRelease(int instrument)
{
    float left[256], right[256];
    for (int i = 0; i < 600; i++)
        ft2_v2_render_for_channel(instrument, left, right, 256, 0);
}

static void checkBitmapCompositing(void)
{
    uint32_t *previous = video.frameBuffer;
    uint32_t *framebuffer = (uint32_t *)calloc((size_t)SCREEN_W * SCREEN_H, sizeof(uint32_t));
    CHECK(framebuffer != NULL, "allocate bitmap compositor fixture");
    if (!framebuffer)
        return;

    video.frameBuffer = framebuffer;
    framebuffer[10 * SCREEN_W + 10] = 0xFF0000FFu;
    framebuffer[10 * SCREEN_W + 11] = 0xFF123456u;
    const uint32_t alpha_pixels[2] = { 0x80FF0000u, 0x00010203u };
    blit32Alpha(10, 10, alpha_pixels, 2, 1, 255);
    CHECK(framebuffer[10 * SCREEN_W + 10] == 0xFF80007Fu,
          "runtime bitmap renderer preserves eight-bit pixel alpha");
    CHECK(framebuffer[10 * SCREEN_W + 11] == 0xFF123456u,
          "runtime bitmap renderer skips zero-alpha pixels");

    framebuffer[11 * SCREEN_W + 10] = 0xFF0000FFu;
    blit32Alpha(10, 11, alpha_pixels, 1, 1, 128);
    CHECK(framebuffer[11 * SCREEN_W + 10] == 0xFF4000BFu,
          "runtime bitmap renderer multiplies schema opacity by pixel alpha");

    const uint32_t clipped_pixels[2] = { 0xFFFF0000u, 0xFF00FF00u };
    blit32Alpha(-1, 0, clipped_pixels, 2, 1, 255);
    CHECK(framebuffer[0] == 0xFF00FF00u,
          "runtime bitmap renderer clips safely and does not color-key opaque green");

    const uint32_t bounded_pixels[3] = { 0xFFFF0000u, 0xFF00FF00u, 0xFF0000FFu };
    framebuffer[12 * SCREEN_W + 9] = 0xFF101010u;
    framebuffer[12 * SCREEN_W + 10] = 0xFF101010u;
    framebuffer[12 * SCREEN_W + 11] = 0xFF101010u;
    blit32AlphaClip(9, 12, bounded_pixels, 3, 1, 255, 10, 12, 1, 1);
    CHECK(framebuffer[12 * SCREEN_W + 9] == 0xFF101010u &&
          framebuffer[12 * SCREEN_W + 10] == 0xFF00FF00u &&
          framebuffer[12 * SCREEN_W + 11] == 0xFF101010u,
          "runtime bitmap renderer stays inside descriptor bounds");

    video.frameBuffer = previous;
    free(framebuffer);
}

static void checkSynthMixing(void)
{
    float left[1025], right[1025], expected[1025];
    instr[1]->useV2 = false;
    instr[1]->useDexed = true;
    for (int add = 0; add <= 1; ++add) {
        ft2_dx_panic();
        CHECK(ft2_dx_load_factory_preset_for_instrument(1, 1), "load Dexed mix fixture");
        ft2_dx_send_midi_to_instrument(1, 0x90, 60, 127);
        for (int i = 0; i < 1025; ++i) left[i] = right[i] = 0.25f;
        ft2_dx_render_for_channel(1, left, right, 1025, add);
        for (int i = 0; i < 1025; ++i) {
            if (!add) expected[i] = left[i];
            else CHECK(fabsf(left[i] - expected[i] - 0.25f) < 1e-6f,
                "Dexed additive render preserves the existing bus");
        }
    }
    ft2_dx_panic();
    const UnifiedSynthInterface *dx = ft2_unified_synth_get_engine(SYNTH_TYPE_DEXED);
    uint8_t dxBefore[155], dxAfter[155];
    CHECK(ft2_dx_get_patch_data(1, dxBefore, 155) == 155, "save Dexed fixture");
    dx->store_state(1);
    CHECK(dx->has_state(1), "Dexed reports stored patch state");
    CHECK(dx->load_preset(1, 2), "change Dexed patch");
    dx->restore_state(1);
    CHECK(ft2_dx_get_patch_data(1, dxAfter, 155) == 155 && !memcmp(dxBefore, dxAfter, 155),
        "Dexed restores every patch byte");
    dx->clear_state(1);
    CHECK(!dx->has_state(1), "Dexed clear discards stored state");
    instr[1]->useDexed = false;
    instr[1]->useTF4 = true;
    const UnifiedSynthInterface *tf = ft2_unified_synth_get_engine(SYNTH_TYPE_TUNEFISH4);
    UnifiedSynthInterface originalTf = *tf, mockTf = *tf;
    mockTf.render_for_channel = constantSynth;
    ft2_unified_synth_register_engine(&mockTf);
    const float limited = 0.95f + 0.05f * tanhf((1.2f - 0.95f) / 0.05f);
    for (int i = 0; i < 1025; ++i) left[i] = right[i] = 0.25f;
    ft2_unified_synth_render_channel(1, left, right, 1025, 1);
    for (int i = 0; i < 1025; ++i)
        CHECK(fabsf(left[i] - 0.25f - limited) < 1e-6f, "TF4 limiter processes only its own contribution");
    ft2_unified_synth_register_engine(&originalTf);
    for (int param = 0; param < tf->get_param_count(); ++param) {
        const ParameterRange *range = tf->get_param_range(param);
        CHECK(range && range->min == 0 && range->max == 1, "all TF4 macro parameters have normalized range");
    }
    CHECK(tf->load_preset(1, 0), "load TF4 panic fixture");
    MidiMessage on = {0x90, 60, 127};
    tf->send_midi(1, &on);
    tf->render_for_channel(1, left, right, 256, 0);
    CHECK(tf->get_active_voices(1) > 0, "TF4 voice starts before panic");
    tf->panic();
    CHECK(tf->get_active_voices(1) == 0, "unified TF4 panic kills held voices");
    uint8_t saved[516], restored[516];
    CHECK(tf->save_patch(1, saved, sizeof(saved)) == sizeof(saved), "serialize TF4 patch");
    tf->set_param(1, 0, 0.123f);
    CHECK(tf->load_patch(1, saved, sizeof(saved)), "restore TF4 patch");
    CHECK(tf->save_patch(1, restored, sizeof(restored)) == sizeof(restored) &&
        !memcmp(saved, restored, sizeof(saved)), "TF4 patch round trip preserves every parameter");
    CHECK(!tf->load_patch(1, saved, sizeof(saved) - 1), "reject truncated TF4 patch");
    saved[4] = 0; saved[5] = 0; saved[6] = 0xC0; saved[7] = 0x7F;
    CHECK(!tf->load_patch(1, saved, sizeof(saved)), "reject NaN TF4 parameter");
    CHECK(tf->save_patch(1, saved, sizeof(saved)) == sizeof(saved) &&
        !memcmp(saved, restored, sizeof(saved)), "invalid TF4 patch does not partially replace state");
    tf->clear_state(1);
    instr[1]->useTF4 = false;
    instr[1]->useV2 = true;
}

bool runStabilityTests(void)
{
    failures = 0;
    checkBitmapCompositing();
    CHECK(runDexedRegressionTests(), "Dexed envelope and panic regression suite");
    CHECK(runDxmChunkRegressionTests(), "DXM bounded chunk and macro round-trip regression suite");
    CHECK(runSampleEditorRegressionTests(), "sample ownership and stereo paste conversion regression suite");
    CHECK(runTunefishLayoutRegressionTests(), "Tunefish effect-stack level routing regression suite");
    CHECK(runRenderSettingsRegressionTests(), "render settings bounds and option regression suite");
    CHECK(runS3MLoaderRegressionTests(), "S3M bounded event decoding regression suite");
    CHECK(runDigiLoaderRegressionTests(), "DIGI truncated event regression suite");
    CHECK(runBEMLoaderRegressionTests(), "BEM bounded track decoder regression suite");
    CHECK(runITLoaderRegressionTests(), "IT bounded pattern and sample decoder regression suite");
    CHECK(runXMLoaderRegressionTests(), "XM bounded pattern, stereo, and ADPCM decoder regression suite");
    CHECK(runBRRLoaderRegressionTests(), "BRR bounded block decoder regression suite");
    CHECK(runIFFLoaderRegressionTests(), "IFF bounded chunk decoder regression suite");
    CHECK(runWAVLoaderRegressionTests(), "WAV bounded chunk and sample decoder regression suite");
    CHECK(runAIFFLoaderRegressionTests(), "AIFF bounded chunk and sample decoder regression suite");
    song.numChannels = 4;
    editor.curInstr = 1;
    editor.curOctave = 4;
    cursor.ch = 0;
    cursor.object = CURSOR_NOTE;
    playMode = PLAYMODE_IDLE;
    config.multiKeyJazz = false;
    CHECK(allocateInstr(1) && allocateInstr(2), "allocate test instruments");
    if (failures) return false;
    instr[1]->useV2 = instr[2]->useV2 = true;
    ft2_unified_synth_init(44100);
    checkSynthMixing();
    UnifiedSynthInterface original = *ft2_unified_synth_get_engine(SYNTH_TYPE_V2);
    UnifiedSynthInterface counted = original;
    counted.send_midi = countNoteOn;
    ft2_unified_synth_register_engine(&counted);
    retriggerCount = 0;
    ft2_send_synth_midi_dedup(0, 1, 0x90, 60, 100);
    ft2_send_synth_midi_dedup(0, 1, 0x90, 60, 100);
    CHECK(retriggerCount == 2, "identical note-ons retrigger even when song.tick repeats");
    ft2_send_synth_midi_dedup(0, 1, 0x80, 60, 0);
    ft2_unified_synth_register_engine(&original);
    for (int i = 1; i <= 2; i++)
    {
        CHECK(ft2_v2_load_factory_preset_for_instrument(i, 0), "load V2 fixture");
        ft2_v2_set_param_for_instrument(i, 36, 0.0f); /* short release */
    }

#ifdef HAS_MIDI
    CHECK(runMidiRegressionTests(), "MIDI input ownership and queue regression suite");
#endif
    CHECK(ft2_v2_load_factory_preset_for_instrument(1, 2),
        "load French Horn V2 page-render fixture");
    V2CompleteLayout *layout = v2_create_complete_layout();
    CHECK(layout != NULL, "create V2 layout");
    if (layout)
    {
        uint32_t *const previousFrameBuffer = video.frameBuffer;
        const bool bitmapsLoaded = loadBMPs();
        video.frameBuffer = calloc((size_t)SCREEN_W * SCREEN_H, sizeof (*video.frameBuffer));
        CHECK(bitmapsLoaded, "load V2 layout regression graphics");
        CHECK(video.frameBuffer != NULL, "allocate V2 layout regression framebuffer");
        v2_show_layout(layout);
        ui.synthEditorShown = true;
        TunefishWidget *buttons[] = {layout->page_voice_button, layout->page_filter_button,
            layout->page_lfo_env_button, layout->page_fx_button, layout->page_master_button,
            layout->page_mod_button};
        for (int page = 0; page < V2_PAGE_COUNT; page++)
        {
            TunefishWidget *b = buttons[page];
            CHECK(b != NULL, "page button exists");
            if (!b) continue;
            v2_handle_layout_mouse_event(layout, b->x + 2, b->y + 2, true);
            v2_handle_layout_mouse_event(layout, b->x + 2, b->y + 2, false);
            CHECK(layout->current_page == page, "mouse selects requested page");
            if (bitmapsLoaded && video.frameBuffer != NULL)
                v2_render_complete_layout(layout);
            for (int j = 0; j < V2_PAGE_COUNT; j++)
                CHECK(buttons[j]->pressed == (j == page), "selected page survives mouse-up");
        }
        v2_switch_to_page(layout, V2_PAGE_VOICE_OSC);
        keyDownHandler(SDL_SCANCODE_TAB, SDLK_TAB, false);
        CHECK(layout->current_page == V2_PAGE_FILTER, "real keyboard dispatch reaches V2 Tab navigation");
        TunefishWidget *slider = NULL;
        for (int i = 0; i < layout->all_widget_count; i++)
        {
            TunefishWidget *w = layout->all_widgets[i];
            if (w->visible && w->type == TF_WIDGET_ROTARY_SLIDER) { slider = w; break; }
        }
        CHECK(slider != NULL, "filter page has slider");
        if (slider)
        {
            float before = slider->value;
            v2_handle_layout_mouse_event(layout, 0, 0, true);
            v2_handle_layout_mouse_drag(layout, slider->x + 1, slider->y + 1);
            CHECK(slider->value == before, "drag from empty space does not edit hovered slider");
            v2_handle_layout_mouse_event(layout, 0, 0, false);
            v2_handle_layout_mouse_event(layout, slider->x + 1, slider->y + 1, true);
            before = slider->value;
            v2_switch_to_page(layout, V2_PAGE_MOD);
            v2_handle_layout_mouse_drag(layout, slider->x + slider->w - 1, slider->y + 1);
            CHECK(slider->value == before, "page switch cancels old slider capture");
        }
        ui.synthEditorShown = false;
        v2_destroy_complete_layout(layout);
        free(video.frameBuffer);
        video.frameBuffer = previousFrameBuffer;
        if (bitmapsLoaded)
            freeBMPs();
    }
    ft2_v2_set_param_for_instrument(1, 36, 0.0f); /* restore short release */

    /* A held physical key still owns its original note after UI state changes. */
    for (int scenario = 0; scenario < 5; scenario++)
    {
        cursor.object = CURSOR_NOTE;
        cursor.ch = 0;
        editor.curOctave = 4;
        editor.curInstr = 1;
        keyDownHandler(SDL_SCANCODE_Z, SDLK_z, false);
        CHECK(editor.keyOnTab[0] != 0, "keyboard note starts");
        CHECK(ft2_v2_get_active_voice_count(1) > 0, "keyboard reaches the V2 engine");
        if (scenario == 0) editor.editTextFlag = true;
        if (scenario == 1) ui.sysReqShown = true;
        if (scenario == 2) { cursor.object = CURSOR_INST1; keyb.keyModifierDown = true; }
        if (scenario == 3) { editor.curOctave = 5; editor.curInstr = 2; cursor.ch = 1; }
        if (scenario == 4) keyb.ignoreCurrKeyUp = true;
        keyUpHandler(SDL_SCANCODE_Z, SDLK_z);
        CHECK(editor.keyOnTab[0] == 0, "original keyboard note is released despite UI changes");
        renderRelease(1);
        CHECK(ft2_v2_get_active_voice_count(1) == 0, "released V2 voice ends");
        editor.editTextFlag = ui.sysReqShown = keyb.keyModifierDown = keyb.ignoreCurrKeyUp = false;
    }
    cursor.object = CURSOR_NOTE;
    cursor.ch = 0;
    editor.curInstr = 1;
    keyDownHandler(SDL_SCANCODE_Z, SDLK_z, false);
    SDL_Event focus = {0};
    focus.type = SDL_WINDOWEVENT;
    focus.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    handleWaitVblQuirk(&focus);
    CHECK(editor.keyOnTab[0] == 0, "focus loss releases held keyboard notes");
    renderRelease(1);
    CHECK(ft2_v2_get_active_voice_count(1) == 0, "focus loss leaves no held V2 voice");

    ft2_v2_panic();
    memset(channel, 0, sizeof(channel));
    playTone(0, 1, 60, 64, 0, 0);
    playTone(0, 2, 64, 64, 0, 0);
    renderRelease(1);
    CHECK(ft2_v2_get_active_voice_count(1) == 0, "instrument replacement releases previous instrument");
    playTone(0, 2, NOTE_OFF, 0, 0, 0);
    renderRelease(2);
    CHECK(ft2_v2_get_active_voice_count(2) == 0, "replacement instrument releases normally");
    ft2_v2_panic();
    CHECK(runSynthMuteRegressionTest(), "muted synth route contributes silence");
    playTone(0, 1, 60, 64, 0, 0);
    stopVoices();
    CHECK(ft2_v2_get_active_voice_count(1) == 0, "stopVoices purges embedded voices");
    for (int i = 1; i <= 2; i++) ft2_v2_clear_persistent_state(i);
    ft2_unified_synth_shutdown();
    freeInstr(1);
    freeInstr(2);
    memset(channel, 0, sizeof(channel));
    CHECK(runAudioRegressionTests(), "audio callback/export gain regression suite");
    if (!failures) puts("ft2-dxm stability tests passed");
    return failures == 0;
}
