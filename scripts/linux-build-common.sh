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
