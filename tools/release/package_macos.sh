#!/usr/bin/env bash
# Makes the Mac's download from what is already built on this Mac: RedGem.app (the editor and its
# tools; opened, it shows the project hub), the demos and the licenses, for this Mac's kind of
# processor.
#   tools/release/package_macos.sh      # dist/release/redgem-macos-arm64.zip (or -x86_64)
# Build first: cmake -S . -B build -G Ninja && cmake --build build
# The app is not signed with a developer's identity (that needs a paid Apple account): its
# programs only carry the ad hoc signature an Apple Silicon Mac requires of everything it runs.
set -euo pipefail

# shellcheck source=tools/release/release_files.sh
source "$(dirname "${BASH_SOURCE[0]}")/release_files.sh"

BUILD="${REPO_ROOT}/build/src"
APP_NAME="RedGem.app"
# The editor first: it is what the app starts. The others stand beside it as they do in a build
# (a game is built with the player found beside the editor).
PROGRAMS=(redgem redgem-player redgem-relay redgem-test)
INFO_TEMPLATE="${REPO_ROOT}/tools/release/Info.plist"
VERSION_MARK="@VERSION@"
MINIMUM_MARK="@MINIMUM_SYSTEM@"
DEFAULT_MINIMUM_SYSTEM="11.0"   # the oldest macOS the programs were built for, unless the build said
AD_HOC_IDENTITY="-"              # codesign's name for "no identity"

# The engine's version, as CMakeLists.txt says it.
engine_version() {
  sed -n 's/^project(redgem VERSION \([0-9.]*\).*/\1/p' "${REPO_ROOT}/CMakeLists.txt"
}

# The app's description, with the version and the oldest macOS it opens on written in.
write_info() {
  local minimum="${MACOSX_DEPLOYMENT_TARGET:-${DEFAULT_MINIMUM_SYSTEM}}"
  sed -e "s/${VERSION_MARK}/$(engine_version)/g" -e "s/${MINIMUM_MARK}/${minimum}/g" "${INFO_TEMPLATE}" > "$1/Info.plist"
}

# RedGem.app, in folder $1: its description and its programs, stripped and signed again.
make_app() {
  local contents="$1/${APP_NAME}/Contents" program
  mkdir -p "${contents}/MacOS" "${contents}/Resources"
  write_info "${contents}"
  for program in "${PROGRAMS[@]}"; do
    need "${BUILD}/${program}"
    cp "${BUILD}/${program}" "${contents}/MacOS/"
    strip "${contents}/MacOS/${program}"
    codesign --force --sign "${AD_HOC_IDENTITY}" "${contents}/MacOS/${program}"
  done
  codesign --force --sign "${AD_HOC_IDENTITY}" "$1/${APP_NAME}"
}

main() {
  local arch work root
  arch="$(uname -m)"
  work="$(mktemp -d)"
  trap 'rm -rf "${work}"' EXIT
  root="${work}/${PACKAGE_NAME}"
  rm -rf "${OUT_DIR}"
  mkdir -p "${OUT_DIR}" "${root}"
  make_app "${root}"
  copy_demos "${root}"
  copy_notices "${root}" README-macos.txt
  (cd "${work}" && zip -qry "${OUT_DIR}/${PACKAGE_NAME}-macos-${arch}.zip" "${PACKAGE_NAME}")
  ls -l "${OUT_DIR}"
}

main "$@"
