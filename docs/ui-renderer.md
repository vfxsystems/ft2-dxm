# UI Renderer

The UI renderer now has an explicit compatibility layer in `src/ft2_ui_render.*`.
The default production path still writes to the classic 632x400 software
framebuffer so existing FT2 screens remain visually stable, but primitive draw
calls are routed through a renderer API that tracks logical-to-output scale,
renderer-output DPI scale, selectable backend state, and palette themes.

## Renderer Contract

- UI layout coordinates remain logical FT2 pixels unless a caller explicitly
  uses the floating-point vector helpers.
- `ft2_ui_render_set_metrics()` is updated from `ft2_video.c` whenever the
  SDL render rectangle changes. High-DPI scale is measured from
  `SDL_GetRendererOutputSize()` with `SDL_GL_GetDrawableSize()` kept as a
  fallback for older SDL backends.
- Classic primitives in `ft2_gui.c` (`clearRect`, `fillRect`, `hLine`, `vLine`,
  `line`, and `drawFramework`) now delegate to `ft2_ui_render`.
- The compatibility raster path clips internally. Public legacy wrappers keep
  their asserts to catch bad caller geometry during development.
- New scalable widgets should prefer `ft2_ui_render_fill_rect_f()`,
  `ft2_ui_render_stroke_rect_f()`, `ft2_ui_render_line_f()`, and
  `ft2_ui_render_arc_f()` so they can be migrated to a true SDL/vector backend
  without changing widget logic.
- Modulated rotary controls use `ft2_ui_render_arc_f()` for the outer feedback
  ring. The filled value arc runs from the rotary start angle to the current
  value; the modulation ring runs from the current value toward the clamped
  modulated target value.

## Build And Runtime Exposure

The vector/high-DPI renderer contract is enabled by default:

```sh
./build-linux.sh --fresh -- -DFT2_ENABLE_VECTOR_UI=ON
```

Disable the selectable vector backend contract for a strict legacy build:

```sh
./build-linux.sh --fresh -- -DFT2_ENABLE_VECTOR_UI=OFF
```

Runtime backend selection is intentionally non-persistent while the larger
vector UI migration is underway:

```sh
FT2_UI_BACKEND=software ./build-linux/bin/ft2-dxm
FT2_UI_BACKEND=vector ./build-linux/bin/ft2-dxm
```

The current `vector` backend is the high-DPI/vector-compatible contract used by
new widgets and tests. It keeps the compatibility framebuffer active until a
screen has been fully migrated away from legacy FT2 raster assumptions.

## Color Themes

The renderer exposes the same named themes as the GUI designer:

- `classic` / `FT2 Arctic`
- `op1` / `OP-1 Inspired`
- `renoise` / `Renoise Inspired`
- `vscode` / `VS Code Inspired`

Use `FT2_UI_THEME` for non-persistent runtime testing:

```sh
FT2_UI_THEME=renoise ./build-linux/bin/ft2-dxm
FT2_UI_BACKEND=vector FT2_UI_THEME=vscode ./build-linux/bin/ft2-dxm
```

The classic FT2 palette editor still owns persistent palette configuration.
These runtime themes are meant for validating the vector UI path and keeping the
engine and `ft2_gui_designer` palette contracts aligned before adding a broader
preferences UI.

## TTF Fonts

SDL_ttf support is optional and disabled by default to keep baseline builds
compatible with existing target toolchains.

Enable it with:

```sh
./build-linux.sh --fresh -- -DFT2_ENABLE_TTF=ON
```

When enabled, `ft2_ui_render_load_ttf_font()` loads a TTF file and
`ft2_ui_render_text_ttf()` renders blended UTF-8 glyphs into the software
framebuffer. When disabled, the API remains available but reports that TTF is
not available, so callers can safely fall back to the built-in bitmap fonts.

## Verification

The renderer self-test is part of `ft2-dxm --self-test` and validates clipped
fills, line drawing, framework raster output, scale metrics, backend selection,
and theme selection. Run it through the normal test entry points after changing
renderer code:

```sh
./build-linux.sh --fresh --test -j 4
./scripts/test-linux.sh --fresh --build-root /tmp/ft2-linux-tests -j 4
```
