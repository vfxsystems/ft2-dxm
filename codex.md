# Codex Progress Notes (ft2-dxm)

## Scope / constraints
- Fix **Dexed + Mixer** GUI pipeline only.
- **Do not touch Tunefish**.
- **Do not change CMake** unless strictly necessary; user suspects CMake changes (CONFIGURE_DEPENDS) broke build.
- Pipeline: `.gui` -> schema export -> runtime rendering in FT2.

## Current symptoms
- Dexed/Mixer widgets missing after export.
- Using new bitmaps causes crash or blank output (when new bitmap background used).
- Not crashing right now but **new bitmaps** in FT2 can crash.

## Findings (from repo scan)

### GUI files
- `ft2_gui_designer/layouts/dx_complete_layout.gui` has **52 widgets**.
  - All **page = 1** (not 0).
  - Contains TF-style widgets (rotaries, combos, labels, group boxes, waveform view).
  - **No bitmap widgets** (type 7) in the file.

- `ft2_gui_designer/layouts/ft2_mixer_layout.gui` also has **no bitmap widgets**.

### Dexed schema export
- `src/dexed/dx_complete_layout_schema.c`
  - `DX_COMPLETE_LAYOUT_BMP_COUNT = 0`
  - All page values are `1`.
  - Rotary arc values are `0.000f .. 2.350f` (but runtime styling overrides).

### Runtime uses schema
- `src/dexed/dx_complete_layout.c` uses schema if `DX_USE_SCHEMA_LAYOUT` is defined.
  - Defined in `CMakeLists.txt` currently.
  - `dx_create_complete_layout_from_schema()` constructs widgets from schema.
  - `dx_apply_dexed_styling()` forces rotary arc to `-2.35..2.35`.

- `src/ft2_mixer_gui.c` renders bitmap widgets from schema and uses `ft2_ui_assets`.

### Asset registry / bitmap export
- `src/shared/ft2_ui_assets.{h,c}` only lists custom bitmaps for **Tunefish** and **Mixer image1**.
  - No Dexed custom bitmap entries currently.

- `ft2_gui_designer/widgets.c` exporter:
  - `export_custom_bitmaps()` writes BMP RLE4 C/H to `src/gfxdata`.
  - Updates `src/ft2_gfxdata.h` + `src/shared/ft2_ui_assets.{h,c}`.
  - Only exports bitmaps where `bmp->id >= FT2_UI_BITMAP_COUNT`.
  - Names are sequential `layout_image1`, `layout_image2`, etc, **based on sorted bitmap IDs** (potential mismatch if IDs are not contiguous).

- `ft2_gui_designer/assets.c` shows designer bitmap IDs start at `FT2_UI_BITMAP_COUNT` and increment.

### Potential mismatch/crash causes
- If GUI doesn’t include bitmap widgets -> schema has no bitmap entries -> runtime sees nothing.
- If bitmap IDs or enum mapping are mismatched (non-contiguous IDs), schema bitmap `asset_id` may not align with generated enum/registry -> decode / lookup errors.
- New bitmap crash could be bad ID mapping or missing enum/registry update.

## Open tasks / plan
1. **Verify Makefile build path** (not CMake) for Dexed: ensure `DX_USE_SCHEMA_LAYOUT` set in Make build if using Make.
2. **Validate pipeline**:
   - Ensure `.gui` files include bitmap widgets when backgrounds are used.
   - Ensure export writes `*_schema.c` with bitmap entries and nonzero `*_BMP_COUNT`.
   - Ensure `ft2_gfxdata.h` and `ft2_ui_assets.{h,c}` include new bitmaps.
3. **Fix bitmap ID mapping**:
   - Ensure custom bitmap enum/registry indices align with actual bitmap IDs in schema (avoid sequential renumber if IDs are sparse).
4. **Dexed**: all widgets on page 0 (single page), using TF rotary arcs (runtime style already sets arc).
5. **Mixer**: schema + assets consistent for new bitmaps.

## Important files
- `ft2_gui_designer/widgets.c` (schema + bitmap export logic)
- `ft2_gui_designer/assets.c` (bitmap IDs)
- `ft2_gui_designer/layouts/dx_complete_layout.gui`
- `ft2_gui_designer/layouts/ft2_mixer_layout.gui`
- `src/dexed/dx_complete_layout_schema.c`
- `src/ft2_mixer_layout_schema.c`
- `src/shared/ft2_ui_assets.{h,c}`
- `src/ft2_gfxdata.h`
- `src/dexed/dx_complete_layout.c`
- `src/ft2_mixer_gui.c`

## Notes
- Git is broken (corrupt objects). Avoid git commands.
- Normal writes to `/home/user/ft2-dxm` failed with `Permission denied`; use `sandbox_permissions=require_escalated` for file edits.

## Session Progress (2026-02-01)

### Dexed
- Fixed portamento/glide not engaging: added `initPortamento()` on new note in `src/dexed/dexed_audio.cpp` with `lastActiveVoice` tracking, and ensured mono transfer copies `porta_curpitch_` in `src/dexed/msfa/dx7note.cc`.
- Added UI + bindings for portamento time (glide knob) and mono toggle:
  - UI widgets in `src/dexed/dx_complete_layout.c` and schema `src/dexed/dx_complete_layout_schema.c/.h`.
  - Parameters wired in `src/ft2_dexed_wrapper.cpp` (IDs 1005/1006).
  - Accessors in `src/dexed/dexed_audio.h/.cpp`.
- LFO waveform writeback fix (from earlier in session): reset LFO when params 136–142 change in `src/dexed/dexed_audio.cpp`.

### Tunefish (TF4)
- Exit button made momentary on mouse-up (shows pressed state).
- Parameter readout text for Poly/Glide set to tiny text in `src/ft2_tunefish_widgets.c`.

### Mixer / DSP UI
- Added vertical scroll for inline DSP parameter list (keeps horizontal param sliders):
  - New scrollbar ID `SB_DSP_PARAM_SCROLL` in `src/ft2_scrollbars.h` + entry in `src/ft2_scrollbars.c`.
  - Scroll offset + mapping to visible rows in `src/ft2_mixer_gui.c`.
  - Nudge buttons (PB_RES_6/7) for scrolling; show only when needed.
- Fixed DSP inline overflow/segfault: param rows now scroll instead of drawing off-screen.
- Fixed bitmap overdraw:
  - DSP inline clear clipped to mixer frame width; fallback to x=478 when no framebox.
- Removed manual draw of mixer/DSP frameboxes in `drawMixerBox()` (frame boxes now solely from schema/bitmaps).
- Persist mixer DSP slots on close/open:
  - Cache on effect change, on exit, and on hide mixer.

### Warnings
- Fixed `END_BIDI` macro backslash warning in `src/mixer/ft2_mix_macros.h`.
- Added `eDeleteArray(ePtr&)` overload in `src/tunefish4/Source/runtime/runtime.hpp` to avoid deleting `void*`.

### Notes
- `ft2_ui_scrollbar_desc_t` has no `name` field; DSP param scroll uses the first scrollbar entry in mixer schema.
- If using the GUI designer, add a single scrollbar to `ft2_mixer_layout.gui` (it will be used automatically).
