#!/usr/bin/env bash

ft2_die() {
    printf 'build-linux: %s\n' "$*" >&2
    exit 1
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

Common package names:
  Debian/Ubuntu: build-essential cmake libsdl2-dev libasound2-dev
  Fedora: cmake gcc gcc-c++ SDL2-devel alsa-lib-devel
  Arch: base-devel cmake sdl2 alsa-lib
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
