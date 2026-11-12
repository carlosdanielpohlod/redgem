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

USAGE_MARK="usage:"   # how the relay and the server start their --help
ELF_MARK="ELF"        # what the first bytes of a Linux program say, after one byte
LINUX_TARGET="linux"

# How each program is run, set by unpack: the editor, the relay and the game server.
EDITOR_RUN=()
RELAY_RUN=()
SERVER_RUN=()
# The download's unpacked folder (its demos and, on Linux, its bin/).
ROOT=""

# Runs program $1 with the arguments after it and fails unless what it printed holds text $2.
expect_output() {
  local expected="$1" output
  shift
  output="$("$@" 2>&1 | tr -d '\r')" || { echo "$* failed:" >&2; echo "${output}" >&2; exit 1; }
  if [[ "${output}" != *"${expected}"* ]]; then
    echo "$* said \"${output}\", not \"${expected}\"" >&2
    exit 1
  fi
  echo "$*: ${output%%$'\n'*}"
}

# Runs the program and arguments given and fails unless it succeeds; prints its last line.
expect_success() {
  local output
  output="$("$@" 2>&1)" || { echo "$* failed:" >&2; echo "${output}" >&2; exit 1; }
  echo "$*: ${output##*$'\n'}"
}

# Unpacks download $1 (called $2) into folder $3 and says how its programs are run.
unpack() {
  local archive="$1" name="$2" work="$3" bin suffix=""
  ROOT="${work}/${PACKAGE_NAME}"
  case "${name}" in
    "${LINUX_DOWNLOAD}")
      tar -xzf "${archive}" -C "${work}"
      bin="${ROOT}/bin"
      ;;
    "${APPIMAGE_DOWNLOAD}")
      # (Unpacked too, for its demos and its programs' glibc; its programs are run through it.)
      install -m 755 "${archive}" "${work}/${name}"
      (cd "${work}" && "./${name}" --appimage-extract > /dev/null)
      ROOT="${work}/${APPIMAGE_UNPACKED}"
      export APPIMAGE_EXTRACT_AND_RUN=1
      EDITOR_RUN=("${work}/${name}")
      RELAY_RUN=("${work}/${name}" redgem-relay)
      SERVER_RUN=("${work}/${name}" redgem-server)
      return
      ;;
    "${WINDOWS_DOWNLOAD}")
      unzip -q "${archive}" -d "${work}"
      bin="${ROOT}"
      suffix=".exe"
      ;;
    "${MAC_ARM_DOWNLOAD}" | "${MAC_INTEL_DOWNLOAD}")
      # (ditto, as the Finder unpacks: it keeps the app's signature whole.)
      ditto -x -k "${archive}" "${work}"
      codesign --verify --strict --verbose "${ROOT}/RedGem.app"
      bin="${ROOT}/RedGem.app/Contents/MacOS"
      ;;
    *)
      echo "${name}: not a download of a release (they are: ${DOWNLOADS[*]})" >&2
      exit 1
      ;;
  esac
  EDITOR_RUN=("${bin}/redgem${suffix}")
  RELAY_RUN=("${bin}/redgem-relay${suffix}")
  SERVER_RUN=("${bin}/redgem-server${suffix}")
}

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
