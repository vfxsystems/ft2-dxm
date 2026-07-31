# Build Instructions

This project uses CMake for all supported targets. The helper scripts in the repository root and `scripts/` directory keep builds out of the source tree, refresh stale CMake caches safely, and provide a consistent command surface for Linux, Windows, macOS, and Raspberry Pi Linux ALSA targets.

Build outputs are written under each build directory's `bin/` subdirectory, for example `build-linux/bin/ft2-dxm`.

The FT2 GUI Designer has its own tracked CMake project in `ft2_gui_designer/`. Its output is written directly to the selected designer build directory.

## Quick Start

On a Linux host with the native dependencies installed:

```sh
./build-linux.sh --deps
./build-linux.sh --fresh --test -j 4
```

Build the GUI designer after schema/widget changes:

```sh
cmake -S ft2_gui_designer -B build-gui-designer -DCMAKE_BUILD_TYPE=Release
cmake --build build-gui-designer --parallel 4
```

Run the full available matrix:

```sh
./scripts/test-target-builds.sh --fresh --build-root /tmp/ft2-dxm-targets -j 4
```

The target sweep runs native Linux Release, Debug, and ASan/leak-detection tests. Windows, macOS, and Raspberry Pi cross-builds are attempted when their toolchains are installed. Missing cross-toolchains are reported as skips unless `--strict` is used.

## Native Linux

Install dependencies:

```sh
# Debian/Ubuntu
sudo apt install build-essential cmake libsdl2-dev libasound2-dev

# Fedora
sudo dnf install cmake gcc gcc-c++ SDL2-devel alsa-lib-devel

# Arch
sudo pacman -S base-devel cmake sdl2 alsa-lib
```

Optional SDL_ttf development packages are only required when building with
`-DFT2_ENABLE_TTF=ON`:

```sh
# Debian/Ubuntu
sudo apt install libsdl2-ttf-dev

# Fedora
sudo dnf install SDL2_ttf-devel

# Arch
sudo pacman -S sdl2_ttf
```

Build Release:

```sh
./build-linux.sh
```

Build Debug:

```sh
./build-linux.sh --debug --build-dir build-linux-debug
```

Build and run CTest:

```sh
./build-linux.sh --fresh --test -j 4
```

CTest currently covers:

- `ft2_version`: command-line startup/version smoke test.
- `ft2_self_test`: core table setup/teardown self-test.
- `ft2_v2_stress`: V2 preset, patch, MIDI, render, panic, and state serialization stress test.

Disk Op module saving defaults to DXM. XM files remain supported for loading and
import, but XM export is intentionally not exposed; use the WAV renderer path for
audio renders.

Pass extra CMake arguments after `--`:

```sh
./build-linux.sh --fresh -- -DWITH_DEXED=ON
./build-linux.sh --fresh -- -DFT2_ENABLE_TTF=ON
```

Useful environment overrides:

```sh
FT2_BUILD_TYPE=Debug ./build-linux.sh
FT2_LINUX_BUILD_DIR=/tmp/ft2-linux ./build-linux.sh
FT2_CMAKE_GENERATOR=Ninja ./build-linux.sh --fresh
FT2_JOBS=8 ./build-linux.sh --test
```

The Linux script detects stale `CMakeCache.txt` files that were generated for a different checkout path and refreshes the build directory automatically when it is safe to do so.

## Linux Test Matrix

Run native Release, Debug, and ASan/leak detection:

```sh
./scripts/test-linux.sh --fresh -j 4
```

Run only one phase:

```sh
./scripts/test-linux.sh --release-only --fresh
./scripts/test-linux.sh --debug-only --fresh
./scripts/test-linux.sh --asan-only --fresh
```

Put generated build directories outside the repository:

```sh
./scripts/test-linux.sh --fresh --build-root /tmp/ft2-linux-tests -j 4
```

ASan defaults to `detect_leaks=1:halt_on_error=1:abort_on_error=1`. LeakSanitizer may fail under ptrace-restricted sandboxes or debuggers; rerun the command directly in a normal shell if leak checks fail before the tests start. To keep address-sanitizer coverage in such an environment, rerun the ASan test directory with leak detection disabled:

```sh
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/ft2-linux-tests/build-linux-asan --output-on-failure
```

## UI Renderer And Fonts

The high-DPI/vector renderer compatibility layer lives in `src/ft2_ui_render.*`.
Classic `ft2_gui.c` primitives delegate to it while preserving the 632x400
software framebuffer output. SDL_ttf-backed scalable text is available behind
`-DFT2_ENABLE_TTF=ON`; without that flag, the API compiles as a safe no-op and
the application keeps using the built-in bitmap fonts.

The renderer layer is always built into `ft2-dxm`; there is currently no
separate `FT2_ENABLE_VECTOR_UI` build switch. The build-time option only gates
SDL_ttf-backed scalable text support.

See [UI renderer](ui-renderer.md) for the migration contract and renderer
self-test coverage.

## GUI Designer

The designer mirrors the shared schema and asset registry, so rebuild it whenever `src/shared/ft2_ui_schema.h`, `src/shared/ft2_ui_assets.*`, or designer source files change:

```sh
cmake -S ft2_gui_designer -B build-gui-designer -DCMAKE_BUILD_TYPE=Release
cmake --build build-gui-designer --parallel 4
```

Designer UI changes should also be checked with the same command after editing
`ft2_gui_designer/main.c`, `palette.*`, `canvas.*`, or `widgets.*`. The designer
preview uses clipped/vector-aligned draw helpers in `palette.*` so it stays in
step with the engine renderer contract in `src/ft2_ui_render.*`.

Legacy Makefile builds are still available:

```sh
make -C ft2_gui_designer
```

Bitmap and skin workflow:

- Import BMP/PNG assets with `Ctrl+I`.
- Use bitmap Layer `0` for foreground/widget art, `1` for backgrounds, and `2` for skin surfaces.
- Imported 24/32-bit BMP and PNG assets retain true-color pixels and export as `FT2_UI_BMP_FMT_RGB` assets when available.
- Runtime schema constructors for Tunefish, Dexed, and OsTIrus load RLE4 palette assets and true-color BMP assets through the same non-interactive bitmap widget path.

## Windows

The supported Windows flow is MinGW-w64 via CMake:

```sh
./scripts/build-windows.sh --deps
```

Install a MinGW-w64 C/C++ toolchain and provide a MinGW-compatible SDL2 package. On Linux hosts, common packages are:

```sh
sudo apt install mingw-w64 cmake wine
```

Build:

```sh
SDL2_DIR=/path/to/mingw-sdl2/lib/cmake/SDL2 \
  ./scripts/build-windows.sh --fresh -j 4
```

If the SDL2 package is discoverable through a prefix:

```sh
CMAKE_PREFIX_PATH=/path/to/mingw-prefix \
  ./scripts/build-windows.sh --fresh
```

Use a non-default compiler prefix:

```sh
MINGW_PREFIX=x86_64-w64-mingw32 ./scripts/build-windows.sh
```

Run CTest for the Windows binary:

```sh
./scripts/build-windows.sh --test
```

On non-Windows hosts, `--test` requires `wine`. The output binary is `build-windows-mingw64/bin/ft2-dxm.exe` unless `--build-dir` or `FT2_WINDOWS_BUILD_DIR` is used.

## macOS

The current macOS wrapper is:

```sh
./scripts/build-macos.sh
```

The legacy `./make-macos.sh` command remains as a compatibility shim and forwards to `scripts/build-macos.sh`.

Native macOS requirements:

- Xcode Command Line Tools
- CMake
- SDL2 framework or CMake package

Native build:

```sh
./scripts/build-macos.sh --fresh -j 4
```

Universal x86_64 + arm64 build on native macOS:

```sh
./scripts/build-macos.sh --fresh --universal
```

Run tests on native macOS:

```sh
./scripts/build-macos.sh --test
```

Linux-hosted macOS cross-builds require osxcross, a legally obtained macOS SDK, and macOS SDL2 dependency roots:

```sh
OSXCROSS_ROOT=/opt/osxcross \
SDL2_DIR=/path/to/macos/SDL2.framework/Resources/CMake \
  ./scripts/build-macos.sh --fresh
```

CTest can only run on native macOS.

## Raspberry Pi Linux ALSA

The Raspberry Pi target script supports native Pi builds and Linux-hosted armhf cross-builds.

Native Raspberry Pi dependencies:

```sh
sudo apt install build-essential cmake libsdl2-dev libasound2-dev
```

Native Raspberry Pi build and test:

```sh
./scripts/build-raspi-alsa.sh --native --fresh --test -j 4
```

Linux-hosted armhf cross-build dependencies:

```sh
sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf cmake
```

Cross-build with a Raspberry Pi sysroot containing SDL2 and ALSA development files:

```sh
RASPI_SYSROOT=/opt/raspi-sysroot \
SDL2_DIR=/opt/raspi-sysroot/usr/lib/arm-linux-gnueabihf/cmake/SDL2 \
  ./scripts/build-raspi-alsa.sh --fresh -j 4
```

Use a custom cross compiler prefix:

```sh
RASPI_TOOLCHAIN_PREFIX=arm-linux-gnueabihf ./scripts/build-raspi-alsa.sh
```

Cross-build tests require running the binary on the target device or in an emulator, so `--test` is only accepted with `--native`.

## Full Target Sweep

Run all available build phases:

```sh
./scripts/test-target-builds.sh --fresh --build-root /tmp/ft2-dxm-targets -j 4
```

Treat missing cross-toolchains as failures:

```sh
./scripts/test-target-builds.sh --strict --fresh -j 4
```

Skip selected targets:

```sh
./scripts/test-target-builds.sh --no-macos --no-windows
```

## Direct CMake

The scripts are preferred, but direct CMake still works:

```sh
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux --target ft2-dxm --parallel 4
ctest --test-dir build-linux --output-on-failure
```

Cross-builds use the checked-in toolchain files:

```sh
cmake -S . -B build-windows-mingw64 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake

cmake -S . -B build-raspi-armhf \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-raspi-armhf.cmake

cmake -S . -B build-macos \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-macos-osxcross.cmake
```

## Build Artifacts

Generated build directories and outputs should stay untracked. The repository ignores common CMake and binary artifacts, including:

- `build/`
- `build-*`
- `cmake-build-*`
- `CMakeFiles/`
- `CMakeCache.txt`
- `cmake_install.cmake`
- `Testing/`
- `release/other/ft2-dxm`

Use `--fresh` when changing toolchains, moving the checkout, or switching between incompatible CMake configurations.

## Troubleshooting

`CMakeCache.txt was created in a different directory`:

Run with `--fresh`, or let `./build-linux.sh` refresh a stale in-repository build directory.

`SDL2Config.cmake` or SDL2 headers are not found:

Install the platform SDL2 development package or set `SDL2_DIR`/`CMAKE_PREFIX_PATH` to the target SDL2 package.

Raspberry Pi cross-build finds the compiler but not SDL2/ALSA:

Set `RASPI_SYSROOT` to a Pi sysroot that contains target headers and libraries, then point `SDL2_DIR` into that sysroot if CMake cannot discover it automatically.

Windows cross-build configures but tests do not run:

Install `wine`, or run the generated executable and CTest on a Windows/MSYS2 host.

macOS cross-build configures but tests do not run:

This is expected. macOS CTest is only supported on native macOS.
