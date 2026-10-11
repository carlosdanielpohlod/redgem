#!/usr/bin/env bash
# Builds the engine for Windows (redgem.exe) from Linux and puts it where Build looks for it:
# runtimes/windows/ next to the Linux engine. After this, "Build Game > For Windows" in the editor
# and `redgem build <game> <out> --target windows` make .exe games.
#   tools/windows/build_runtime.sh
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build-windows"
ENGINE_DIR="${REPO_ROOT}/build/src"

"${REPO_ROOT}/tools/windows/fetch_toolchain.sh"
cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="${REPO_ROOT}/cmake/toolchains/windows.cmake" \
  -DCMAKE_BUILD_TYPE=Release -DREDGEM_BUILD_TESTS=OFF
cmake --build "${BUILD_DIR}" --target redgem

mkdir -p "${ENGINE_DIR}/runtimes/windows"
cp "${BUILD_DIR}/src/redgem.exe" "${ENGINE_DIR}/runtimes/windows/redgem.exe"
echo "Windows runtime ready: ${ENGINE_DIR}/runtimes/windows/redgem.exe"
