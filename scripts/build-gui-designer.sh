#!/usr/bin/env bash
set -Eeuo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)
source "${script_dir}/build-common.sh"

repo_root=$(ft2_repo_root)
source_dir="${repo_root}/ft2_gui_designer"
build_dir="${FT2_GUI_DESIGNER_BUILD_DIR:-build-gui-designer}"
build_type="${FT2_BUILD_TYPE:-Release}"
jobs="${FT2_JOBS:-$(ft2_default_jobs)}"
fresh=0
clean_first=0
verbose=0
run_tests=0
install=0
print_deps=0
declare -a cmake_args=()

usage() {
    cat <<'EOF'
Usage: ./scripts/build-gui-designer.sh [options] [-- extra-cmake-args...]

Options:
  --debug              Configure a Debug build.
  --release            Configure a Release build. This is the default.
  --build-dir DIR      Use a custom CMake build directory.
  --fresh              Remove and recreate the build directory before configuring.
  --clean-first        Ask CMake to clean before building.
  -j, --jobs N         Parallel build jobs. Defaults to detected CPU count.
  -v, --verbose        Show verbose compiler/linker commands.
  --test               Run layout validation and bitmap pipeline tests.
  --install            Run the install step after building.
  --deps               Print expected GUI designer dependencies and exit.
  -h, --help           Show this help.

Environment overrides:
  FT2_BUILD_TYPE                 CMake build type.
  FT2_GUI_DESIGNER_BUILD_DIR     Build directory.
  FT2_JOBS                       Parallel build/test jobs.
  FT2_CMAKE_GENERATOR            Generator for a new build directory.
  SDL2_DIR / CMAKE_PREFIX_PATH   Target SDL2 package location.
  SDL2_DLL                       Optional Windows SDL2 runtime path.
EOF
}

print_deps() {
    cat <<'EOF'
Required GUI designer build tools/libraries:
  CMake and a C99 compiler
  SDL2 development package
  OpenGL and GLU development files

Common Linux packages:
  Debian/Ubuntu: build-essential cmake libsdl2-dev libgl-dev libglu1-mesa-dev
  Fedora: cmake gcc SDL2-devel mesa-libGL-devel mesa-libGLU-devel
  Arch: base-devel cmake sdl2 glu

Native macOS builds require Xcode Command Line Tools, CMake, and SDL2.
Windows builds require CMake, a C compiler, SDL2, and the system OpenGL SDK.
Cross-builds can pass a CMake toolchain file after --; tests require a runnable
target or a configured CMAKE_CROSSCOMPILING_EMULATOR.
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --debug) build_type=Debug ;;
        --release) build_type=Release ;;
        --build-dir)
            shift
            [ "$#" -gt 0 ] || ft2_die "--build-dir needs a directory"
            build_dir=$1
            ;;
        --fresh) fresh=1 ;;
        --clean-first) clean_first=1 ;;
        -j|--jobs)
            option=$1
            shift
            [ "$#" -gt 0 ] || ft2_die "${option} needs a job count"
            jobs=$1
            ;;
        -v|--verbose) verbose=1 ;;
        --test) run_tests=1 ;;
        --install) install=1 ;;
        --deps) print_deps=1 ;;
        -h|--help) usage; exit 0 ;;
        --)
            shift
            cmake_args+=("$@")
            break
            ;;
        *) ft2_die "unknown option: $1" ;;
    esac
    shift
done

if [ "$print_deps" -eq 1 ]; then
    print_deps
    exit 0
fi

ft2_validate_jobs "$jobs"
ft2_have cmake || ft2_die "cmake was not found in PATH. Run ./scripts/build-gui-designer.sh --deps for package hints."

build_path=$(ft2_make_abs_path "$build_dir")
ft2_prepare_build_dir "$source_dir" "$build_path" "$fresh"

configure_cmd=(cmake -S "$source_dir" -B "$build_path"
    -DCMAKE_BUILD_TYPE="$build_type"
    -DBUILD_TESTING=ON)
generator=$(ft2_select_generator "$build_path")
if [ -n "$generator" ]; then
    configure_cmd+=(-G "$generator")
fi
configure_cmd+=("${cmake_args[@]}")

printf 'Configuring FT2 GUI Designer (%s) in %s\n' "$build_type" "$build_path"
"${configure_cmd[@]}"

build_cmd=(cmake --build "$build_path" --target ft2_gui_designer)
if [ "$run_tests" -eq 1 ]; then
    build_cmd+=(ft2_gui_bitmap_tests)
fi
build_cmd+=(--parallel "$jobs")
if [ "$clean_first" -eq 1 ]; then
    build_cmd+=(--clean-first)
fi
if [ "$verbose" -eq 1 ]; then
    build_cmd+=(--verbose)
fi

printf 'Building FT2 GUI Designer with %s job(s)\n' "$jobs"
"${build_cmd[@]}"

if [ "$run_tests" -eq 1 ]; then
    printf 'Testing FT2 GUI Designer in %s\n' "$build_path"
    ctest --test-dir "$build_path" --output-on-failure --parallel "$jobs"
fi

if [ "$install" -eq 1 ]; then
    cmake --install "$build_path"
fi

if [ -f "${build_path}/ft2_gui_designer.exe" ]; then
    designer_binary="${build_path}/ft2_gui_designer.exe"
else
    designer_binary="${build_path}/ft2_gui_designer"
fi
printf 'Done. GUI Designer: %s\n' "$designer_binary"
