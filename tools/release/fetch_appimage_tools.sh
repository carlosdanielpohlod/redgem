#!/usr/bin/env bash
# Downloads what packs the AppImage (package.sh) into .deps/appimage, both pinned by version and
# checksum: appimagetool, and the runtime that starts an AppImage (put in front of its files, given
# to appimagetool so that it downloads nothing itself). Prints where they are: appimagetool on the
# first line, the runtime on the second. A file whose checksum is not the pinned one is never kept.
#   tools/release/fetch_appimage_tools.sh
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DEST="${REPO_ROOT}/.deps/appimage"
TOOL_VERSION="1.9.1"
TOOL_SHA256="ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0"
TOOL_URL="https://github.com/AppImage/appimagetool/releases/download/${TOOL_VERSION}/appimagetool-x86_64.AppImage"
RUNTIME_RELEASE="20251108"
RUNTIME_SHA256="2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d"
RUNTIME_URL="https://github.com/AppImage/type2-runtime/releases/download/${RUNTIME_RELEASE}/runtime-x86_64"
# The names they are kept under.
APPIMAGETOOL="appimagetool-${TOOL_VERSION}-x86_64.AppImage"
RUNTIME="runtime-${RUNTIME_RELEASE}-x86_64"

has_checksum() { [[ -f "$1" ]] && echo "$2  $1" | sha256sum --check --status -; }

# Downloads $2 as ${DEST}/$1 unless it is there with checksum $3.
fetch() {
  local name="$1" url="$2" checksum="$3" partial
  if has_checksum "${DEST}/${name}" "${checksum}"; then return; fi
  partial="$(mktemp "${DEST}/.${name}.XXXXXX")"
  echo "Downloading ${url}..." >&2
  curl -fsSL --retry 3 -o "${partial}" "${url}"
  if ! has_checksum "${partial}" "${checksum}"; then
    rm -f "${partial}"
    echo "${url}: not the file pinned (sha256 ${checksum})" >&2
    exit 1
  fi
  chmod +x "${partial}"
  mv "${partial}" "${DEST}/${name}"
}

main() {
  mkdir -p "${DEST}"
  fetch "${APPIMAGETOOL}" "${TOOL_URL}" "${TOOL_SHA256}"
  fetch "${RUNTIME}" "${RUNTIME_URL}" "${RUNTIME_SHA256}"
  echo "${DEST}/${APPIMAGETOOL}"
  echo "${DEST}/${RUNTIME}"
}

main "$@"
