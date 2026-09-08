#!/usr/bin/env bash
set -Eeuo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)
source "${script_dir}/scripts/build-common.sh"

repo_root=$(ft2_repo_root)
build_dir="${FT2_LINUX_BUILD_DIR:-build-linux}"
build_type="${FT2_BUILD_TYPE:-Release}"
jobs="${FT2_JOBS:-$(ft2_default_jobs)}"
fresh=0
clean_first=0
verbose=0
install=0
run_tests=0
build_designer=0
print_deps=0
declare -a cmake_args=()

usage() {
    cat <<'EOF'
Usage: ./build-linux.sh [options] [-- extra-cmake-args...]

Options:
  --debug              Configure a Debug build.
  --release            Configure a Release build. This is the default.
  --build-dir DIR      Use a custom CMake build directory.
  --fresh              Remove and recreate the build directory before configuring.
  --clean-first        Ask CMake to clean before building.
  -j, --jobs N         Parallel build jobs. Defaults to detected CPU count.
  -v, --verbose        Show verbose compiler/linker commands.
  --test               Run CTest after building.
  --with-designer      Build the FT2 GUI Designer in the same CMake tree.
  --install            Run the install step after building.
  --deps               Print expected Linux dependencies and exit.
  -h, --help           Show this help.

Environment overrides:
  FT2_BUILD_TYPE       CMake build type, for example Release or Debug.
  FT2_LINUX_BUILD_DIR  Build directory.
  FT2_JOBS             Parallel build jobs.
  FT2_CMAKE_GENERATOR  CMake generator for a new build dir, for example Ninja.
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --debug)
            build_type=Debug
            ;;
        --release)
            build_type=Release
            ;;
        --build-dir)
            shift
            [ "$#" -gt 0 ] || ft2_die "--build-dir needs a directory"
            build_dir=$1
            ;;
        --fresh)
            fresh=1
            ;;
        --clean-first)
            clean_first=1
            ;;
        -j|--jobs)
            option=$1
            shift
            [ "$#" -gt 0 ] || ft2_die "${option} needs a job count"
            jobs=$1
            ;;
        -v|--verbose)
            verbose=1
            ;;
        --test)
            run_tests=1
            ;;
        --with-designer)
            build_designer=1
            ;;
        --install)
            install=1
            ;;
        --deps)
            print_deps=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            cmake_args+=("$@")
            break
            ;;
        *)
            ft2_die "unknown option: $1"
            ;;
    esac
    shift
done

if [ "$print_deps" -eq 1 ]; then
    ft2_print_linux_deps
    exit 0
fi

build_dir=$(ft2_make_abs_path "$build_dir")

ft2_have cmake || ft2_die "cmake was not found in PATH. Run ./build-linux.sh --deps for package hints."

ft2_validate_jobs "$jobs"
ft2_prepare_build_dir "$repo_root" "$build_dir" "$fresh"

cd -- "$repo_root"

configure_cmd=(cmake -S "$repo_root" -B "$build_dir"
    -DCMAKE_BUILD_TYPE="$build_type"
    -DBUILD_TESTING=ON
    -DFT2_BUILD_GUI_DESIGNER="$build_designer")
generator=$(ft2_select_generator "$build_dir")
if [ -n "$generator" ]; then
    configure_cmd+=(-G "$generator")
fi
configure_cmd+=("${cmake_args[@]}")

printf 'Configuring ft2-dxm (%s) in %s\n' "$build_type" "$build_dir"
"${configure_cmd[@]}"

build_cmd=(cmake --build "$build_dir" --target ft2-dxm)
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

printf 'Building ft2-dxm with %s job(s)\n' "$jobs"
"${build_cmd[@]}"

if [ "$run_tests" -eq 1 ]; then
    printf 'Testing ft2-dxm in %s\n' "$build_dir"
    ctest --test-dir "$build_dir" --output-on-failure --parallel "$jobs"
fi

if [ "$install" -eq 1 ]; then
    printf 'Installing ft2-dxm from %s\n' "$build_dir"
    cmake --install "$build_dir"
fi

ft2_report_ostirus_rom "${build_dir}/bin"
printf 'Done. Binary: %s\n' "${build_dir}/bin/ft2-dxm"
if [ "$build_designer" -eq 1 ]; then
    printf 'Done. GUI Designer: %s\n' "${build_dir}/bin/ft2_gui_designer"
fi
