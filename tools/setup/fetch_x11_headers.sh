#!/usr/bin/env bash
# Extracts the X11 extension development headers that GLFW needs at compile time
# into .deps/x11 when they are not installed system-wide. No root access needed:
# GLFW loads the matching shared libraries with dlopen at runtime.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DEST_DIR="${REPO_ROOT}/.deps/x11"
SYSTEM_INCLUDE_DIR="/usr/include/X11/extensions"
REQUIRED_HEADERS=(Xrandr.h Xinerama.h Xrender.h)
PACKAGES=(libxrandr-dev libxinerama-dev libxcursor-dev libxrender-dev libxfixes-dev libxext-dev x11proto-dev)

all_present=true
for header in "${REQUIRED_HEADERS[@]}"; do
  [[ -f "${SYSTEM_INCLUDE_DIR}/${header}" ]] || all_present=false
done
if [[ -f "/usr/include/X11/Xcursor/Xcursor.h" && "${all_present}" == true ]]; then
  echo "X11 headers already installed system-wide."
  exit 0
fi

if [[ -f "${DEST_DIR}/usr/include/X11/extensions/Xrandr.h" ]]; then
  echo "X11 headers already extracted to ${DEST_DIR}."
  exit 0
fi

work_dir="$(mktemp -d)"
trap 'rm -rf "${work_dir}"' EXIT
mkdir -p "${DEST_DIR}"
(cd "${work_dir}" && apt-get download "${PACKAGES[@]}")
for deb in "${work_dir}"/*.deb; do
  dpkg-deb -x "${deb}" "${DEST_DIR}"
done
echo "X11 headers extracted to ${DEST_DIR}/usr/include"
