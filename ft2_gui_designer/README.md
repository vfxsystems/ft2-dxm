# FT2 GUI Designer

FT2 GUI Designer is a layout editor that mirrors FastTracker 2 widget behavior and styling so you can author GUI screens for FT2 and export schema-based C modules. The tool targets the existing FT2 widget suite (pushbuttons, checkboxes, scrollbars, textboxes, Tunefish widgets, etc.) and keeps exports modular and compatible with classic layouts.

## Highlights

- FT2-style palette and widgets, including authentic bitmap fonts.
- Tunefish and Dexed widget sets (buttons, toggles, labels, rotaries, linears, combos, meters, parameter controls, envelopes, groups).
- Schema-based C export for FT2 (per-layout descriptor with ID ranges).
- Bitmap import using FT2 asset registry (BMP/PNG, indexed or 24/32-bit true-color).
- Bitmap layer and skin metadata for foreground art, backgrounds, and widget skin surfaces.
- Wider two-column toolbar with readable schema labels.
- Numeric page selector with minus/plus controls for layout pages 0 through 7 (`0` shows all pages).
- Designer palette themes: FT2 Arctic, OP-1 inspired, Renoise inspired, and VS Code inspired.
- Vector modulation-ring preview for TF rotary controls using the shared schema v9 `mod_ring` contract.
- Built-in file prompt for load/save/export without blocking the app.

## Build and Run

From the repository root:

```bash
./scripts/build-gui-designer.sh --test -j 4
./build-gui-designer/ft2_gui_designer
```

The compatibility Makefile delegates to that wrapper and keeps the same
out-of-source layout:

```bash
make -C ft2_gui_designer test
```

The equivalent direct CMake commands are:

```bash
cmake -S ft2_gui_designer -B build-gui-designer -DCMAKE_BUILD_TYPE=Release
cmake --build build-gui-designer --parallel 4
./build-gui-designer/ft2_gui_designer
```

Pass a design file to load it at startup, or validate one or more design files
without opening a window:

```bash
./build-gui-designer/ft2_gui_designer ft2_gui_designer/layouts/v2_complete_layout.gui
./build-gui-designer/ft2_gui_designer --validate \
  ft2_gui_designer/layouts/v2_complete_layout.gui \
  ft2_gui_designer/layouts/ostirus_complete_layout.gui
ctest --test-dir build-gui-designer --output-on-failure
```

CMake copies the tracked layouts beside the executable. Relative `layouts/...`
paths are resolved from the executable directory, so loading works regardless of
the directory used to launch the designer.

If SDL2/OpenGL development packages are missing, install them using your platform package manager and rebuild.

## Controls

Toolbar tools:

- 1: Select tool (select and drag widgets).
- 2: Pushbutton
- 3: Radiobutton
- 4: Checkbox
- 5: Horizontal scrollbar
- 6: Vertical scrollbar
- 7+: Additional tools (Tunefish widgets, framebox, bitmap, waveform, etc.).

Global keys:

- G: toggle grid
- P: toggle properties panel
- T: cycle designer color theme
- Delete: delete selected widget
- Esc: cancel drag or close prompt

File operations (open prompt in the canvas footer):

- Ctrl+O: load `.gui`
- Ctrl+S: save `.gui`
- Ctrl+E: export schema C (`.c` + `.h`)
- Ctrl+I: import bitmap (BMP/PNG)

## Canvas and Placement

- Canvas size is 632x400 with an 8px grid.
- Drag tools from the toolbar or select a tool and click to place.
- The toolbar page spinner controls preview filtering without changing widget coordinates. Page `0` shows all widgets; pages `1` through `7` show their assigned widgets.
- The theme dropdown changes the designer palette preview while preserving FT2 palette indexes used by export.
- TF rotary widgets export modulation-ring metadata. Matrix amount knobs are tagged as direct matrix-amount rings; other rotary widgets use automatic engine/name matching for live modulation feedback.
- Selected widgets show handles for visual feedback.
- Drag to reposition; grid snapping is applied.

## Properties Panel

Select a widget to edit:

- Position and size: X, Y, W, H
- Text: Caption and optional Caption2
- Name: C identifier used for schema bindings
- Page: Tunefish/Dexed page (0=Both, 1=Page 1, 2=Page 2)
- Font: FT2 font index
- Bitmap: asset ID for bitmap widgets
- Trans: palette transparency index for indexed bitmap export
- Layer: bitmap layer (`0` foreground widget, `1` background, `2` skin)
- Flags: bitmap flags bitmask (`1` click-through, `2` true-color; true-color is also inferred on export)
- Opacity: global bitmap opacity (`0` transparent, `255` opaque), multiplied by per-pixel alpha
- Skin: skin part (`0` none, `1` background, `2` panel, `3` button, `4` knob, `5` slider, `6` meter)
- Scrollbar nudge: toggle for FT2-style arrow buttons

Edits are inline and non-blocking; Enter commits, Esc cancels.

## File Formats

### `.gui` design files

Design files store the widget list and editor state. Current version is `FT2GUI_V8`.

Notes:
- Captions and names are quoted in current files.
- `FT2GUI_V2` through `FT2GUI_V7` remain loadable. Files before V8 use full bitmap opacity.

### Schema export

Ctrl+E exports a schema-based layout module:

```
<base>.h
<base>.c
```

The export includes:

- `ft2_ui_layout_desc_t` descriptor
- Per-widget arrays (only for widget types that exist)
- `*_BASE` and `*_COUNT` macros with `#ifndef` guards
- Bitmap descriptors with page, layer, flags, skin part, and opacity metadata

Recommended export targets:

- Tunefish: `../src/ft2_tunefish_complete_layout_schema`
- Dexed: `../src/dexed/dx_complete_layout_schema`
- V2: `../src/ft2_v2_complete_layout_schema`
- OsTIrus: `../src/ft2_ostirus_complete_layout_schema`

The exporter strips a trailing `_schema` from the base name so macro prefixes match existing layout code.

## Tunefish and Dexed Workflow

1. Load an existing layout:
   - `ft2_gui_designer/layouts/tf_complete_layout.gui`
   - `ft2_gui_designer/layouts/dx_complete_layout.gui`
   - `ft2_gui_designer/layouts/v2_complete_layout.gui`
   - `ft2_gui_designer/layouts/ostirus_complete_layout.gui`
2. Edit widgets on the canvas and in the properties panel.
3. Export schema to the corresponding `src/` path.
4. Rebuild FT2 to compile the updated schema.

## Bitmap Import

- Ctrl+I opens the bitmap import prompt.
- Accepted formats: indexed/RLE BMP, uncompressed 24/32-bit BMP, or PNG.
- True-color pixels use straight-alpha ARGB (`0xAARRGGBB`) in both the designer and runtime. PNG and 32-bit BMP preserve all eight alpha bits; 24-bit BMP imports as opaque.
- Imported bitmaps are registered in `ft2_ui_assets` and assigned numeric IDs.
- Use the Bitmap property to reference the ID for bitmap widgets.
- Set Layer to `1` for backgrounds or `2` for reusable skin surfaces. Background/skin bitmaps render behind other widgets in the designer preview and export as alpha-preserving 32-bit BMP V4 assets when true-color pixels are available.

Import status appears in the footer and full errors are printed to the console.

## Architecture Overview

- `main.c`: SDL event loop, UI, file prompts.
- `widgets.c/h`: widget creation, rendering, schema export, and `.gui` load/save.
- `font.c/h`: FT2 font rendering (uses the shared asset registry).
- `palette.c/h`: FT2 palette themes and renderer-aligned clipped/vector drawing helpers.
- `assets.c/h`: bitmap/font asset access via `ft2_ui_assets`.
- `shared/ft2_ui_schema.h`: schema descriptor definitions used by both FT2 and the designer.

## Known Limitations

- Combo box contents are not embedded in `.gui` (schema uses runtime list injection in FT2).
- The designer preview does not simulate full runtime state (values are preview defaults).

## License

Based on FastTracker 2 Clone by Olav Sorensen. This tool reuses FT2 assets and widget behaviors for layout design and schema export.
