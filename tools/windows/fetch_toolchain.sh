#!/usr/bin/env bash
# Downloads the compiler that builds RedGem for Windows (llvm-mingw, a pinned release) into
# .deps/llvm-mingw: the one that runs on Linux, or, from a Windows shell (Git Bash, MSYS2), the
# same release built to run on Windows itself. No root access needed.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RELEASE="20261006"
DEST="${REPO_ROOT}/.deps/llvm-mingw"
case "$(uname -s)" in
  MINGW* | MSYS*) ARCHIVE="llvm-mingw-${RELEASE}-ucrt-x86_64.zip" ;;
  *) ARCHIVE="llvm-mingw-${RELEASE}-ucrt-ubuntu-22.04-x86_64.tar.xz" ;;
esac
URL="https://github.com/mstorsjo/llvm-mingw/releases/download/${RELEASE}/${ARCHIVE}"

if [[ -x "${DEST}/bin/x86_64-w64-mingw32-clang++" ]]; then
  echo "Windows toolchain already in ${DEST}."
  exit 0
fi

work_dir="$(mktemp -d)"
trap 'rm -rf "${work_dir}"' EXIT
echo "Downloading ${ARCHIVE}..."
curl -fL --retry 3 -o "${work_dir}/${ARCHIVE}" "${URL}"
mkdir -p "${work_dir}/unpacked"
if [[ "${ARCHIVE}" == *.zip ]]; then
  unzip -q "${work_dir}/${ARCHIVE}" -d "${work_dir}/unpacked"
else
  tar -xf "${work_dir}/${ARCHIVE}" -C "${work_dir}/unpacked"
fi
# (Both archives hold one folder, named after the release, with the toolchain in it. What a
# download cut short left behind is replaced.)
rm -rf "${DEST}"
mkdir -p "$(dirname "${DEST}")"
mv "${work_dir}"/unpacked/llvm-mingw-* "${DEST}"
echo "Windows toolchain ready in ${DEST}."
