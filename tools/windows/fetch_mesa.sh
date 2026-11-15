#!/usr/bin/env bash
# Downloads Mesa's OpenGL for Windows (mesa-dist-win, a pinned release checked by its checksum)
# and keeps the two libraries a program needs to draw with llvmpipe, Mesa's software renderer
# (OpenGL 3.3 core and more, as the engine asks), in .deps/mesa. Copied beside a program, they
# draw its windows on a machine with no graphics card, such as GitHub's Windows runners; with
# GALLIUM_DRIVER=llvmpipe. Prints the folder they are in. Needs 7z (GitHub's Windows runners
# have it).
#   tools/windows/fetch_mesa.sh
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DEST="${REPO_ROOT}/.deps/mesa"
RELEASE="26.2.4"
ARCHIVE="mesa3d-${RELEASE}-release-msvc.7z"
SHA256="351fc8c8b695878ffb3eaa044b3ead08672a48b1a045e3c3e3975811df0f6695"
URL="https://github.com/pal1000/mesa-dist-win/releases/download/${RELEASE}/${ARCHIVE}"
# What a program loads instead of Windows' own OpenGL, and the renderer it loads in turn.
LIBRARIES=(x64/opengl32.dll x64/libgallium_wgl.dll)
STAMP="${DEST}/release"   # which release the folder holds
WORK=""                   # (a global: the trap runs after main has returned)

main() {
  if [[ "$(cat "${STAMP}" 2>/dev/null)" == "${RELEASE}" ]]; then
    echo "${DEST}"
    return
  fi
  WORK="$(mktemp -d)"
  trap 'rm -rf "${WORK}"' EXIT
  echo "Downloading ${ARCHIVE}..." >&2
  curl -fsSL --retry 3 -o "${WORK}/${ARCHIVE}" "${URL}"
  echo "${SHA256}  ${WORK}/${ARCHIVE}" | sha256sum --check --status - ||
    { echo "${URL}: not the file pinned (sha256 ${SHA256})" >&2; exit 1; }
  7z x -y -o"${WORK}/unpacked" "${WORK}/${ARCHIVE}" "${LIBRARIES[@]}" > /dev/null
  rm -rf "${DEST}"
  mkdir -p "${DEST}"
  local library
  for library in "${LIBRARIES[@]}"; do cp "${WORK}/unpacked/${library}" "${DEST}/"; done
  echo "${RELEASE}" > "${STAMP}"
  echo "${DEST}"
}

main "$@"
