#!/usr/bin/env bash
# Builds the player for Windows (redgem-player.exe: what a built game's .exe is a copy of) from
# Linux and puts it where Build looks for it: runtimes/windows/ beside the Linux programs. After
# this, "Build Game > For Windows" in the editor and `redgem build <game> <out> --target windows`
# make .exe games. Only the player is built: the editor and the other tools are not what ships.
#   tools/windows/build_runtime.sh
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build-windows"
ENGINE_DIR="${REPO_ROOT}/build/src"

"${REPO_ROOT}/tools/windows/fetch_toolchain.sh"
cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="${REPO_ROOT}/cmake/toolchains/windows.cmake" \
  -DCMAKE_BUILD_TYPE=Release -DREDGEM_BUILD_TESTS=OFF
# (Half the processors: building for another system while something else runs must not starve the machine.)
cmake --build "${BUILD_DIR}" --target redgem_player --parallel "$(( ($(nproc) + 1) / 2 ))"

mkdir -p "${ENGINE_DIR}/runtimes/windows"
cp "${BUILD_DIR}/src/redgem-player.exe" "${ENGINE_DIR}/runtimes/windows/redgem-player.exe"
# (What this script once made: a whole editor that games were copies of. Nothing looks for it now.)
rm -f "${ENGINE_DIR}/runtimes/windows/redgem.exe"
echo "Windows player ready: ${ENGINE_DIR}/runtimes/windows/redgem-player.exe"
