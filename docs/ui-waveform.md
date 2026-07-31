# UI Waveform Refresh

The shared waveform view is embedded by the Tunefish4, V2, and OsTIrus editor pages.

The oscilloscope section should repaint on every main video-loop draw so the editor feels responsive at the host UI refresh rate. Audio monitor samples are cheap to copy and are already bounded by `AUDIO_OUTPUT_MONITOR_LEN`.

The spectrum analyzer is more expensive, so it should only recompute when the audio callback publishes a new monitor generation through `audioGetOutputMonitorGeneration()`. This keeps TF4/V2/OsTIrus waveform redraw smooth without doing redundant per-frame spectrum analysis.

Run after waveform, synth editor, or audio monitor changes:

```sh
./build-linux.sh --test -j 4
./scripts/test-target-builds.sh --build-root /tmp/ft2-dxm-targets -j 4
```
