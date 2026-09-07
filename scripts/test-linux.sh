#!/usr/bin/env bash
set -Eeuo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)
source "${script_dir}/linux-build-common.sh"

repo_root=$(ft2_repo_root)
build_root="${FT2_TEST_BUILD_ROOT:-$repo_root}"
jobs="${FT2_JOBS:-$(ft2_default_jobs)}"
run_release=1
run_debug=1
run_asan=1
fresh=0
declare -a ctest_args=()

usage() {
    cat <<'EOF'
Usage: ./scripts/test-linux.sh [options] [-- extra-ctest-args...]

Options:
  --release-only       Run only the Release build/test phase.
  --debug-only         Run only the Debug build/test phase.
  --asan-only          Run only the ASan + leak-detection build/test phase.
  --no-release         Skip Release.
  --no-debug           Skip Debug.
  --no-asan            Skip ASan + leak detection.
  --fresh              Refresh each selected build directory before configuring.
  --build-root DIR     Put matrix build directories under DIR. Defaults to repo root.
  -j, --jobs N         Parallel build/test jobs. Defaults to detected CPU count.
  -h, --help           Show this help.

Environment:
  FT2_JOBS             Parallel build/test jobs.
  FT2_TEST_BUILD_ROOT  Directory that receives build-linux-release/debug/asan.
  ASAN_OPTIONS         Appended to the default ASan options.
  LSAN_OPTIONS         Appended to the default leak-sanitizer options.
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --release-only)
            run_release=1; run_debug=0; run_asan=0
            ;;
        --debug-only)
            run_release=0; run_debug=1; run_asan=0
            ;;
        --asan-only)
            run_release=0; run_debug=0; run_asan=1
            ;;
        --no-release)
            run_release=0
            ;;
        --no-debug)
            run_debug=0
            ;;
        --no-asan)
            run_asan=0
            ;;
        --fresh)
            fresh=1
            ;;
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
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            ctest_args+=("$@")
            break
            ;;
        *)
            ft2_die "unknown option: $1"
            ;;
    esac
    shift
done

case "$jobs" in
    ''|*[!0-9]*)
        ft2_die "job count must be a positive integer"
        ;;
    0)
        ft2_die "job count must be greater than zero"
        ;;
esac

case "$build_root" in
    /*)
        ;;
    *)
        build_root="${repo_root}/${build_root}"
        ;;
esac

case "$build_root" in
    ''|/)
        ft2_die "refusing to use unsafe build root: ${build_root}"
        ;;
esac

run_phase() {
    local name=$1
    local build_dir=$2
    local build_path="${build_root}/${build_dir}"
    shift 2

    printf '\n== %s ==\n' "$name"
    if [ "$fresh" -eq 1 ]; then
        rm -rf -- "$build_path"
    fi

    cmake -S "$repo_root" -B "$build_path" "$@"
    cmake --build "$build_path" --target ft2-dxm --parallel "$jobs"
    ctest --test-dir "$build_path" --output-on-failure --parallel "$jobs" "${ctest_args[@]}"
    ft2_report_ostirus_rom "$build_path/bin"
}

if [ "$run_release" -eq 1 ]; then
    run_phase "Release" build-linux-release -DCMAKE_BUILD_TYPE=Release
fi

if [ "$run_debug" -eq 1 ]; then
    run_phase "Debug" build-linux-debug -DCMAKE_BUILD_TYPE=Debug
fi

if [ "$run_asan" -eq 1 ]; then
    if [ -n "${ASAN_OPTIONS:-}" ]; then
        export ASAN_OPTIONS="detect_leaks=1:halt_on_error=1:abort_on_error=1:${ASAN_OPTIONS}"
    else
        export ASAN_OPTIONS="detect_leaks=1:halt_on_error=1:abort_on_error=1"
    fi

    if [ -n "${LSAN_OPTIONS:-}" ]; then
        export LSAN_OPTIONS="exitcode=23:${LSAN_OPTIONS}"
    else
        export LSAN_OPTIONS="exitcode=23"
    fi

    run_phase "ASan + Leak Detection" build-linux-asan \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
        -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
        -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
fi
