#!/usr/bin/env bash
# Makes the downloads of a release from what is already built: the editor with its tools, the
# players for the systems games are built for, the demos and the licenses.
#   tools/release/package.sh            # dist/release/redgem-linux-x86_64.tar.gz
#                                       # dist/release/redgem-linux-x86_64.AppImage (the same files)
#                                       # dist/release/redgem-windows-x86_64.zip
#                                       # dist/release/redgem-symbols-{linux,windows,android}.zip
# Build first: cmake --build build, tools/windows/build_runtime.sh, tools/android/build_runtime.sh
# and, for the Windows download, cmake --build build-windows --target redgem redgem_relay redgem_server.
# On a Mac it makes the Mac's download instead (package_macos.sh).
set -euo pipefail

# shellcheck source=tools/release/release_files.sh
source "$(dirname "${BASH_SOURCE[0]}")/release_files.sh"

LINUX_BUILD="${REPO_ROOT}/build/src"
WINDOWS_BUILD="${REPO_ROOT}/build-windows/src"
ANDROID_BUILD="${REPO_ROOT}/build-android"   # tools/android/build_runtime.sh's, a folder per processor
WINDOWS_STRIP="${REPO_ROOT}/.deps/llvm-mingw/bin/x86_64-w64-mingw32-strip"
MAC_SYSTEM="Darwin"   # what uname says on a Mac
APPIMAGE_ARCH="x86_64"
BRANDING="${REPO_ROOT}/branding"

package_linux() {
  local root="$1/${PACKAGE_NAME}" program
  mkdir -p "${root}/bin/runtimes"
  for program in "${PROGRAMS[@]}"; do
    need "${LINUX_BUILD}/${program}"
    cp "${LINUX_BUILD}/${program}" "${root}/bin/"
    strip "${root}/bin/${program}"
  done
  local runtime
  for runtime in "${WINDOWS_RUNTIME}" "${ANDROID_RUNTIMES[@]}"; do need "${LINUX_BUILD}/${runtime}"; done
  cp -r "${LINUX_BUILD}/runtimes/windows" "${LINUX_BUILD}/runtimes/android" "${root}/bin/runtimes/"
  "${WINDOWS_STRIP}" "${root}/bin/${WINDOWS_RUNTIME}"
  copy_demos "${root}"
  copy_notices "${root}" README-linux.txt
  tar -czf "${OUT_DIR}/${LINUX_DOWNLOAD}" -C "$1" "${PACKAGE_NAME}"
}

# The AppImage, from the tarball's folder (in $1, as package_linux left it) with what starts it,
# its menu entry and its icon at the top. appimagetool and the runtime are the pinned ones
# (fetch_appimage_tools.sh); appimagetool runs unpacked, so packing needs no FUSE.
package_appimage() {
  local app_dir="$2/${PACKAGE_NAME}.AppDir" tools appimagetool runtime
  tools="$("${REPO_ROOT}/tools/release/fetch_appimage_tools.sh")"
  appimagetool="$(sed -n 1p <<<"${tools}")"
  runtime="$(sed -n 2p <<<"${tools}")"
  cp -a "$1/${PACKAGE_NAME}" "${app_dir}"
  install -m 755 "${REPO_ROOT}/tools/release/AppRun.sh" "${app_dir}/${APPIMAGE_ENTRY}"
  install -m 644 "${BRANDING}/redgem.desktop" "${app_dir}/${APPIMAGE_DESKTOP}"
  install -m 644 "${BRANDING}/redgem-mark.svg" "${app_dir}/${APPIMAGE_ICON}"
  ARCH="${APPIMAGE_ARCH}" APPIMAGE_EXTRACT_AND_RUN=1 "${appimagetool}" --no-appstream \
    --runtime-file "${runtime}" "${app_dir}" "${OUT_DIR}/${APPIMAGE_DOWNLOAD}"
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
  (cd "$1" && zip -qr "${OUT_DIR}/${WINDOWS_DOWNLOAD}" "${PACKAGE_NAME}")
}

# Copies built file $1 to $2 in the symbols' folder $3, making the folders on the way.
keep_symbols() {
  need "$1"
  mkdir -p "$(dirname "$3/$2")"
  cp "$1" "$3/$2"
}

# The players as they were linked, before they were stripped, each system's in an archive of its
# own beside the downloads (never in one): what `redgem symbolize` reads a crash report with.
package_symbols() {
  local root="$1/${SYMBOLS_PACKAGE}" abi
  keep_symbols "${LINUX_BUILD}/redgem-player" "${LINUX_PLAYER_SYMBOLS}" "${root}"
  keep_symbols "${WINDOWS_BUILD}/redgem-player.exe" "${WINDOWS_PLAYER_SYMBOLS}" "${root}"
  for abi in "${ANDROID_ABIS[@]}"; do
    keep_symbols "${ANDROID_BUILD}/${abi}/src/${ANDROID_PLAYER}" "android/${abi}/${ANDROID_PLAYER}" "${root}"
  done
  (cd "$1" && zip -qr "${OUT_DIR}/${LINUX_SYMBOLS}" "${SYMBOLS_PACKAGE}/linux")
  (cd "$1" && zip -qr "${OUT_DIR}/${WINDOWS_SYMBOLS}" "${SYMBOLS_PACKAGE}/windows")
  (cd "$1" && zip -qr "${OUT_DIR}/${ANDROID_SYMBOLS}" "${SYMBOLS_PACKAGE}/android")
}

main() {
  if [[ "$(uname -s)" == "${MAC_SYSTEM}" ]]; then
    exec "${REPO_ROOT}/tools/release/package_macos.sh"
  fi
  WORK_DIR="$(mktemp -d)"
  trap 'rm -rf "${WORK_DIR}"' EXIT
  rm -rf "${OUT_DIR}"
  mkdir -p "${OUT_DIR}" "${WORK_DIR}/linux" "${WORK_DIR}/appimage" "${WORK_DIR}/windows" "${WORK_DIR}/symbols"
  package_linux "${WORK_DIR}/linux"
  package_appimage "${WORK_DIR}/linux" "${WORK_DIR}/appimage"
  package_windows "${WORK_DIR}/windows"
  package_symbols "${WORK_DIR}/symbols"
  ls -l "${OUT_DIR}"
}

main "$@"
