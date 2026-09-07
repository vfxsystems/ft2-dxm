# GitHub readiness and stabilization plan

Status: approved for implementation. See [stabilization results](stabilization-results.md) for completed fixes, validation, and remaining release gates.

## Scope and order

Prepare a reproducible, documented source release with reliable V2 navigation, note lifetimes, and mixer behavior. Preserve tracker playback compatibility and existing DXM/DXI projects deliberately. Complete behavioral fixes before broad structural cleanup. Keep each fix and its regression coverage in a focused commit.

Publication inventory starts immediately; implementation proceeds through baseline, input/note fixes, mixer contract, gain calibration, then cleanup and release validation.

## Evidence from inspection

- `CMakeLists.txt` registers version, self-test, vector-theme self-test, and V2 stress tests. `scripts/test-linux.sh` provides Release, Debug, and ASan/leak phases. These are a starting point, not evidence that the reported bugs are covered.
- `docs/v2-stability.md` documents patch-bank lifetime and panic testing. `docs/ui-safety.md` documents overlay mouse ownership. Extend these contracts to page transitions and held-note release.
- `src/ft2_v2_complete_layout.c:v2_switch_to_page()` sets the page and updates visibility. Audit surrounding widget capture, popup, focus, and redraw behavior before identifying a root cause.
- `src/ft2_keyboard.c:keyUpHandler()` can return for text/modal state and gates note release on cursor/modifier state. These are candidate lost-release paths, not confirmed reproductions.
- `src/ft2_audio.c` applies master effects and master fader in the live callback; `mixReplayerTickToBuffer()` directly mixes and converts output. Trace the full callers to establish live/export parity. `setAudioAmp()` also includes master gain in a normalization value; determine whether that value is consumed before concluding gain is doubled.
- Both output conversion helpers apply `tanhf()` unconditionally. Characterize its effect on level and distortion before deciding output protection behavior.
- The sanitized `master` history excludes local OsTIrus firmware/runtime data,
  `gfxassets/`, and private assistant transcripts. OsTIrus remains compiled and
  accepts an owner-supplied `rom.bin` at runtime; source exports enforce the same exclusions.
- The README largely describes upstream FT2, and `LICENSES.txt` does not inventory the embedded synth integrations.

## 1. Establish a baseline and reproducible failures

- Record the starting commit, compiler, dependencies, platform, audio device, sample rate, and buffer size. Preserve existing local work, including the pre-existing deletion of `src/gemini.md`.
- Run the existing Linux test matrix and record failures before changing behavior. Separate unavailable platform checks from passes.
- Create minimal reproductions for V2 page switching, stuck notes, mixer routing, and inconsistent levels. Each record includes exact steps, expected/actual behavior, patch/module fixture, and input source (keyboard, MIDI, or tracker).
- Capture reference renders for sample-only XM playback and representative DXM projects before gain changes.

Done when: each reported bug has a repeatable case or a clearly recorded investigation status, and baseline results are saved.

## 2. Repair V2 navigation and note ownership

- Exercise every V2 page through mouse and keyboard navigation, including active slider drags, open dropdowns, held notes, instrument changes, editor close/reopen, and window focus loss.
- Make transitions clear stale widget capture/focus/popups and redraw the correct page. Hidden widgets must not receive input or modify parameters.
- Trace note-on through note-off across keyboard, MIDI, replayer, unified synth, and engine wrappers. Track releases against the original engine/instrument/channel/note, independently of the currently selected editor or instrument.
- Define handling for repeated notes, sustain pedal, transport stop, patch replacement, module load, focus loss, and MIDI disconnect. Page navigation should preserve legitimate held notes; release must still reach their owner. Panic is an explicit recovery/reset operation, not the normal page-switch mechanism.
- Review V2 patch storage and locking alongside note transitions; avoid introducing blocking or allocation into audio processing.
- Add event-sequence regression tests, retaining existing patch/persistence stress coverage. Validate both voice state and rendered output: normal release may have a bounded envelope/effect tail, whereas panic must clear voices and meet a defined silence bound.

Done when: all navigation/input reproductions pass, no hidden control receives events, no note remains orphaned, and V2 stress plus sanitizer checks pass.

## 3. Define and enforce the mixer contract

- Map current sample and synth paths, including channel ownership, stereo-pair routing, mute/solo, pan, inserts, sends/returns if present, master processing, output conversion, WAV export, and render-to-slot.
- Specify one authoritative processing order and who owns each gain. Distinguish tracker volume/effects, instrument output trim, strip fader, master fader, and device/output settings; document any intended export differences.
- Verify shared synth instruments render exactly once per required block and route correctly when used by multiple tracker channels. Check duplicate summing, stale scratch buffers, tail handling, stereo balance, and sample-rate/block-size changes.
- Consolidate duplicated live/offline processing where practical after regression coverage exists. Review state handoff between GUI and audio thread for races, partial updates, allocations, locks, and logging.
- Test unity, zero gain, known attenuation, hard pan, mute/solo combinations, effect bypass, multiple instruments, and save/reload restoration using deterministic signals. Compare live/offline output at a common internal tap with explicit numerical tolerances and aligned initial DSP state.

Done when: controls have consistent documented meaning, gain is applied exactly where intended, routing tests pass, and live/export differences are either eliminated or explicitly specified and tested.

## 4. Calibrate embedded synth gain staging

- Measure TF4, Dexed, V2, and OsTIrus with a small representative patch suite: quiet/loud patches, single notes, chords, high velocity, resonant patches, and effect tails. Use externally supplied firmware for OsTIrus validation where required.
- Record peak, RMS, DC offset, non-finite samples, and clipping before/after each major stage. Choose headroom and engine trim values from these measurements; preserve intended patch dynamics rather than normalize every patch to equal loudness.
- Define unity, fader range, pan law, meter tap points, and overload indication. Smooth interactive gains to avoid zipper noise.
- Decide the output saturation/limiting policy explicitly, including float export. Measure the existing unconditional `tanhf()` behavior rather than treating it as transparent.
- Check 16-bit/float output, multiple supported sample rates and block sizes, and dense mixed sample/synth arrangements.
- If corrected gain changes existing project playback, document the difference and choose an explicit compatibility/versioning policy before changing stored defaults.

Done when: measured levels and headroom meet agreed bounds, representative mixes do not clip unexpectedly, meters match their documented tap, and old-project behavior is accounted for.

## 5. Sanitize the repository and documentation

- Inventory tracked files and full history for credentials, private paths/data, firmware, archives, binaries, generated artifacts, logs, and oversized blobs. Inspect archive contents as well as filenames. Record findings without copying secrets into reports.
- Inventory dependency versions, local vendor patches, licenses/notices, presets, graphics, and other asset provenance. Resolve unclear redistribution status before including affected assets; do not infer a project-wide license from the upstream README.
- Prepare a publication tree with external runtime assets supplied by users and a tested, clear missing-asset path. Ensure builds and CI do not silently depend on ignored local files or firmware.
- Choose a sanitized history or a fresh publication snapshot once the inventory is known. Preserve required attribution; review any history rewrite or destructive removal separately before executing it.
- Remove obsolete first-party code, duplicate build instructions, debug remnants, and unused assets in small verified changes. Avoid wholesale vendor reformatting; retain reproducible dependency sources and required generated files or generators.
- Rewrite README around ft2-dxm features, maturity, supported builds, external assets, screenshots, known issues, and actual release locations. Update license inventory and add contribution/bug-report guidance with an audio reproduction template.

Done when: the exact tree and history proposed for upload pass the inventory, dependency/asset provenance is documented, and a clean checkout builds without workstation-only files.

## 6. Release validation and publication gate

- Add CI for clean Linux build/tests and sanitizer coverage; add supported target build jobs using existing wrappers. Distinguish cross-compilation from native runtime validation.
- Keep firmware-dependent tests opt-in and report skips visibly. Public CI must have meaningful coverage of the other engines and mixer.
- Run the final matrix once on the release candidate, including actual UI/input and audio-device smoke checks, DXM/DXI round trips, old-project playback, WAV export, and an extended playback/navigation session.
- Record results, supported/untested platforms, remaining non-blocking issues, and any compatibility changes in release notes.
- Review the publication candidate and destination before upload. Replacing a pre-sanitation remote branch requires an explicit force-with-lease publication step. Repoint or remove any tag that still reaches the excluded history; the existing `pre-alpha` tag currently follows the unsanitized branch and must move with the sanitized release candidate.

Release gate: no reproducible stuck notes or broken V2 navigation; mixer/gain contracts verified; no unexplained live/export mismatch; no unresolved release-blocking sanitizer failures; source/history audit complete; clean build and documentation validated. Cosmetic improvements can remain tracked issues.

## Suggested change sequence

1. Baseline fixtures, bug records, and publication inventory.
2. V2 page/input state fixes and regression tests.
3. Note ownership/release fixes and regression tests.
4. Mixer routing and live/offline consistency fixes.
5. Gain calibration, meter behavior, and compatibility documentation.
6. Source/asset cleanup, dependency notices, README, and CI.
7. Final candidate validation and publication review.
