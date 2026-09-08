import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
BUILD_WRAPPERS = (
    "build-linux.sh",
    "scripts/build-gui-designer.sh",
    "scripts/build-windows.sh",
    "scripts/build-macos.sh",
    "scripts/build-raspi-alsa.sh",
)


def make_logging_tool(path: Path, log_variable: str) -> None:
    path.write_text(
        "#!/usr/bin/env bash\n"
        f"printf 'CALL\\n' >> \"${{{log_variable}}}\"\n"
        f"printf '%s\\n' \"$@\" >> \"${{{log_variable}}}\"\n",
        encoding="utf-8",
    )
    path.chmod(0o755)


class BuildScriptTests(unittest.TestCase):
    def test_build_shell_scripts_parse(self):
        scripts = [
            ROOT / "build-linux.sh",
            ROOT / "make-macos.sh",
            *(ROOT / "scripts").glob("*.sh"),
            ROOT / "ft2_gui_designer" / "test_compile.sh",
        ]
        for script in scripts:
            with self.subTest(script=script.relative_to(ROOT)):
                result = subprocess.run(
                    ["bash", "-n", str(script)],
                    cwd=ROOT,
                    text=True,
                    capture_output=True,
                    check=False,
                )
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_build_wrappers_expose_common_controls(self):
        expected = (
            "--debug",
            "--release",
            "--build-dir",
            "--fresh",
            "--clean-first",
            "--verbose",
            "--test",
            "--install",
            "--deps",
        )
        for relative_path in BUILD_WRAPPERS:
            with self.subTest(script=relative_path):
                result = subprocess.run(
                    [str(ROOT / relative_path), "--help"],
                    cwd=ROOT,
                    text=True,
                    capture_output=True,
                    check=False,
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                for option in expected:
                    self.assertIn(option, result.stdout)

        for relative_path in (
            "build-linux.sh",
            "scripts/build-windows.sh",
            "scripts/build-macos.sh",
            "scripts/build-raspi-alsa.sh",
        ):
            with self.subTest(script=relative_path, option="--with-designer"):
                result = subprocess.run(
                    [str(ROOT / relative_path), "--help"],
                    cwd=ROOT,
                    text=True,
                    capture_output=True,
                    check=False,
                )
                self.assertIn("--with-designer", result.stdout)

    def test_linux_custom_build_directory_reaches_cmake(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fake_bin = root / "bin"
            fake_bin.mkdir()
            log = root / "cmake.log"
            make_logging_tool(fake_bin / "cmake", "FT2_TEST_CMAKE_LOG")
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

    def test_linux_combined_build_requests_both_products_and_tests(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fake_bin = root / "bin"
            fake_bin.mkdir()
            cmake_log = root / "cmake.log"
            ctest_log = root / "ctest.log"
            make_logging_tool(fake_bin / "cmake", "FT2_TEST_CMAKE_LOG")
            make_logging_tool(fake_bin / "ctest", "FT2_TEST_CTEST_LOG")
            build_dir = root / "combined"
            environment = os.environ.copy()
            environment["PATH"] = f"{fake_bin}:{environment['PATH']}"
            environment["FT2_TEST_CMAKE_LOG"] = str(cmake_log)
            environment["FT2_TEST_CTEST_LOG"] = str(ctest_log)

            result = subprocess.run(
                [
                    str(ROOT / "build-linux.sh"),
                    "--build-dir",
                    str(build_dir),
                    "--with-designer",
                    "--test",
                    "-j",
                    "3",
                ],
                cwd=ROOT,
                env=environment,
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            calls = cmake_log.read_text(encoding="utf-8").split("CALL\n")[1:]
            self.assertEqual(len(calls), 2)
            self.assertIn("-DFT2_BUILD_GUI_DESIGNER=1\n", calls[0])
            self.assertIn(
                "--target\nft2-dxm\nft2_gui_designer\nft2_gui_bitmap_tests\n",
                calls[1],
            )
            self.assertIn("--parallel\n3\n", calls[1])
            self.assertIn(f"--test-dir\n{build_dir}\n", ctest_log.read_text(encoding="utf-8"))

    def test_gui_designer_wrapper_keeps_build_out_of_source(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fake_bin = root / "bin"
            fake_bin.mkdir()
            cmake_log = root / "cmake.log"
            ctest_log = root / "ctest.log"
            make_logging_tool(fake_bin / "cmake", "FT2_TEST_CMAKE_LOG")
            make_logging_tool(fake_bin / "ctest", "FT2_TEST_CTEST_LOG")
            build_dir = root / "gui"
            environment = os.environ.copy()
            environment["PATH"] = f"{fake_bin}:{environment['PATH']}"
            environment["FT2_TEST_CMAKE_LOG"] = str(cmake_log)
            environment["FT2_TEST_CTEST_LOG"] = str(ctest_log)

            result = subprocess.run(
                [
                    str(ROOT / "scripts/build-gui-designer.sh"),
                    "--debug",
                    "--build-dir",
                    str(build_dir),
                    "--clean-first",
                    "--verbose",
                    "--test",
                    "--install",
                    "-j",
                    "2",
                ],
                cwd=ROOT,
                env=environment,
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            calls = cmake_log.read_text(encoding="utf-8").split("CALL\n")[1:]
            self.assertEqual(len(calls), 3)
            self.assertIn(f"-S\n{ROOT / 'ft2_gui_designer'}\n", calls[0])
            self.assertIn(f"-B\n{build_dir}\n", calls[0])
            self.assertIn("-DCMAKE_BUILD_TYPE=Debug\n", calls[0])
            self.assertIn(
                "--target\nft2_gui_designer\nft2_gui_bitmap_tests\n",
                calls[1],
            )
            self.assertIn("--clean-first\n--verbose\n", calls[1])
            self.assertIn(f"--install\n{build_dir}\n", calls[2])
            self.assertIn(f"--test-dir\n{build_dir}\n", ctest_log.read_text(encoding="utf-8"))

    def test_cross_wrappers_configure_combined_target_builds(self):
        cases = (
            (
                "scripts/build-windows.sh",
                {"MINGW_PREFIX": "fake-cross"},
                ("fake-cross-gcc", "fake-cross-g++"),
                ROOT / "cmake" / "toolchain-mingw64.cmake",
            ),
            (
                "scripts/build-macos.sh",
                {"OSXCROSS_ROOT": "/tmp/fake-osxcross"},
                (),
                ROOT / "cmake" / "toolchain-macos-osxcross.cmake",
            ),
            (
                "scripts/build-raspi-alsa.sh",
                {"RASPI_TOOLCHAIN_PREFIX": "fake-cross"},
                ("fake-cross-gcc", "fake-cross-g++"),
                ROOT / "cmake" / "toolchain-raspi-armhf.cmake",
            ),
        )

        for relative_path, overrides, compiler_names, toolchain in cases:
            with self.subTest(script=relative_path), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                fake_bin = root / "bin"
                fake_bin.mkdir()
                cmake_log = root / "cmake.log"
                make_logging_tool(fake_bin / "cmake", "FT2_TEST_CMAKE_LOG")
                for compiler_name in compiler_names:
                    compiler = fake_bin / compiler_name
                    compiler.write_text("#!/usr/bin/env bash\nexit 0\n", encoding="utf-8")
                    compiler.chmod(0o755)

                environment = os.environ.copy()
                environment.update(overrides)
                environment["PATH"] = f"{fake_bin}:{environment['PATH']}"
                environment["FT2_TEST_CMAKE_LOG"] = str(cmake_log)
                build_dir = root / "target"
                result = subprocess.run(
                    [
                        str(ROOT / relative_path),
                        "--build-dir",
                        str(build_dir),
                        "--with-designer",
                        "-j",
                        "2",
                    ],
                    cwd=ROOT,
                    env=environment,
                    text=True,
                    capture_output=True,
                    check=False,
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                calls = cmake_log.read_text(encoding="utf-8").split("CALL\n")[1:]
                self.assertEqual(len(calls), 2)
                self.assertIn(f"-DCMAKE_TOOLCHAIN_FILE={toolchain}\n", calls[0])
                self.assertIn("-DFT2_BUILD_GUI_DESIGNER=1\n", calls[0])
                self.assertIn(
                    "--target\nft2-dxm\nft2_gui_designer\n",
                    calls[1],
                )

    def test_unavailable_cross_toolchain_uses_skip_status(self):
        environment = os.environ.copy()
        environment["MINGW_PREFIX"] = "ft2-definitely-missing-compiler"
        result = subprocess.run(
            [str(ROOT / "scripts/build-windows.sh")],
            cwd=ROOT,
            env=environment,
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertEqual(result.returncode, 77)
        self.assertIn("unavailable", result.stderr)

    def test_target_sweep_only_skips_explicitly_unavailable_toolchains(self):
        environment = os.environ.copy()
        environment["MINGW_PREFIX"] = "ft2-definitely-missing-compiler"
        command = [
            str(ROOT / "scripts/test-target-builds.sh"),
            "--no-native",
            "--no-designer",
            "--no-macos",
            "--no-raspi",
        ]
        result = subprocess.run(
            command,
            cwd=ROOT,
            env=environment,
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("SKIP: Windows MinGW build smoke", result.stdout)

        strict_result = subprocess.run(
            [*command, "--strict"],
            cwd=ROOT,
            env=environment,
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertEqual(strict_result.returncode, 77)

    def test_designer_makefile_normalizes_its_build_directory(self):
        result = subprocess.run(
            ["make", "-C", str(ROOT / "ft2_gui_designer"), "--dry-run", "test"],
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(
            f'--build-dir "{ROOT / "build-gui-designer"}"',
            result.stdout,
        )

    def test_fresh_build_preserves_external_ostirus_rom(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fake_bin = root / "tools"
            fake_bin.mkdir()
            cmake_log = root / "cmake.log"
            make_logging_tool(fake_bin / "cmake", "FT2_TEST_CMAKE_LOG")
            build_dir = root / "build"
            rom = build_dir / "bin" / "OsTIrus" / "rom.bin"
            rom.parent.mkdir(parents=True)
            rom.write_bytes(b"user-supplied-test-rom")
            (build_dir / "stale-object.o").write_bytes(b"stale")

            environment = os.environ.copy()
            environment["PATH"] = f"{fake_bin}:{environment['PATH']}"
            environment["FT2_TEST_CMAKE_LOG"] = str(cmake_log)
            result = subprocess.run(
                [
                    str(ROOT / "build-linux.sh"),
                    "--build-dir",
                    str(build_dir),
                    "--fresh",
                    "-j",
                    "1",
                ],
                cwd=ROOT,
                env=environment,
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(rom.read_bytes(), b"user-supplied-test-rom")
            self.assertFalse((build_dir / "stale-object.o").exists())
            self.assertIn("Preserved external OsTIrus ROM", result.stdout)

    def test_windows_sdl_runtime_staging_contract(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source_dir = root / "source"
            build_dir = root / "build"
            source_dir.mkdir()
            runtime = root / "SDL2.dll"
            runtime.write_bytes(b"test-runtime")
            (source_dir / "main.c").write_text(
                "int main(void) { return 0; }\n",
                encoding="utf-8",
            )
            (source_dir / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.16)\n"
                "project(runtime_staging C)\n"
                "set(WIN32 TRUE)\n"
                "add_library(SDL2::SDL2 SHARED IMPORTED)\n"
                f'set_target_properties(SDL2::SDL2 PROPERTIES IMPORTED_LOCATION "{runtime}")\n'
                "add_executable(runtime_test main.c)\n"
                f'include("{ROOT / "cmake" / "ft2_sdl2_runtime.cmake"}")\n'
                "ft2_stage_sdl2_runtime(runtime_test)\n",
                encoding="utf-8",
            )
            configure = subprocess.run(
                ["cmake", "-S", str(source_dir), "-B", str(build_dir)],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(configure.returncode, 0, configure.stderr)
            build = subprocess.run(
                ["cmake", "--build", str(build_dir)],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(build.returncode, 0, build.stderr)
            self.assertEqual((build_dir / "SDL2.dll").read_bytes(), b"test-runtime")


if __name__ == "__main__":
    unittest.main()
