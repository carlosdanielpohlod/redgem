#!/usr/bin/env bash
# What every download of a release carries besides its programs, for the scripts that make them
# (package.sh, package_macos.sh) to source: the demos and the licenses, and the check that what
# is packed was built.

# (What the scripts that source this read: shellcheck sees no use of it here.)
# shellcheck disable=SC2034
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT_DIR="${REPO_ROOT}/dist/release"
PACKAGE_NAME="redgem"
# (The showcase is built from downloaded assets that may not be passed on.)
DEMOS=(platformer driving city)
NOTICES=(LICENSE THIRD_PARTY_LICENSES.md)

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
