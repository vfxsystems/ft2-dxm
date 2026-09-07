#!/usr/bin/env bash
set -Eeuo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)
source "${script_dir}/linux-build-common.sh"

repo_root=$(ft2_repo_root)
build_dir="${FT2_WINDOWS_BUILD_DIR:-build-windows-mingw64}"
build_type="${FT2_BUILD_TYPE:-Release}"
jobs="${FT2_JOBS:-$(ft2_default_jobs)}"
fresh=0
run_tests=0
print_deps=0
declare -a cmake_args=()

usage() {
    cat <<'EOF'
Usage: ./scripts/build-windows.sh [options] [-- extra-cmake-args...]

Options:
  --debug              Configure a Debug build.
  --release            Configure a Release build. This is the default.
  --build-dir DIR      Use a custom CMake build directory.
  --fresh              Remove and recreate the build directory before configuring.
  --test               Run CTest after building. Requires wine on non-Windows hosts.
  -j, --jobs N         Parallel build jobs. Defaults to detected CPU count.
  --deps               Print expected Windows cross-build dependencies and exit.
  -h, --help           Show this help.

Environment:
  MINGW_PREFIX         Compiler prefix. Defaults to x86_64-w64-mingw32.
  SDL2_DIR             CMake SDL2 package directory for the MinGW SDL2 build.
  CMAKE_PREFIX_PATH    Extra CMake dependency roots.
EOF
}

print_deps() {
    cat <<'EOF'
Required for Linux-hosted Windows builds:
  x86_64-w64-mingw32-gcc and x86_64-w64-mingw32-g++
  MinGW-compatible SDL2 development files
  cmake
  wine, only if you want --test on Linux

Common Debian/Ubuntu package names:
  mingw-w64 cmake wine

SDL2 must be supplied as a MinGW package/root via SDL2_DIR or CMAKE_PREFIX_PATH.
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
        --test) run_tests=1 ;;
        -j|--jobs)
            option=$1
            shift
            [ "$#" -gt 0 ] || ft2_die "${option} needs a job count"
            jobs=$1
            ;;
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
ft2_have cmake || ft2_die "cmake was not found in PATH."

mingw_prefix="${MINGW_PREFIX:-x86_64-w64-mingw32}"
ft2_have "${mingw_prefix}-gcc" || ft2_die "${mingw_prefix}-gcc was not found. Run ./scripts/build-windows.sh --deps."
ft2_have "${mingw_prefix}-g++" || ft2_die "${mingw_prefix}-g++ was not found. Run ./scripts/build-windows.sh --deps."

build_path=$(ft2_make_abs_path "$build_dir")
if [ "$fresh" -eq 1 ]; then
    case "$build_path" in
        "$repo_root"/*|/tmp/*) rm -rf -- "$build_path" ;;
        *) ft2_die "refusing to remove unsafe build directory: ${build_path}" ;;
    esac
fi

cd -- "$repo_root"
cmake -S . -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="$build_type" \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake \
    -DMINGW_PREFIX="$mingw_prefix" \
    "${cmake_args[@]}"

cmake --build "$build_dir" --target ft2-dxm --parallel "$jobs"

if [ "$run_tests" -eq 1 ]; then
    case "$(uname -s)" in
        MINGW*|MSYS*) ;;
        *)
            ft2_have wine || ft2_die "--test for Windows cross-builds requires wine on this host"
            ;;
    esac
    ctest --test-dir "$build_dir" --output-on-failure --parallel "$jobs"
fi

ft2_report_ostirus_rom "$build_path/bin"
printf 'Done. Windows binary: %s/bin/ft2-dxm.exe\n' "$build_path"
