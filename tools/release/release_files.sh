#!/usr/bin/env bash
# What every download of a release carries, for the scripts that make and check them
# (package.sh, package_macos.sh, check_downloads.sh, smoke_test.sh) to source: the downloads'
# names, the programs, the demos and the licenses, and the check that what is packed was built.

# (What the scripts that source this read: shellcheck sees no use of it here.)
# shellcheck disable=SC2034
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT_DIR="${REPO_ROOT}/dist/release"
PACKAGE_NAME="redgem"
# The downloads a release carries. The README's links (releases/latest/download/<name>) depend on
# these names: a release gets all four or none (check_downloads.sh).
LINUX_DOWNLOAD="${PACKAGE_NAME}-linux-x86_64.tar.gz"
WINDOWS_DOWNLOAD="${PACKAGE_NAME}-windows-x86_64.zip"
MAC_ARM_DOWNLOAD="${PACKAGE_NAME}-macos-arm64.zip"
MAC_INTEL_DOWNLOAD="${PACKAGE_NAME}-macos-x86_64.zip"
DOWNLOADS=("${LINUX_DOWNLOAD}" "${WINDOWS_DOWNLOAD}" "${MAC_ARM_DOWNLOAD}" "${MAC_INTEL_DOWNLOAD}")
# The programs of the Linux and Mac downloads. The editor first: it is what the Mac's app starts.
# The others stand beside it as they do in a build (a game is built with the player found beside
# the editor).
PROGRAMS=(redgem redgem-player redgem-relay redgem-server redgem-test)
# The Windows download's programs.
WINDOWS_PROGRAMS=(redgem.exe redgem-player.exe redgem-relay.exe redgem-server.exe)
# The players for other systems the Linux download carries in bin/runtimes/.
WINDOWS_RUNTIME="runtimes/windows/redgem-player.exe"
ANDROID_RUNTIMES=(runtimes/android/arm64-v8a/libredgem.so runtimes/android/x86_64/libredgem.so)
# (The showcase is built from downloaded assets that may not be passed on.)
DEMOS=(platformer driving city)
DEMO_FILE="scene.glb"   # what every demo's folder holds at its top (for check_downloads.sh)
NOTICES=(LICENSE THIRD_PARTY_LICENSES.md)

# The engine's version, as CMakeLists.txt says it.
engine_version() {
  sed -n 's/^project(redgem VERSION \([0-9.]*\).*/\1/p' "${REPO_ROOT}/CMakeLists.txt"
}

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

# The licenses, and the download's README (tools/release/<$2>) as README.txt.
copy_notices() {
  local notice
  for notice in "${NOTICES[@]}"; do cp "${REPO_ROOT}/${notice}" "$1/"; done
  cp "${REPO_ROOT}/tools/release/$2" "$1/README.txt"
}
