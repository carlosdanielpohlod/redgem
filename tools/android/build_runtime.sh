#!/usr/bin/env bash
# Builds the player for Android (libredgem.so: the runtime alone, as the library a game's app
# loads) from Linux, once per kind of processor, and puts it where Build looks for it:
# runtimes/android/<abi>/ beside the Linux programs. After this,
# `redgem build <game> <out> --target android` makes .apk games.
#   tools/android/build_runtime.sh              # phones (arm64-v8a) and emulators (x86_64)
#   tools/android/build_runtime.sh x86_64       # only one
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ENGINE_DIR="${REPO_ROOT}/build/src"
NDK="${REPO_ROOT}/.deps/android-ndk"
# The oldest Android the engine is built for (26 is Android 8: every 64-bit device of the last
# years). Written once, in a file the packer of the apps reads as well (see src/CMakeLists.txt).
MIN_ANDROID_API="$(tr -d '[:space:]' <"${REPO_ROOT}/tools/android/min_api.txt")"
ABIS=("$@")
if [[ ${#ABIS[@]} -eq 0 ]]; then
  ABIS=(arm64-v8a x86_64)
fi

"${REPO_ROOT}/tools/android/fetch_toolchain.sh"
for abi in "${ABIS[@]}"; do
  build_dir="${REPO_ROOT}/build-android/${abi}"
  cmake -S "${REPO_ROOT}" -B "${build_dir}" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="${NDK}/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="${abi}" -DANDROID_PLATFORM="android-${MIN_ANDROID_API}" -DANDROID_STL=c++_static \
    -DCMAKE_BUILD_TYPE=Release -DREDGEM_BUILD_TESTS=OFF
  # (Half the processors: building for a phone while something else runs must not starve the machine.)
  cmake --build "${build_dir}" --target redgem --parallel "$(( ($(nproc) + 1) / 2 ))"
  mkdir -p "${ENGINE_DIR}/runtimes/android/${abi}"
  # Without its debugging information: a tenth of the size, and it goes into every game's APK.
  "${NDK}/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip" -o "${ENGINE_DIR}/runtimes/android/${abi}/libredgem.so" \
    "${build_dir}/src/libredgem.so"
  echo "Android player ready: ${ENGINE_DIR}/runtimes/android/${abi}/libredgem.so"
done
