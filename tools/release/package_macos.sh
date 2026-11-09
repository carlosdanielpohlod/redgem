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
INFO_TEMPLATE="${REPO_ROOT}/tools/release/Info.plist"
VERSION_MARK="@VERSION@"
MINIMUM_MARK="@MINIMUM_SYSTEM@"
DEFAULT_MINIMUM_SYSTEM="11.0"   # the oldest macOS the programs were built for, unless the build said
AD_HOC_IDENTITY="-"              # codesign's name for "no identity"
ARM_PROCESSOR="arm64"            # what uname -m says on Apple Silicon
INTEL_PROCESSOR="x86_64"         # and on an Intel Mac

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

# The download's name for this Mac's kind of processor $1 (as uname -m says it).
download_for() {
  case "$1" in
    "${ARM_PROCESSOR}") echo "${MAC_ARM_DOWNLOAD}" ;;
    "${INTEL_PROCESSOR}") echo "${MAC_INTEL_DOWNLOAD}" ;;
    *) echo "no download for a Mac with a $1 processor" >&2; exit 1 ;;
  esac
}

main() {
  local download root
  download="$(download_for "$(uname -m)")"
  WORK_DIR="$(mktemp -d)"
  trap 'rm -rf "${WORK_DIR}"' EXIT
  root="${WORK_DIR}/${PACKAGE_NAME}"
  rm -rf "${OUT_DIR}"
  mkdir -p "${OUT_DIR}" "${root}"
  make_app "${root}"
  copy_demos "${root}"
  copy_notices "${root}" README-macos.txt
  (cd "${WORK_DIR}" && zip -qry "${OUT_DIR}/${download}" "${PACKAGE_NAME}")
  ls -l "${OUT_DIR}"
}

main "$@"
