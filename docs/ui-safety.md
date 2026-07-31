# UI Input Safety

Synth editor overlays for Tunefish4, V2, Dexed, and OsTIrus are full-screen FT2 surfaces. While one is visible, it owns mouse input for the whole screen, including empty space between widgets. This prevents click-through into the underlying instrument editor and keeps stale FT2 mouse-object state from acting on hidden controls.

Mouse-up handling must clear global button state before dispatching to synth editor callbacks. Close buttons and stale layout states can return early from callbacks, so button release state cannot be left behind for a later drag or release path.

If `ui.synthEditorShown` is set but no visible layout matches the active instrument, close through `ft2_close_synth_editor()` instead of only clearing the flag. That path hides all synth layouts and restores the normal top/bottom FT2 screens.

Run after mouse dispatch, synth editor, or overlay visibility changes:

```sh
./build-linux.sh --test -j 4
./scripts/test-target-builds.sh --build-root /tmp/ft2-dxm-targets -j 4
```
