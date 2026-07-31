#!/usr/bin/env bash
set -Eeuo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)
source "${script_dir}/scripts/linux-build-common.sh"

repo_root=$(ft2_repo_root)
build_dir_arg="${FT2_LINUX_BUILD_DIR:-build-linux}"
build_dir="$build_dir_arg"
build_type="${FT2_BUILD_TYPE:-Release}"
jobs="${FT2_JOBS:-$(ft2_default_jobs)}"
fresh=0
clean_first=0
verbose=0
install=0
run_tests=0
print_deps=0
declare -a cmake_args=()

refresh_build_dir() {
    local reason=$1

    case "$build_dir" in
        ''|/|"$repo_root")
            ft2_die "refusing to remove unsafe build directory: ${build_dir}"
            ;;
        "$repo_root"/*|/tmp/*)
            printf '%s\n' "$reason"
            printf 'Refreshing build directory: %s\n' "$build_dir"
            rm -rf -- "$build_dir"
            ;;
        *)
            ft2_die "${reason} Re-run with --fresh if you want this script to remove ${build_dir}."
            ;;
    esac
}

refresh_stale_cmake_cache() {
    local cache_file="${build_dir}/CMakeCache.txt"
    local cache_home=
    local cache_build=

    [ -f "$cache_file" ] || return 0

    cache_home=$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$cache_file" | tail -n 1)
    cache_build=$(sed -n 's/^# For build in directory: //p' "$cache_file" | head -n 1)

    if [ -n "$cache_home" ] && [ "$cache_home" != "$repo_root" ]; then
        refresh_build_dir "Existing CMake cache was created for source directory ${cache_home}, not ${repo_root}."
        return 0
    fi

    if [ -n "$cache_build" ] && [ "$cache_build" != "$build_dir" ]; then
        refresh_build_dir "Existing CMake cache was created for build directory ${cache_build}, not ${build_dir}."
        return 0
    fi
}

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

case "$build_dir" in
    /*)
        build_dir_arg="$build_dir"
        ;;
    *)
        build_dir="${repo_root}/${build_dir}"
        ;;
esac

ft2_have cmake || ft2_die "cmake was not found in PATH. Run ./build-linux.sh --deps for package hints."

case "$jobs" in
    ''|*[!0-9]*)
        ft2_die "job count must be a positive integer"
        ;;
    0)
        ft2_die "job count must be greater than zero"
        ;;
esac

if [ "$fresh" -eq 1 ]; then
    refresh_build_dir "Removing build directory because --fresh was requested."
else
    refresh_stale_cmake_cache
fi

cd -- "$repo_root"

configure_cmd=(cmake -S . -B "$build_dir_arg" -DCMAKE_BUILD_TYPE="$build_type")
if [ ! -f "${build_dir}/CMakeCache.txt" ] && [ -n "${FT2_CMAKE_GENERATOR:-}" ]; then
    configure_cmd+=(-G "$FT2_CMAKE_GENERATOR")
elif [ ! -f "${build_dir}/CMakeCache.txt" ] && ft2_have ninja; then
    configure_cmd+=(-G Ninja)
fi
configure_cmd+=("${cmake_args[@]}")

printf 'Configuring ft2-dxm (%s) in %s\n' "$build_type" "$build_dir"
"${configure_cmd[@]}"

build_cmd=(cmake --build "$build_dir_arg" --parallel "$jobs")
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
    ctest --test-dir "$build_dir_arg" --output-on-failure --parallel "$jobs"
fi

if [ "$install" -eq 1 ]; then
    printf 'Installing ft2-dxm from %s\n' "$build_dir"
    cmake --install "$build_dir_arg"
fi

printf 'Done. Binary: %s\n' "${build_dir}/bin/ft2-dxm"
