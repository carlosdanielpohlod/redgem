#!/usr/bin/env bash
# Unpacks a download on the system it is for and runs its programs as a user would first run them
# from a terminal: the editor says its version (the engine's, from CMakeLists.txt), and the relay
# and the game server their usage. On a Mac the app's signature is checked as well.
#   tools/release/smoke_test.sh dist/release/redgem-linux-x86_64.tar.gz   # on Linux
#   tools/release/smoke_test.sh dist/release/redgem-windows-x86_64.zip    # on Windows (Git Bash)
#   tools/release/smoke_test.sh dist/release/redgem-macos-arm64.zip       # on a Mac of that kind
set -euo pipefail

# shellcheck source=tools/release/release_files.sh
source "$(dirname "${BASH_SOURCE[0]}")/release_files.sh"

USAGE_MARK="usage:"   # how the relay and the server start their --help

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

main() {
  local archive="${1:?usage: smoke_test.sh <download>}" name bin suffix="" work
  name="$(basename "${archive}")"
  # (A global: the trap runs after main has returned.)
  WORK_DIR="$(mktemp -d)"
  trap 'rm -rf "${WORK_DIR}"' EXIT
  work="${WORK_DIR}"
  case "${name}" in
    "${LINUX_DOWNLOAD}")
      tar -xzf "${archive}" -C "${work}"
      bin="${work}/${PACKAGE_NAME}/bin"
      ;;
    "${WINDOWS_DOWNLOAD}")
      unzip -q "${archive}" -d "${work}"
      bin="${work}/${PACKAGE_NAME}"
      suffix=".exe"
      ;;
    "${MAC_ARM_DOWNLOAD}" | "${MAC_INTEL_DOWNLOAD}")
      # (ditto, as the Finder unpacks: it keeps the app's signature whole.)
      ditto -x -k "${archive}" "${work}"
      codesign --verify --strict --verbose "${work}/${PACKAGE_NAME}/RedGem.app"
      bin="${work}/${PACKAGE_NAME}/RedGem.app/Contents/MacOS"
      ;;
    *)
      echo "${name}: not a download of a release (they are: ${DOWNLOADS[*]})" >&2
      exit 1
      ;;
  esac
  expect_output "RedGem $(engine_version)" "${bin}/redgem${suffix}" --version
  expect_output "${USAGE_MARK}" "${bin}/redgem-relay${suffix}" --help
  expect_output "${USAGE_MARK}" "${bin}/redgem-server${suffix}" --help
  echo "${name} runs."
}

main "$@"
