# Macro Map

The Macro Map editor binds tracker macro slots `M0` through `MF` to per-instrument synth parameters or mixer DSP parameters.

## Synth Targets

Synth macro targets use normalized values from `00..FF` to `0.0..1.0` after the selected curve is applied. The active instrument flag must match the target before playback applies the macro:

- `TF4`: Tunefish4 parameters
- `DX`: Dexed parameters
- `V2`: V2 parameters
- `OTI`: OsTIrus parameters

The editor, DXM loader/saver, and replayer all use the shared helpers in `ft2_macro_map.c` to resolve target IDs, parameter counts, parameter labels, and stale mapping sanitation.

## Modulation Ring Feedback

Schema version 9 adds `mod_ring` metadata to rotary slider descriptors. The
current TF layout uses this contract to show PhasePlant/Serum/Vital-style outer
modulation arcs on knob widgets:

- matrix destination slots are mapped back to their target knob by engine
  destination ID/name;
- matrix amount is scaled from the live normalized `0..127` shadow value;
- amount knobs also show their own matrix amount as a direct outer ring;
- the draw path uses the shared vector arc primitive, matching the designer
  preview path.

DX, V2, and OsTIrus schema importers preserve the same rotary `mod_ring`
metadata. Their live matrix/automation providers can feed the existing
`TunefishWidget.modValue` path as those engine-side modulation surfaces are
completed.

## DSP Targets

DSP targets keep using the packed parameter ID:

```text
(scope << 12) | (slot << 8) | param
```

`scope` is `PAIR` or `MSTR`. Pair-scoped mappings apply to the stereo pair that owns the playing channel; master-scoped mappings apply to the master effect chain.

## Tracker Flow

- `Zx` selects macro slot `x`.
- `Yyy` writes an 8-bit value to the selected slot.
- `Nyy` writes a raw 8-bit value with curve processing bypassed.
- `Uyy` writes an 8-bit value with smoothing.
- Legacy `Mxy` writes macro slot `x` from a 4-bit value `y`, expanded to `00..FF`.

## Validation

DXM load, DXM save, and editor commit sanitize every slot. Invalid target IDs are disabled; invalid curve IDs reset to linear; stale synth parameter IDs clamp back to parameter `0`; DSP slot/scope/parameter fields are bounded before use.

Run the regression coverage with:

```sh
./build-linux.sh --test -j 4
./scripts/test-target-builds.sh --build-root /tmp/ft2-dxm-targets -j 4
```
