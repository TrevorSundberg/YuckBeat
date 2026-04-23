#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_type="${CMAKE_BUILD_TYPE:-Release}"
build_dir="${YUCKBEAT_BUILD_DIR:-"${repo_root}/build-win"}"
vst3_sdk_root="${VST3_SDK_ROOT:-/home/trevor/vst3sdk}"
llvm_mingw_root="${LLVM_MINGW_ROOT:-/home/trevor/.local/toolchains/llvm-mingw-20260421-ucrt-ubuntu-22.04-x86_64}"
copy_after_build="${YUCKBEAT_COPY_VST3_AFTER_BUILD:-OFF}"

cmake_args=(
  -S "${repo_root}"
  -B "${build_dir}"
  -G Ninja
  -DCMAKE_TOOLCHAIN_FILE="${repo_root}/cmake/llvm-mingw-x86_64.cmake"
  -DCMAKE_BUILD_TYPE="${build_type}"
  -DVST3_SDK_ROOT="${vst3_sdk_root}"
  -DLLVM_MINGW_ROOT="${llvm_mingw_root}"
  -DYUCKBEAT_COPY_VST3_AFTER_BUILD="${copy_after_build}"
)

if [[ -n "${YUCKBEAT_VST3_COPY_DIR:-}" ]]; then
  cmake_args+=("-DYUCKBEAT_VST3_COPY_DIR=${YUCKBEAT_VST3_COPY_DIR}")
fi

cmake "${cmake_args[@]}"
cmake --build "${build_dir}" --config "${build_type}" --target YuckBeat smoke_load_vst3 smoke_hot_reload_engine -j "${YUCKBEAT_BUILD_JOBS:-4}"
