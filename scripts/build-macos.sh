#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_type="${CMAKE_BUILD_TYPE:-Release}"
build_dir="${YUCKBEAT_BUILD_DIR:-"${repo_root}/build-macos"}"
vst3_sdk_root="${VST3_SDK_ROOT:-"${HOME}/vst3sdk"}"
copy_after_build="${YUCKBEAT_COPY_VST3_AFTER_BUILD:-OFF}"

cmake -S "${repo_root}" -B "${build_dir}" -G Ninja \
  -DCMAKE_BUILD_TYPE="${build_type}" \
  -DVST3_SDK_ROOT="${vst3_sdk_root}" \
  -DYUCKBEAT_COPY_VST3_AFTER_BUILD="${copy_after_build}"

cmake --build "${build_dir}" --config "${build_type}" --target YuckBeat smoke_load_vst3 smoke_hot_reload_engine smoke_hot_reload_visual -j "${YUCKBEAT_BUILD_JOBS:-4}"
