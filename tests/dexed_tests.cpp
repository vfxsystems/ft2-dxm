#include "dexed/dexed_audio.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <atomic>
#include <thread>

extern "C" bool runDexedRegressionTests(void)
{
    // A decaying sine with zero sustain exposes updates that skip the attack.
    std::array<uint8_t, 155> patch{};
    for (int op = 0; op < 6; ++op) {
        const int base = op * 21;
        patch[base] = 99;
        patch[base + 1] = 50;
        patch[base + 2] = 50;
        patch[base + 3] = 99;
        patch[base + 4] = 99;
        patch[base + 18] = 1;
        patch[base + 20] = 7;
    }
    patch[121] = 99;
    for (int i = 126; i < 130; ++i) patch[i] = 99;
    for (int i = 130; i < 134; ++i) patch[i] = 50;
    patch[134] = 31;
    patch[136] = 1;
    patch[144] = 24;
    bool passed = true;
    for (const int rate : {44100, 48000, 96000}) {
        DexedAudio immediate, primed;
        immediate.prepareToPlay(rate, 256);
        primed.prepareToPlay(rate, 256);
        immediate.queuePatchBlob(patch.data(), patch.size());
        primed.loadPatchBlob(patch.data(), patch.size());
        std::array<float, 4096> left{}, right{}, reference{}, scratch{};
        primed.render(reference.data(), scratch.data(), 64);
        immediate.midiNoteOn(60, 127);
        primed.midiNoteOn(60, 127);
        immediate.render(left.data(), right.data(), left.size());
        primed.render(reference.data(), scratch.data(), reference.size());
        double energy = 0, difference = 0;
        for (size_t i = 0; i < left.size(); ++i) {
            energy += left[i] * left[i];
            difference += std::abs(left[i] - reference[i]);
        }
        if (!(energy > 0.01) || difference > 0.0001) {
            std::fprintf(stderr, "Dexed attack %d Hz: energy=%g difference=%g\n", rate, energy, difference);
            passed = false;
        }
        // Leave a partially consumed internal block, then demand silence.
        immediate.render(left.data(), right.data(), 7);
        immediate.panic();
        immediate.render(left.data(), right.data(), left.size());
        for (size_t i = 0; i < left.size(); ++i) {
            if (left[i] != 0 || right[i] != 0) {
                std::fprintf(stderr, "Dexed panic left buffered audio at %d Hz\n", rate);
                passed = false;
                break;
            }
        }
        // Sustain belongs to the incoming MIDI channel, including equal pitches.
        immediate.sendMidi(0x90, 60, 127);
        immediate.sendMidi(0x91, 60, 127);
        immediate.sendMidi(0xB0, 64, 127);
        immediate.sendMidi(0x81, 60, 0);
        for (int block = 0; block < 50; ++block)
            immediate.render(left.data(), right.data(), left.size());
        if (immediate.getActiveVoiceCount() != 1) {
            std::fprintf(stderr, "Dexed released the wrong MIDI channel at %d Hz\n", rate);
            passed = false;
        }
        immediate.sendMidi(0x80, 60, 0);
        immediate.render(left.data(), right.data(), left.size());
        if (immediate.getActiveVoiceCount() != 1) passed = false;
        immediate.sendMidi(0xB0, 64, 0);
        for (int block = 0; block < 50; ++block)
            immediate.render(left.data(), right.data(), left.size());
        if (immediate.getActiveVoiceCount() != 0) {
            std::fprintf(stderr, "Dexed sustain release stuck at %d Hz\n", rate);
            passed = false;
        }
    }
    DexedAudio concurrent;
    concurrent.prepareToPlay(48000, 64);
    concurrent.loadPatchBlob(patch.data(), patch.size());
    concurrent.midiNoteOn(60, 127);
    std::atomic<bool> finished{false};
    std::thread publisher([&] {
        auto changed = patch;
        for (int i = 0; i < 4000; ++i) {
            changed[121] = (i & 1) ? 99 : 90;
            concurrent.queuePatchBlob(changed.data(), changed.size());
        }
        finished.store(true);
    });
    std::array<float, 64> left{}, right{};
    do {
        concurrent.render(left.data(), right.data(), left.size());
        for (float value : left) if (!std::isfinite(value)) passed = false;
    } while (!finished.load());
    publisher.join();
    concurrent.panic();
    return passed;
}
