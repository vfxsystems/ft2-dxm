# Source publication

The public branch and source exporter exclude local firmware, the `gfxassets/`
workspace, and assistant transcripts. Keep those paths untracked: ignore rules
prevent new additions but do not remove already tracked files or historical blobs.

Create a source snapshot without changing or deleting local ROMs:

```sh
python3 scripts/export-source.py
python3 scripts/export-source.py --output /tmp/ft2-dxm-source.tar.gz
```

The exporter uses current tracked file contents plus new files under `docs/`,
`tests/`, `scripts/`, and `.github/`. It excludes local Gearmulator runtime assets
(including the ROM-containing ZIP), assistant transcripts, development metadata,
build binaries, and the entire local `gfxassets/` workspace. It checks selected credential
signatures and ZIP contents and refuses to overwrite an existing archive.
It never includes `.git` history.

Inspect and build the extracted snapshot, retain component attribution, then
initialize a new repository in that snapshot for publication. The current
checkout remains the development copy. Upload/destination selection is separate
from preparing the candidate.

Normal CMake builds have `FT2_EMBED_OSTIRUS_ASSETS=OFF`. Private local builds can
explicitly enable it, but their binaries/copied archives must not be public
release artifacts. Firmware loading via `FT2_OSTIRUS_ROM` remains available
without embedding. An empty value disables ROM discovery for automated tests.
An invalid explicit path does not fall back to another ROM.

The signature scan is not a complete secret audit. Review asset/preset provenance
and component notices before publication. The exporter retains vendored source
rather than attempting to prune transitive build dependencies.
