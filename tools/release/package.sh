#!/usr/bin/env bash
# Makes the downloads of a release from what is already built: the editor with its tools, the
# players for the systems games are built for, the demos and the licenses.
#   tools/release/package.sh            # dist/release/redgem-linux-x86_64.tar.gz
#                                       # dist/release/redgem-windows-x86_64.zip
# Build first: cmake --build build, tools/windows/build_runtime.sh, tools/android/build_runtime.sh
# and, for the Windows download, cmake --build build-windows --target redgem redgem_relay.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT_DIR="${REPO_ROOT}/dist/release"
LINUX_BUILD="${REPO_ROOT}/build/src"
WINDOWS_BUILD="${REPO_ROOT}/build-windows/src"
WINDOWS_STRIP="${REPO_ROOT}/.deps/llvm-mingw/bin/x86_64-w64-mingw32-strip"
PACKAGE_NAME="redgem"
LINUX_PROGRAMS=(redgem redgem-player redgem-relay redgem-test)
WINDOWS_PROGRAMS=(redgem.exe redgem-player.exe redgem-relay.exe)
# (The showcase is built from downloaded assets that may not be passed on.)
DEMOS=(platformer driving city)
NOTICES=(LICENSE THIRD_PARTY_LICENSES.md)

need() {
  [[ -e "$1" ]] || { echo "missing $1: build it first (see the top of this script)" >&2; exit 1; }
}

# The demos as git has them: no saves, builds or editor leftovers.
copy_demos() {
  local demo
  for demo in "${DEMOS[@]}"; do
    git -C "${REPO_ROOT}" archive --format=tar --prefix="games/${demo}/" "HEAD:games/${demo}" | tar -x -C "$1"
  done
}

copy_notices() {
  local notice
  for notice in "${NOTICES[@]}"; do cp "${REPO_ROOT}/${notice}" "$1/"; done
  cp "${REPO_ROOT}/tools/release/$2" "$1/README.txt"
}

package_linux() {
  local root="$1/${PACKAGE_NAME}" program
  mkdir -p "${root}/bin/runtimes"
  for program in "${LINUX_PROGRAMS[@]}"; do
    need "${LINUX_BUILD}/${program}"
    cp "${LINUX_BUILD}/${program}" "${root}/bin/"
    strip "${root}/bin/${program}"
  done
  need "${LINUX_BUILD}/runtimes/windows/redgem-player.exe"
  need "${LINUX_BUILD}/runtimes/android"
  cp -r "${LINUX_BUILD}/runtimes/windows" "${LINUX_BUILD}/runtimes/android" "${root}/bin/runtimes/"
  "${WINDOWS_STRIP}" "${root}/bin/runtimes/windows/redgem-player.exe"
  copy_demos "${root}"
  copy_notices "${root}" README-linux.txt
  tar -czf "${OUT_DIR}/${PACKAGE_NAME}-linux-x86_64.tar.gz" -C "$1" "${PACKAGE_NAME}"
}

package_windows() {
  local root="$1/${PACKAGE_NAME}" program
  mkdir -p "${root}"
  for program in "${WINDOWS_PROGRAMS[@]}"; do
    need "${WINDOWS_BUILD}/${program}"
    cp "${WINDOWS_BUILD}/${program}" "${root}/"
    "${WINDOWS_STRIP}" "${root}/${program}"
  done
  copy_demos "${root}"
  copy_notices "${root}" README-windows.txt
  (cd "$1" && zip -qr "${OUT_DIR}/${PACKAGE_NAME}-windows-x86_64.zip" "${PACKAGE_NAME}")
}

main() {
  local work
  work="$(mktemp -d)"
  trap "rm -rf '${work}'" EXIT
  rm -rf "${OUT_DIR}"
  mkdir -p "${OUT_DIR}" "${work}/linux" "${work}/windows"
  package_linux "${work}/linux"
  package_windows "${work}/windows"
  ls -l "${OUT_DIR}"
}

main "$@"
