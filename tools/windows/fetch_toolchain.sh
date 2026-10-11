#!/usr/bin/env bash
# Downloads the compiler that builds RedGem for Windows from Linux (llvm-mingw, a pinned release)
# into .deps/llvm-mingw. No root access needed.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RELEASE="20261006"
ARCHIVE="llvm-mingw-${RELEASE}-ucrt-ubuntu-22.04-x86_64.tar.xz"
URL="https://github.com/mstorsjo/llvm-mingw/releases/download/${RELEASE}/${ARCHIVE}"
DEST="${REPO_ROOT}/.deps/llvm-mingw"

if [[ -x "${DEST}/bin/x86_64-w64-mingw32-clang++" ]]; then
  echo "Windows toolchain already in ${DEST}."
  exit 0
fi

work_dir="$(mktemp -d)"
trap 'rm -rf "${work_dir}"' EXIT
echo "Downloading ${ARCHIVE}..."
curl -fL --retry 3 -o "${work_dir}/${ARCHIVE}" "${URL}"
mkdir -p "${DEST}"
tar -xf "${work_dir}/${ARCHIVE}" -C "${DEST}" --strip-components=1
echo "Windows toolchain ready in ${DEST}."
