#!/usr/bin/env bash
set -Eeuo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)
source "${script_dir}/build-common.sh"

repo_root=$(ft2_repo_root)
build_dir="${FT2_MACOS_BUILD_DIR:-build-macos}"
build_type="${FT2_BUILD_TYPE:-Release}"
jobs="${FT2_JOBS:-$(ft2_default_jobs)}"
fresh=0
clean_first=0
verbose=0
run_tests=0
build_designer=0
install=0
universal=0
print_deps=0
declare -a cmake_args=()

usage() {
    cat <<'EOF'
Usage: ./scripts/build-macos.sh [options] [-- extra-cmake-args...]

Options:
  --debug              Configure a Debug build.
  --release            Configure a Release build. This is the default.
  --universal          Build x86_64 + arm64 on native macOS.
  --build-dir DIR      Use a custom CMake build directory.
  --fresh              Remove and recreate the build directory before configuring.
  --clean-first        Ask CMake to clean before building.
  -v, --verbose        Show verbose compiler/linker commands.
  --test               Run CTest after building. Only supported on native macOS.
  --with-designer      Build the FT2 GUI Designer in the same CMake tree.
  --install            Run the install step after building.
  -j, --jobs N         Parallel build jobs. Defaults to detected CPU count.
  --deps               Print expected macOS build dependencies and exit.
  -h, --help           Show this help.

Environment:
  OSXCROSS_ROOT        Optional osxcross installation root for Linux-hosted configure/build.
  OSXCROSS_TARGET      osxcross compiler command prefix. Defaults to o64-clang.
  SDL2_DIR             CMake SDL2 package directory.
  CMAKE_PREFIX_PATH    Extra CMake dependency roots.
EOF
}

print_deps() {
    cat <<'EOF'
Required for native macOS builds:
  Xcode Command Line Tools
  CMake
  SDL2 framework or CMake package

Linux-hosted macOS cross-builds are only configured when OSXCROSS_ROOT is set,
and still require a legally obtained macOS SDK plus macOS SDL2 dependency roots.
CTest can only run on native macOS.
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --debug) build_type=Debug ;;
        --release) build_type=Release ;;
        --universal) universal=1 ;;
        --build-dir)
            shift
            [ "$#" -gt 0 ] || ft2_die "--build-dir needs a directory"
            build_dir=$1
            ;;
        --fresh) fresh=1 ;;
        --clean-first) clean_first=1 ;;
        -v|--verbose) verbose=1 ;;
        --test) run_tests=1 ;;
        --with-designer) build_designer=1 ;;
        --install) install=1 ;;
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

build_path=$(ft2_make_abs_path "$build_dir")
ft2_prepare_build_dir "$repo_root" "$build_path" "$fresh"

cd -- "$repo_root"
configure_cmd=(cmake -S "$repo_root" -B "$build_path"
    -DCMAKE_BUILD_TYPE="$build_type"
    -DBUILD_TESTING=ON
    -DFT2_BUILD_GUI_DESIGNER="$build_designer")

if [ "$(uname -s)" = "Darwin" ]; then
    if [ "$universal" -eq 1 ]; then
        configure_cmd+=("-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64")
    fi
elif [ -n "${OSXCROSS_ROOT:-}" ]; then
    configure_cmd+=("-DCMAKE_TOOLCHAIN_FILE=${repo_root}/cmake/toolchain-macos-osxcross.cmake")
else
    ft2_unavailable "macOS builds require native macOS or OSXCROSS_ROOT. Run ./scripts/build-macos.sh --deps."
fi

generator=$(ft2_select_generator "$build_path")
if [ -n "$generator" ]; then
    configure_cmd+=(-G "$generator")
fi
configure_cmd+=("${cmake_args[@]}")
"${configure_cmd[@]}"

build_cmd=(cmake --build "$build_path" --target ft2-dxm)
if [ "$build_designer" -eq 1 ]; then
    build_cmd+=(ft2_gui_designer)
    if [ "$run_tests" -eq 1 ]; then
        build_cmd+=(ft2_gui_bitmap_tests)
    fi
fi
build_cmd+=(--parallel "$jobs")
if [ "$clean_first" -eq 1 ]; then
    build_cmd+=(--clean-first)
fi
if [ "$verbose" -eq 1 ]; then
    build_cmd+=(--verbose)
fi
"${build_cmd[@]}"

if [ "$run_tests" -eq 1 ]; then
    [ "$(uname -s)" = "Darwin" ] || ft2_die "--test for macOS builds requires native macOS"
    ctest --test-dir "$build_path" --output-on-failure --parallel "$jobs"
fi

if [ "$install" -eq 1 ]; then
    cmake --install "$build_path"
fi

ft2_report_ostirus_rom "$build_path/bin"
printf 'Done. macOS binary dir: %s/bin\n' "$build_path"
if [ "$build_designer" -eq 1 ]; then
    printf 'Done. macOS GUI Designer: %s/bin/ft2_gui_designer\n' "$build_path"
fi
