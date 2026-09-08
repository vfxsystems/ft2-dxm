# ft2-dxm

A work-in-progress Fasttracker II derivative for Windows, macOS, and Linux, combining tracker samples with embedded Tunefish4, Dexed, V2, and OsTIrus synth engines.

The project builds on the FT2 clone and retains its tracker workflow. Extensions include DXM modules with synth state, DXI instrument patches, synth editors, macro mapping, stereo mixer strips, DSP effects, and WAV/render-to-sample workflows.

## Build and test

On Linux, install CMake, a C/C++17 toolchain, SDL2 development files, and ALSA development files, then run:

```sh
./build-linux.sh --test -j 4
./build-linux.sh --with-designer --test -j 4
./scripts/test-linux.sh -j 4
```

The executables are `build-linux/bin/ft2-dxm` and, when requested,
`build-linux/bin/ft2_gui_designer`. The designer also has a standalone wrapper:

```sh
./scripts/build-gui-designer.sh --fresh --test -j 4
```

See [build instructions](docs/build.md) for dependencies,
Windows/macOS/Raspberry Pi wrappers, optional UI features, and tests.
Cross-compilation alone does not verify native audio-device behavior.

## External Virus firmware

Virus firmware is not part of the source publication. OsTIrus requires a compatible ROM supplied separately by the user. This project does not provide permission to use or redistribute firmware.

Place the file in an `OsTIrus` directory beside the executable:

```text
build-linux/bin/OsTIrus/rom.bin
```

The same layout applies beside the Windows, macOS, and Raspberry Pi executable. An explicit path can be used instead:

```sh
FT2_OSTIRUS_ROM=/path/to/your/rom.bin ./build-linux/bin/ft2-dxm
```

An invalid explicit path leaves OsTIrus unavailable; the other engines remain usable. Existing local development search paths remain supported. Build scripts report whether the runtime folder or environment path is ready, but never copy firmware. Normal builds do not embed local ROM archives. Public tests run without Virus firmware.

## Documentation and status

- [Documentation index](docs/README.md)
- [DXM modules](docs/dxm.md) and [DXI patches](docs/dxi.md)
- [Mixer behavior and compatibility](docs/mixer-contract.md)
- [Stabilization results and remaining work](docs/stabilization-results.md)
- [Source publication procedure](docs/publication.md)

This is a development project. Release availability and platform verification are recorded for this derivative; upstream FT2 downloads are not ft2-dxm releases.

## Attribution and licenses

Original FT2/FT2-clone attribution is retained in the source notices. This repository contains components under several licenses, including BSD, GPL, Apache, and asset-specific terms. See [LICENSES.txt](LICENSES.txt) and the component notices; the original FT2 license does not describe every bundled component.
