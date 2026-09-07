import importlib.util
import io
from pathlib import Path
import unittest
import zipfile

spec = importlib.util.spec_from_file_location("export_source", Path(__file__).parents[1] / "scripts/export-source.py")
exporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exporter)


class SourceExportTests(unittest.TestCase):
    def test_firmware_and_transcripts_excluded_but_sources_retained(self):
        for name in ("src/gearmulator/assets/otstirusdat.zip", "OsTIrus/roms/rom.bin",
                     "src/dexed/_old.Codex.md", "src/gearmulator/CLAUDE.md",
                     "gfxassets/logo1.bmp", "gfxassets/working/concept.png"):
            self.assertTrue(exporter.excluded(name), name)
        for name in ("src/ft2_ostirus_wrapper.cpp", "src/v2/LICENSE.txt", "tests/stability_tests.c"):
            self.assertFalse(exporter.excluded(name), name)

    def test_nested_firmware_archive_is_rejected(self):
        inner = io.BytesIO()
        with zipfile.ZipFile(inner, "w") as archive:
            archive.writestr("rom.bin", b"test fixture, not firmware")
        outer = io.BytesIO()
        with zipfile.ZipFile(outer, "w") as archive:
            archive.writestr("nested.zip", inner.getvalue())
        with self.assertRaises(ValueError):
            exporter.inspect_payload("outer.zip", outer.getvalue())

    def test_private_key_signature_is_rejected(self):
        marker = b"-----BEGIN " + b"OPENSSH PRIVATE KEY-----"
        with self.assertRaises(ValueError):
            exporter.inspect_payload("unexpected.txt", marker)


if __name__ == "__main__":
    unittest.main()
