# FT2 GUI Designer

FT2 GUI Designer is a layout editor that mirrors FastTracker 2 widget behavior and styling so you can author GUI screens for FT2 and export schema-based C modules. The tool targets the existing FT2 widget suite (pushbuttons, checkboxes, scrollbars, textboxes, Tunefish widgets, etc.) and keeps exports modular and compatible with classic layouts.

## Highlights

- FT2-style palette and widgets, including authentic bitmap fonts.
- Tunefish and Dexed widget sets (buttons, toggles, labels, rotaries, linears, combos, meters, parameter controls, envelopes, groups).
- Schema-based C export for FT2 (per-layout descriptor with ID ranges).
- Bitmap import using FT2 asset registry (BMP/PNG, indexed).
- Built-in file prompt for load/save/export without blocking the app.

## Build and Run

From the repository root:

```bash
make -C ft2_gui_designer
./ft2_gui_designer/ft2_gui_designer
```

If SDL2 is missing, install it using your platform package manager and rebuild.

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
- Scrollbar nudge: toggle for FT2-style arrow buttons

Edits are inline and non-blocking; Enter commits, Esc cancels.

## File Formats

### `.gui` design files

Design files store the widget list and editor state. Current version is `FT2GUI_V4`.

Notes:
- Captions are space-delimited in the current V4 text format. Use underscores in captions or edit captions in the properties panel after loading.
- Backward compatibility is not required for older versions in this project.

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

Recommended export targets:

- Tunefish: `../src/ft2_tunefish_complete_layout_schema`
- Dexed: `../src/dexed/dx_complete_layout_schema`

The exporter strips a trailing `_schema` from the base name so macro prefixes match existing layout code.

## Tunefish and Dexed Workflow

1. Load an existing layout:
   - `ft2_gui_designer/layouts/tf_complete_layout.gui`
   - `ft2_gui_designer/layouts/dx_complete_layout.gui`
2. Edit widgets on the canvas and in the properties panel.
3. Export schema to the corresponding `src/` path.
4. Rebuild FT2 to compile the updated schema.

## Bitmap Import

- Ctrl+I opens the bitmap import prompt.
- Accepted formats: 4-bit indexed BMP or PNG (with palette/alpha handling).
- Imported bitmaps are registered in `ft2_ui_assets` and assigned numeric IDs.
- Use the Bitmap property to reference the ID for bitmap widgets.

Import status appears in the footer and full errors are printed to the console.

## Architecture Overview

- `main.c`: SDL event loop, UI, file prompts.
- `widgets.c/h`: widget creation, rendering, schema export, and `.gui` load/save.
- `font.c/h`: FT2 font rendering (uses the shared asset registry).
- `assets.c/h`: bitmap/font asset access via `ft2_ui_assets`.
- `shared/ft2_ui_schema.h`: schema descriptor definitions used by both FT2 and the designer.

## Known Limitations

- `.gui` captions are whitespace-delimited (underscores recommended).
- Combo box contents are not embedded in `.gui` (schema uses runtime list injection in FT2).
- The designer preview does not simulate full runtime state (values are preview defaults).

## License

Based on FastTracker 2 Clone by Olav Sorensen. This tool reuses FT2 assets and widget behaviors for layout design and schema export.
