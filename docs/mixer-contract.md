# Mixer behavior and compatibility

The processing order is sample/synth source, assigned stereo strip, strip DSP,
strip fader and linear stereo balance, existing strip saturation, master sum,
master DSP, master fader and configured amplification/volume, existing output
saturation, then float or 16-bit conversion. WAV export and render-to-slot use
the same master processing as device playback.

## Gains

- Strip and master faders use linear gain; 1 is unity, 0 is silence, 2 is approximately +6 dB.
- Strip balance is 1/1 at center, 1/0 fully left, and 0/1 fully right. It attenuates the opposite side of stereo; it does not fold stereo to mono.
- Configured amplification and master volume contribute `(amp / 32) * (volume / 256)` once in the float domain. Integer PCM scaling occurs only at conversion.
- Existing engine trims and `tanh` saturation stages remain unchanged. They color signals below full scale too; they are not transparent peak-only protection.
- A synth instance renders once per instrument per block and distributes its output among routes. Muted routes contribute no output, while rendering still advances the engine.

## Intentional corrections affecting existing projects

Previously the configuration amplification/volume multiplier was computed but
unused. These controls now work. To match the previous effective unity at this
stage, use amplification 32 and master volume 256. Previously saved values below
these maxima now attenuate the output as their controls indicate.

Master effects/fader now affect WAV and slot export, matching playback. Enabled
gainer effects now process audio (the chain previously skipped stateless effects).
DSP no longer bypasses blocks larger than 8192 frames. Projects relying on those
omissions can sound different. No DXM/DXI file-format fields or synth patch
defaults were changed.

Dexed patches now play their attack immediately after loading. Previously a
pending update could skip the attack and make decaying patches silent. Its
engine gain is unchanged, but affected patches are substantially louder after
this correction. Additive synth rendering preserves existing bus contributions;
TF4 limiting applies only to its own contribution.

The unified TF4 patch API uses `TFP1` followed by 128 little-endian IEEE-754
32-bit normalized parameters (516 bytes total). Loading validates the complete
blob before changing any parameter. This API format does not change DXM/DXI
serialization. Dexed's unified state snapshot contains its 155-byte unpacked
patch; runtime voices and wrapper filter/controller settings are not serialized
by that snapshot.

## Remaining architectural limits

The existing channel-to-strip mapping is retained: tracker channels 0–15 map
directly to strips 0–15; channels 16–31 map by `channel / 2`. Changing this mapping
requires a project compatibility decision. Multiple tracker channels sharing one
instrument also share its stereo synth output: independent post-synth isolation
of individual voices is not provided by the current instance model.

Live/offline numerical tests currently cover deterministic source audio, master
gain and effects, conversion, mute routing, and large DSP blocks. Time-dependent
effects, full project round trips, actual devices, and representative listening
comparisons require additional validation before claiming complete equivalence.

## Measurements

With testing enabled, `ft2-dxm --synth-level-test` reports `LEVEL` CSV rows for
three factory patches per available engine, four-note chords at velocity 127,
and 44100/64, 48000/256, 96000/1024 rate/block combinations. Measurements are at
the unified engine output, before strip/master processing. Missing presets or
Virus firmware are reported as skips. This is a calibration probe, not evidence
that every patch is within headroom or equally loud.
