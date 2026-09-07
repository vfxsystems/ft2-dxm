import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class BuildScriptTests(unittest.TestCase):
    def test_linux_custom_build_directory_reaches_cmake(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fake_bin = root / "bin"
            fake_bin.mkdir()
            log = root / "cmake.log"
            cmake = fake_bin / "cmake"
            cmake.write_text(
                "#!/usr/bin/env bash\n"
                "printf 'CALL\\n' >> \"$FT2_TEST_CMAKE_LOG\"\n"
                "printf '%s\\n' \"$@\" >> \"$FT2_TEST_CMAKE_LOG\"\n",
                encoding="utf-8",
            )
            cmake.chmod(0o755)
            build_dir = root / "custom-build"
            environment = os.environ.copy()
            environment["PATH"] = f"{fake_bin}:{environment['PATH']}"
            environment["FT2_TEST_CMAKE_LOG"] = str(log)
            result = subprocess.run(
                [str(ROOT / "build-linux.sh"), "--build-dir", str(build_dir), "-j", "1"],
                cwd=ROOT,
                env=environment,
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            calls = log.read_text(encoding="utf-8").split("CALL\n")[1:]
            self.assertEqual(len(calls), 2)
            self.assertIn(f"-B\n{build_dir}\n", calls[0])
            self.assertIn(f"--build\n{build_dir}\n", calls[1])
            self.assertIn(f"Done. Binary: {build_dir}/bin/ft2-dxm", result.stdout)


if __name__ == "__main__":
    unittest.main()
