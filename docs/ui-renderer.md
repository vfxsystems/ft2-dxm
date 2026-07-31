# UI Renderer

The UI renderer now has an explicit compatibility layer in `src/ft2_ui_render.*`.
The current production backend still writes to the classic 632x400 software
framebuffer so existing FT2 screens remain visually stable, but primitive draw
calls are routed through a renderer API that tracks logical-to-output scale and
DPI scale.

## Renderer Contract

- UI layout coordinates remain logical FT2 pixels unless a caller explicitly
  uses the floating-point vector helpers.
- `ft2_ui_render_set_metrics()` is updated from `ft2_video.c` whenever the
  SDL render rectangle changes.
- Classic primitives in `ft2_gui.c` (`clearRect`, `fillRect`, `hLine`, `vLine`,
  `line`, and `drawFramework`) now delegate to `ft2_ui_render`.
- The compatibility raster path clips internally. Public legacy wrappers keep
  their asserts to catch bad caller geometry during development.
- New scalable widgets should prefer `ft2_ui_render_fill_rect_f()`,
  `ft2_ui_render_stroke_rect_f()`, and `ft2_ui_render_line_f()` so they can be
  migrated to a true SDL/vector backend without changing widget logic.

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
fills, line drawing, and framework raster output. Run it through the normal
test entry points after changing renderer code:

```sh
./build-linux.sh --fresh --test -j 4
./scripts/test-linux.sh --fresh --build-root /tmp/ft2-linux-tests -j 4
```
