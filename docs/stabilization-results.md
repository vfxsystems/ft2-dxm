# Stabilization results

Working changes based on `84dbf0bf7cacc8cef8e8c2901e49e16d7bd4b00d`.
This records verified fixes and remaining release work; it is not a declaration
that the entire GitHub-readiness plan is complete.

## Baseline

Environment: Linux, GCC 11.4, CMake 4.4.3, SDL 2.0.20. The four existing CTests
passed in Release, Debug, and ASan/leak detection. LeakSanitizer required a run
outside the sandbox because ptrace supervision prevents its leak scan.

New regression tests run against the unchanged Release objects reproduced V2
page-button deselection, missing Tab dispatch, editing a slider by dragging from
empty space, lost key releases after UI/octave/instrument changes, focus-loss
notes, and release sent to the wrong instrument. Audio tests then reproduced
master gain/export discrepancies and the stateless-gainer bypass.

## Implemented

- V2 page selection persists after mouse release; actual keyboard dispatch reaches Tab/Escape handling. Slider drags belong to the pressed control and page changes cancel capture.
- Physical note keys retain original note/channel/instrument ownership. Key-up runs before text/modal/cursor filtering; focus loss releases held keys without recording edits.
- Synth note ownership survives tracker-channel instrument changes. Repeated identical note-ons retrigger; stop/reset purges embedded voices. Muted synth routes contribute silence.
- Live, WAV, and slot rendering share master DSP/fader/configured volume processing. Stateless gain effects work and large offline blocks no longer bypass DSP.
- Slot rendering saves the full mute array before restoring it. TF4 preset loading initializes a fresh instrument when necessary.
- Normal builds do not embed or copy local Virus firmware. External ROM selection and `OsTIrus/rom.bin` beside the executable are supported on every target; build wrappers report the expected runtime path, and public tests explicitly disable ROM discovery.
- Source exporter omits firmware/runtime assets, transcripts, development metadata, and Git history. README/component inventory and Linux CI were added/updated.
- Dexed patch revisions prevent pending updates from skipping a newly triggered voice's attack. Panic clears buffered audio, filter history, and sustain. MIDI channel identity survives the wrapper; note-off and sustain release affect the matching channel.
- Dexed additive rendering preserves existing bus audio using bounded scratch blocks. Rendering all engines clears the bus once, then sums contributions. TF4's additive limiter processes only TF4 audio.
- TF4's unified panic and patch serialization now work; all exposed macro parameters have a 0–1 range. Dexed's unified patch-state save/restore and voice counts replace empty handlers. Unused TF4 setters that wrote unrelated parameter IDs were removed.
- MIDI callbacks now publish validated packets to a bounded queue instead of editing tracker state. Application-thread processing tracks ownership by input channel and wire note; transpose/filter/cursor changes do not redirect releases. Sustain, velocity-zero note-off, device close, malformed input, and overflow recovery have regression coverage, including concurrent producer/consumer traffic. Dialogs continue processing releases and suppress new MIDI notes.
- Dexed render/note-start consume fixed-size atomic patch snapshots with one bounded read attempt. Dynamic patch queues and reader-side mutex acquisition were removed. Concurrent patch publication is tested; patch/parameter writers are still serialized by a mutex.
- Removed the disabled resampling stub and obsolete V2 preparation files, including declarations and feature flags with no implementation. The active slot renderer and V2 engine remain in place.
- DXM chunk loading now accounts for every byte before reading, rejects duplicate or oversized synth blobs, validates finite/ranged state, and clears staged synth data after any failed load. Regression fixtures cover valid and truncated macro, metadata, arpeggiator, V2, and OsTIrus chunks.
- DXM saving now writes through a same-directory temporary file and atomically replaces the destination only after a successful flush and close. Sample payload writes, synth serialization, file positions, 32-bit chunk lengths, and every length backpatch are checked; a failed save preserves the prior destination and restores playback sample padding.
- Stereo range paste now allocates and commits independent left/right buffers, retains channel separation through 8/16-bit conversion, uses checked length arithmetic, and adjusts loops from the actual inserted-length delta. Sample pointer ownership and conversion are regression tested.
- Tunefish stack level knobs route to the selected effect's real amount, gain, or wet parameter. They are disabled for Delay and EQ because those effects expose no single level control; Delay no longer changes feedback under a wet label. LFO waveform buttons retain the engine's zero-based shape index, so UI selection stays synchronized after edits.
- Render Settings treats Escape and every non-OK close as cancellation, bounds sample slots and supported rates, and restores the WAV screen's complete radio-button definitions. Option transitions are regression tested.
- Removed dormant Dexed plugin-host, ZIP, and unimplemented tuning shims, the unused envelope UI, and generated OsTIrus placeholder aliases. Debug event logging is compile-time opt-in.
- Legacy S3M import now validates signed header counts, file offsets, packed event boundaries, and decoded stereo/16-bit sample sizes before reading or allocating. DIGI and MOD imports require complete pattern events and sample payloads instead of decoding stale bytes or padding truncated files.
- BEM import now validates every metadata, instrument, track-table, track, and sample read. Track storage is sized from validated pattern references, compressed opcodes decode from bounded buffers, repeat runs cannot overrun row storage, and temporary decoded tracks are released on success and failure.
- IT import now validates signatures, count tables, offsets, instrument/sample references, pattern bounds, loop ranges, and complete sample payloads before use. Pattern events and compressed 8/16-bit samples decode through bounded readers; invalid channel descriptors, bit-width transitions, truncated blocks, and row overruns fail cleanly. Empty instruments no longer access an uninitialized sample header.
- XM import now validates module, pattern, instrument, extended sample-header, loop, and sample payload boundaries. Packed events use the declared payload size, odd ADPCM lengths do not write an extra sample, and XM 1.02/1.03 retains unsupported-sample skip lengths per instrument. ModPlug/OpenMPT stereo samples now use their defined planar channel layout, independent delta decoding, and consistent frame/loop units.
- Shared sample allocation and reallocation now compute byte counts in `size_t`, reject negative or over-limit frame counts before touching existing buffers, and include interpolation padding without signed overflow at the configured maximum 16-bit sample length.
- BRR import validates block structure and complete input bytes, avoids signed-shift undefined behavior during prediction, and rejects invalid loop offsets. IFF import validates FORM/chunk headers, padded chunk extents, VHDR fields, body reads, loop arithmetic, and names; truncated bodies no longer allocate or decode partial data.

## Validation

The expanded six-test CTest suite passed in Release and Debug and under
ASan/leak detection after the loader, saver, stereo editor, effect-stack, and
legacy decoder changes. Python support tests cover source exclusions, nested
firmware archives, credential-signature rejection, and custom build-directory
propagation. The suite includes
numerical live/export checks, original unity transfer, 16-bit conversion,
large-block DSP, synth mute/stop, input sequences, and existing V2 persistence
stress coverage. The exported snapshot also built from scratch under `/tmp`
and passed all six tests with no local ROM/archive directory or Git history.

Windows MinGW, macOS, and Raspberry Pi cross-builds were unavailable because
their toolchains are not installed. They are skips, not passes. The target-test
wrapper now preserves failing command exit codes in strict mode.

## Level probe findings

The first three factory patches were measured at 44100/64, 48000/256, and
96000/1024 sample-rate/block-size combinations, using four notes at velocity 127.
TF4 reaches its existing engine limiter; V2 can exceed unity before the mixer
(one measured peak was approximately 1.50). The original Dexed measurements
included silence and very low levels. A decaying-sine regression reproduced a
skipped-attack defect at all three rates. After the fix, sampled factory patches
have RMS levels around 0.09–0.20 without changing engine gain. Level tests now
reject silent factory fixtures as well as non-finite samples while accepting
intentionally quiet hardware presets. All measured samples were finite. Local
probes through both `FT2_OSTIRUS_ROM` and `OsTIrus/rom.bin` beside the executable
completed all nine OsTIrus cases (maximum observed peak approximately 0.138),
including an ASan/UBSan/leak-detection run.
The 36 measurement rows are in [synth-levels.csv](synth-levels.csv); they contain
measurements only, not firmware or patch data. Public tests explicitly skip
OsTIrus measurements without firmware.

## Remaining release gates

- Confirm the user's exact V2 page-switching reproduction and perform real UI/audio-device smoke checks; headless event tests do not establish visual correctness.
- Broaden patch calibration and validate OsTIrus live playback. Choose headroom/smoothing policy from those results.
- Finish the realtime ownership audit: MIDI dispatch now follows the application input loop (roughly 60 Hz), adding frame-dependent latency compared with direct device-thread dispatch. Measure end-to-end latency and move audition scheduling to an audio-owned command path before making low-latency performance claims. Dexed automation writers still acquire a mutex, and other engines/control paths require review.
- Verify physical MIDI unplug/reconnect on actual devices. Explicit device close releases owned notes, but silent hardware disappearance without a close notification is not automatically detected.
- Validate MIDI sustain/disconnect, shared-instrument voice isolation, dense arrangements, time-dependent effects, and complete DXM/DXI project round trips. Include interrupted/failed saves and stereo sample round trips on real projects. The existing shared-instance/channel mapping is documented in the mixer contract.
- Run coverage-guided fuzzing across every module and sample importer before accepting untrusted files as a hardened production claim. DXM has bounded-reader coverage; S3M, DIGI, MOD, BEM, IT, XM, BRR, IFF, WAV, and AIFF now reject truncated or structurally invalid payloads. WAV and AIFF channel selection is consistent across supported integer and floating-point depths, with planar stereo output when requested.
- Perform listening comparisons with representative user projects. Configuration volume and previously bypassed gainer/export controls now work, so old projects relying on those omissions may sound different; see the compatibility notes.
- Complete asset/preset provenance review and native target/device validation before publishing. No repository upload, history rewrite, or release tag has been performed.

