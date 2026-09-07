#!/usr/bin/env python3
"""Export a source snapshot from tracked files and new first-party support files.

Never includes .git/history or local firmware. Does not modify the checkout.
Run without arguments to inspect the candidate; --output creates a new tar.gz.
"""
import argparse
import io
from pathlib import Path
import re
import subprocess
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
PRIVATE_NAMES = {"codex.md", "_old.codex.md", "gemini.md", "claude.md"}
EXCLUDED_PREFIXES = (
    "src/gearmulator/assets/",  # local ROMs, extracted presets and databases
    "src/release/", "src/vs2019_project/x64/", "gfxassets/",
)
SECRET = re.compile(rb"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----|"
                    rb"\bgh[pousr]_[A-Za-z0-9]{30,}\b|\bAKIA[A-Z0-9]{16}\b")


def excluded(name):
    path = Path(name)
    return (name.startswith(EXCLUDED_PREFIXES) or path.name.lower() in PRIVATE_NAMES
            or path.suffix.lower() in {".exe", ".dll", ".obj", ".o", ".pdb", ".log"}
            or any(part.lower() in {"roms", ".git", ".vs", ".vscode", ".codex", ".agents", ".claude"}
                   for part in path.parts))


def inspect_payload(name, data, depth=0):
    if SECRET.search(data):
        raise ValueError(f"possible credential in {name}; inspect locally before export")
    if name.lower().endswith(".zip"):
        if depth >= 4:
            raise ValueError(f"archive nesting too deep: {name}")
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            for entry in archive.infolist():
                lower = entry.filename.lower()
                if "rom.bin" in lower or "firmware" in lower or "roms/" in lower:
                    raise ValueError(f"firmware entry in {name}; exclude archive before export")
                if entry.file_size > 100 * 1024 * 1024:
                    raise ValueError(f"archive entry too large to audit: {name}")
                inspect_payload(entry.filename, archive.read(entry), depth + 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, help="new .tar.gz path (must not exist)")
    args = parser.parse_args()
    tracked = subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT).decode().split("\0")
    new = subprocess.check_output(["git", "ls-files", "--others", "--exclude-standard", "-z"], cwd=ROOT).decode().split("\0")
    names = sorted(set(filter(None, tracked + [p for p in new if p == "CONTRIBUTING.md" or p.startswith(("docs/", "tests/", "scripts/", ".github/"))])))
    candidate = []
    omitted = 0
    for name in names:
        path = ROOT / name
        if excluded(name) or not path.exists():
            omitted += 1
            continue
        if path.is_symlink() or not path.is_file():
            raise ValueError(f"review non-regular file before export: {name}")
        inspect_payload(name, path.read_bytes())
        candidate.append(name)
    if args.output:
        # Exclusive create also prevents accidentally replacing a previous candidate.
        with args.output.open("xb") as output, tarfile.open(fileobj=output, mode="w:gz", compresslevel=1) as archive:
            for name in candidate:
                archive.add(ROOT / name, arcname="ft2-dxm/" + name, recursive=False)
    print(f"Source candidate: {len(candidate)} files; {omitted} excluded/missing; no Git history.")
    print("Credential signatures and ZIP contents checked; manual asset/provenance review remains required.")


if __name__ == "__main__":
    main()
