#!/usr/bin/env bash
set -Eeuo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)
source "${script_dir}/build-common.sh"

repo_root=$(ft2_repo_root)
jobs="${FT2_JOBS:-$(ft2_default_jobs)}"
fresh=0
strict=0
run_native=1
run_designer=1
run_windows=1
run_macos=1
run_raspi=1
build_root="${FT2_TARGET_BUILD_ROOT:-/tmp/ft2-dxm-targets}"

usage() {
    cat <<'EOF'
Usage: ./scripts/test-target-builds.sh [options]

Runs the native Linux Release/Debug/ASan matrix, the GUI designer build/tests,
and configured target build smoke tests. Unavailable cross-toolchains return the
standard skip status (77) unless --strict is used. Build failures always fail.

Options:
  --fresh              Refresh selected build directories.
  --strict             Treat unavailable target toolchains as failures.
  --no-native          Skip native Linux matrix.
  --no-designer        Skip the native GUI designer build/tests.
  --no-windows         Skip Windows MinGW target.
  --no-macos           Skip macOS target.
  --no-raspi           Skip Raspberry Pi ALSA target.
  --build-root DIR     Put generated build dirs under DIR. Defaults to /tmp/ft2-dxm-targets.
  -j, --jobs N         Parallel build jobs. Defaults to detected CPU count.
  -h, --help           Show this help.
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --fresh) fresh=1 ;;
        --strict) strict=1 ;;
        --no-native) run_native=0 ;;
        --no-designer) run_designer=0 ;;
        --no-windows) run_windows=0 ;;
        --no-macos) run_macos=0 ;;
        --no-raspi) run_raspi=0 ;;
        --build-root)
            shift
            [ "$#" -gt 0 ] || ft2_die "--build-root needs a directory"
            build_root=$1
            ;;
        -j|--jobs)
            option=$1
            shift
            [ "$#" -gt 0 ] || ft2_die "${option} needs a job count"
            jobs=$1
            ;;
        -h|--help) usage; exit 0 ;;
        *) ft2_die "unknown option: $1" ;;
    esac
    shift
done

ft2_validate_jobs "$jobs"

case "$build_root" in
    /*) ;;
    *) build_root="${repo_root}/${build_root}" ;;
esac

run_or_skip() {
    local name=$1
    shift

    printf '\n== %s ==\n' "$name"
    if "$@"; then
        return 0
    else
        local status=$?
    fi

    if [ "$status" -eq 77 ] && [ "$strict" -eq 0 ]; then
        printf 'SKIP: %s is unavailable on this host.\n' "$name"
        return 0
    fi

    return "$status"
}

fresh_arg=()
[ "$fresh" -eq 1 ] && fresh_arg=(--fresh)

if [ "$run_native" -eq 1 ]; then
    "${repo_root}/scripts/test-linux.sh" "${fresh_arg[@]}" --build-root "$build_root/native" -j "$jobs"
fi

if [ "$run_designer" -eq 1 ]; then
    "${repo_root}/scripts/build-gui-designer.sh" "${fresh_arg[@]}" \
        --build-dir "$build_root/gui-designer" --test -j "$jobs"
fi

if [ "$run_windows" -eq 1 ]; then
    run_or_skip "Windows MinGW build smoke" \
        "${repo_root}/scripts/build-windows.sh" "${fresh_arg[@]}" --with-designer --build-dir "$build_root/windows-mingw64" -j "$jobs"
fi

if [ "$run_macos" -eq 1 ]; then
    run_or_skip "macOS build smoke" \
        "${repo_root}/scripts/build-macos.sh" "${fresh_arg[@]}" --with-designer --build-dir "$build_root/macos" -j "$jobs"
fi

if [ "$run_raspi" -eq 1 ]; then
    run_or_skip "Raspberry Pi ALSA build smoke" \
        "${repo_root}/scripts/build-raspi-alsa.sh" "${fresh_arg[@]}" --with-designer --build-dir "$build_root/raspi-armhf" -j "$jobs"
fi
