#!/usr/bin/env bash
# Unpacks a download on the system it is for and runs its programs as a user would first run them
# from a terminal: the editor says its version (the engine's, from CMakeLists.txt), and the relay
# and the game server their usage. On a Mac the app's signature is checked as well. A Linux
# download also checks a demo and builds it for Linux (both need no display), and the oldest glibc
# its programs and the built game run on is measured against what the docs say (glibc_floor.sh).
# The AppImage is run as a user runs it, through the runtime in front of its files, unpacking
# itself each time (APPIMAGE_EXTRACT_AND_RUN): a container has no FUSE.
#   tools/release/smoke_test.sh dist/release/redgem-linux-x86_64.tar.gz   # on Linux
#   tools/release/smoke_test.sh dist/release/redgem-linux-x86_64.AppImage # on Linux
#   tools/release/smoke_test.sh dist/release/redgem-windows-x86_64.zip    # on Windows (Git Bash)
#   tools/release/smoke_test.sh dist/release/redgem-macos-arm64.zip       # on a Mac of that kind
set -euo pipefail

# shellcheck source=tools/release/release_files.sh
source "$(dirname "${BASH_SOURCE[0]}")/release_files.sh"
# shellcheck source=tools/release/unpack_download.sh
source "$(dirname "${BASH_SOURCE[0]}")/unpack_download.sh"

USAGE_MARK="usage:"   # how the relay and the server start their --help
ELF_MARK="ELF"        # what the first bytes of a Linux program say, after one byte
LINUX_TARGET="linux"

is_linux_download() { [[ "$1" == "${LINUX_DOWNLOAD}" || "$1" == "${APPIMAGE_DOWNLOAD}" ]]; }

# Whether file $1 is a Linux program.
is_elf() { [[ "$(head -c 4 "$1" | tail -c 3)" == "${ELF_MARK}" ]]; }

# A demo checked and built for Linux by the editor (in folder $1), and the glibc the programs and
# the built game need: a game built for Linux is a copy of bin/redgem-player.
check_linux_download() {
  local work="$1" demo="${ROOT}/games/${DEMOS[0]}" built="$1/built" file programs=()
  expect_success "${EDITOR_RUN[@]}" check "${demo}"
  expect_success "${EDITOR_RUN[@]}" build "${demo}" "${built}" --target "${LINUX_TARGET}"
  for file in "${PROGRAMS[@]}"; do programs+=("${ROOT}/bin/${file}"); done
  while IFS= read -r file; do
    if is_elf "${file}"; then programs+=("${file}"); fi
  done < <(find "${built}" -maxdepth 1 -type f -perm -u+x)
  [[ ${#programs[@]} -gt ${#PROGRAMS[@]} ]] || { echo "the build of ${demo} made no program" >&2; exit 1; }
  "${REPO_ROOT}/tools/release/glibc_floor.sh" "${programs[@]}"
}

main() {
  local archive="${1:?usage: smoke_test.sh <download>}" name
  name="$(basename "${archive}")"
  # (A global: the trap runs after main has returned.)
  WORK_DIR="$(mktemp -d)"
  trap 'rm -rf "${WORK_DIR}"' EXIT
  unpack "${archive}" "${name}" "${WORK_DIR}"
  expect_output "RedGem $(engine_version)" "${EDITOR_RUN[@]}" --version
  expect_output "${USAGE_MARK}" "${RELAY_RUN[@]}" --help
  expect_output "${USAGE_MARK}" "${SERVER_RUN[@]}" --help
  if is_linux_download "${name}"; then check_linux_download "${WORK_DIR}"; fi
  echo "${name} runs."
}

main "$@"
