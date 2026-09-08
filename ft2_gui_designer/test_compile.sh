#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "$script_dir/.." && pwd)
build_dir=${1:-"$repo_root/build-gui-designer"}
jobs=${FT2_JOBS:-${JOBS:-4}}

exec "$repo_root/scripts/build-gui-designer.sh" \
    --release \
    --build-dir "$build_dir" \
    --jobs "$jobs" \
    --test
