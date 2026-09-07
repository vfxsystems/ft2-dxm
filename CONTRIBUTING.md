# Contributing

Keep fixes focused and include a regression for reproducible playback/input
bugs. Run `./build-linux.sh --test` and applicable phases of
`./scripts/test-linux.sh`. Keep vendor modifications separate from integration
changes and retain component notices.

For audio/input bug reports, include the commit, OS, build type, engine/patch,
sample rate, buffer size, input source (keyboard/MIDI/tracker), exact steps, and
expected/actual behavior. Attach a minimal module only when it can be shared.
Do not attach Virus firmware, private data, or credentials.

For gain changes, include before/after measurements and the effect on existing
modules. For persistence changes, verify DXM/DXI round trips.
