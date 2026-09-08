#!/usr/bin/env bash

ft2_die() {
    printf 'build: %s\n' "$*" >&2
    exit 1
}

ft2_unavailable() {
    printf 'build: unavailable: %s\n' "$*" >&2
    exit 77
}

ft2_have() {
    command -v "$1" >/dev/null 2>&1
}

ft2_repo_root() {
    local script_dir
    script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)
    cd -- "${script_dir}/.." >/dev/null 2>&1 && pwd
}

ft2_default_jobs() {
    if ft2_have nproc; then
        nproc
    elif ft2_have getconf; then
        getconf _NPROCESSORS_ONLN
    else
        printf '2\n'
    fi
}

ft2_print_linux_deps() {
    cat <<'EOF'
Required Linux build tools/libraries:
  cmake
  a C compiler and C++17 compiler
  SDL2 development package
  ALSA development package
  pthreads/libm from the system C toolchain
  OpenGL and GLU development packages when using --with-designer

Common package names:
  Debian/Ubuntu: build-essential cmake libsdl2-dev libasound2-dev libgl-dev libglu1-mesa-dev
  Fedora: cmake gcc gcc-c++ SDL2-devel alsa-lib-devel mesa-libGL-devel mesa-libGLU-devel
  Arch: base-devel cmake sdl2 alsa-lib glu
EOF
}

ft2_make_abs_path() {
    case "$1" in
        /*)
            printf '%s\n' "$1"
            ;;
        *)
            printf '%s/%s\n' "$(ft2_repo_root)" "$1"
            ;;
    esac
}

ft2_validate_build_path() {
    local build_path=$1
    local repo_root
    repo_root=$(ft2_repo_root)

    case "$build_path" in
        ''|/|"$repo_root")
            ft2_die "refusing to use unsafe build directory: ${build_path}"
            ;;
    esac
}

ft2_remove_build_dir() {
    local build_path=$1
    local repo_root
    local runtime_rom="${build_path}/bin/OsTIrus/rom.bin"
    local rom_backup_dir=
    repo_root=$(ft2_repo_root)

    ft2_validate_build_path "$build_path"
    case "$build_path" in
        "$repo_root"/*|/tmp/*)
            if [ -f "$runtime_rom" ] && [ -s "$runtime_rom" ]; then
                rom_backup_dir=$(mktemp -d "${TMPDIR:-/tmp}/ft2-ostirus-rom.XXXXXX")
                cp -p -- "$runtime_rom" "${rom_backup_dir}/rom.bin"
            fi
            printf 'Refreshing build directory: %s\n' "$build_path"
            rm -rf -- "$build_path"
            if [ -n "$rom_backup_dir" ]; then
                mkdir -p -- "$(dirname -- "$runtime_rom")"
                cp -p -- "${rom_backup_dir}/rom.bin" "$runtime_rom"
                rm -rf -- "$rom_backup_dir"
                printf 'Preserved external OsTIrus ROM at %s\n' "$runtime_rom"
            fi
            ;;
        *)
            ft2_die "refusing to remove build directory outside the repository or /tmp: ${build_path}"
            ;;
    esac
}

ft2_prepare_build_dir() {
    local source_path=$1
    local build_path=$2
    local fresh=${3:-0}
    local cache_file="${build_path}/CMakeCache.txt"
    local cache_home=
    local cache_build=

    ft2_validate_build_path "$build_path"

    if [ "$fresh" -eq 1 ]; then
        ft2_remove_build_dir "$build_path"
        return 0
    fi

    [ -f "$cache_file" ] || return 0
    cache_home=$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$cache_file" | tail -n 1)
    cache_build=$(sed -n 's/^# For build in directory: //p' "$cache_file" | head -n 1)

    if [ -n "$cache_home" ] && [ "$cache_home" != "$source_path" ]; then
        printf 'Existing CMake cache uses source directory %s instead of %s.\n' \
            "$cache_home" "$source_path"
        ft2_remove_build_dir "$build_path"
        return 0
    fi

    if [ -n "$cache_build" ] && [ "$cache_build" != "$build_path" ]; then
        printf 'Existing CMake cache uses build directory %s instead of %s.\n' \
            "$cache_build" "$build_path"
        ft2_remove_build_dir "$build_path"
    fi
}

ft2_select_generator() {
    local build_path=$1

    [ -f "${build_path}/CMakeCache.txt" ] && return 0
    if [ -n "${FT2_CMAKE_GENERATOR:-}" ]; then
        printf '%s\n' "$FT2_CMAKE_GENERATOR"
    elif ft2_have ninja; then
        printf 'Ninja\n'
    fi
}

ft2_validate_jobs() {
    case "$1" in
        ''|*[!0-9]*)
            ft2_die "job count must be a positive integer"
            ;;
        0)
            ft2_die "job count must be greater than zero"
            ;;
    esac
}

ft2_report_ostirus_rom() {
    local binary_dir=$1
    local runtime_rom="${binary_dir}/OsTIrus/rom.bin"

    if [ "${FT2_OSTIRUS_ROM+x}" = x ]; then
        if [ -z "$FT2_OSTIRUS_ROM" ]; then
            printf 'OsTIrus: ROM discovery explicitly disabled by an empty FT2_OSTIRUS_ROM.\n'
        elif [ -r "$FT2_OSTIRUS_ROM" ] && [ -s "$FT2_OSTIRUS_ROM" ]; then
            printf 'OsTIrus: external ROM configured through FT2_OSTIRUS_ROM.\n'
        else
            printf 'OsTIrus: FT2_OSTIRUS_ROM does not name a readable, non-empty file.\n' >&2
        fi
        return 0
    fi

    if [ -r "$runtime_rom" ] && [ -s "$runtime_rom" ]; then
        printf 'OsTIrus: runtime ROM found at %s\n' "$runtime_rom"
    else
        printf 'OsTIrus: optional ROM not staged; place rom.bin at %s or set FT2_OSTIRUS_ROM when launching.\n' "$runtime_rom"
    fi
}
