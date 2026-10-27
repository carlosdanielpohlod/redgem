#!/usr/bin/env bash
# Downloads the compiler that builds RedGem for Android from Linux (the Android NDK, a pinned
# release) into .deps/android-ndk. No root access needed.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RELEASE="r27c"
ARCHIVE="android-ndk-${RELEASE}-linux.zip"
URL="https://dl.google.com/android/repository/${ARCHIVE}"
# The archive's SHA-256: the download is refused unless it is exactly this release. (Google lists
# its SHA-1, 090e8083a715fdb1a3e402d0763c388abb03fb4e, which this archive has as well.)
SHA256="59c2f6dc96743b5daf5d1626684640b20a6bd2b1d85b13156b90333741bad5cc"
DEST="${REPO_ROOT}/.deps/android-ndk"

if [[ -f "${DEST}/build/cmake/android.toolchain.cmake" ]]; then
  echo "Android toolchain already in ${DEST}."
  exit 0
fi

work_dir="$(mktemp -d)"
trap 'rm -rf "${work_dir}"' EXIT
echo "Downloading ${ARCHIVE}..."
curl -fL --retry 3 -o "${work_dir}/${ARCHIVE}" "${URL}"
echo "${SHA256}  ${work_dir}/${ARCHIVE}" | sha256sum --check --quiet
unzip -q "${work_dir}/${ARCHIVE}" -d "${work_dir}"
mkdir -p "$(dirname "${DEST}")"
mv "${work_dir}/android-ndk-${RELEASE}" "${DEST}"
echo "Android toolchain ready in ${DEST}."
