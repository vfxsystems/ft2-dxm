# DXM Module Workflow

DXM is the project module save format. Disk Op. module save starts with
`untitled.dxm`, loaded module names are normalized back to `.dxm` unless the WAV
renderer mode is selected, and XM files are load/import-only.

## Save

Disk Op. module save writes `.dxm` through `saveDXM()`. The DXM file contains an
XM-compatible core followed by typed chunks for mixer/DSP state, sample payloads,
TF4, Dexed, V2, OsTIrus, macro maps, macro pattern data, and instrument metadata.

The sample chunk stores its own mono/stereo and 8/16-bit payload flags. Stereo is
emitted only when both channel buffers exist, so the payload length matches the
chunk header.

## Load

DXM load first imports the embedded XM core into temporary module state, then
applies DXM chunks. Chunk readers must leave the file cursor on the declared
chunk boundary so later synth-state chunks cannot be lost after malformed or
unsupported payloads.

After temporary instruments are promoted to the live module, the loader
initializes the unified synth layer and restores per-instrument patch/state for
TF4, Dexed, V2, and OsTIrus.
