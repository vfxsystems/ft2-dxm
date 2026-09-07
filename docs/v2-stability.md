# V2 Stability Notes

## Crash Class

The V2 engine stores a direct pointer to the active patch-bank map during `synthInit()`. If wrapper-owned patch-bank storage is replaced, moved, or recreated without reinitializing the synth, the audio render path can dereference stale patch memory. The observed failure signature is an invalid read inside `V2Synth::render()` on the SDL audio thread, often leaving audio in a stuck-note state after the UI changes patches.

The wrapper must therefore treat V2 patch-bank storage as render-critical state:

- serialize public V2 UI/audio calls through the wrapper lock;
- silence active voices before preset, patch, or persistent-state replacement;
- sanitize loaded patch/mod-matrix bytes before exposing them to the synth;
- reinitialize the V2 synth whenever patch-bank storage is replaced or restored.

## ENV/LFO page renderer

The ENV/LFO page draws its envelope segments with an integer Bresenham loop. Both
axis decisions must use the same error value from the start of an iteration.
Recomputing the second decision after changing the accumulator can step past the
target on steep segments and loop forever. The factory `BR_French HornZ` patch
exposed this as an apparent application crash when opening ENV/LFO.

The line renderer snapshots the doubled error before either axis update. Custom
envelope pixels also follow the shared renderer's null-framebuffer behavior.

## Regression Test

Run the V2 stress test after changing the V2 wrapper, V2 UI editor, patch load/save flow, or instrument-state persistence:

```sh
./build-linux.sh --test -j 4
```

For sanitizer coverage:

```sh
./scripts/test-target-builds.sh --build-root /tmp/ft2-dxm-targets -j 4
```

`ft2_stability` renders every V2 page into a test framebuffer with the French
Horn fixture, covering the steep ENV/LFO segments as well as page visibility.
`ft2_v2_stress` rapidly changes presets while notes are active, exports/reloads
patches, stresses invalid mod-matrix requests, serializes/deserializes V2 state,
renders audio, and verifies panic clears active voices. The ASan/leak-detection
phase should run outside ptrace/sandbox supervision when LeakSanitizer reports
that it cannot run under ptrace.
