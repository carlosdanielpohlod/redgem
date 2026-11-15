#!/usr/bin/env bash
# Unpacks a download of a release and says how its programs are run, for the scripts that run
# them (smoke_test.sh, smoke.sh) to source after release_files.sh; and how a program they run is
# expected to answer.

# (What the scripts that source this read: shellcheck sees no use of it here.)
# shellcheck disable=SC2034

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

# How each program is run, set by unpack: the editor, the relay, the game server and the test tool.
EDITOR_RUN=()
RELAY_RUN=()
SERVER_RUN=()
TESTER_RUN=()
# The download's unpacked folder (its demos and, on Linux, its bin/), and the folder its programs
# are in.
ROOT=""
BIN_DIR=""
PROGRAM_SUFFIX=""   # what the names of the download's programs end with (".exe" on Windows)

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
      BIN_DIR="${ROOT}/bin"
      export APPIMAGE_EXTRACT_AND_RUN=1
      EDITOR_RUN=("${work}/${name}")
      RELAY_RUN=("${work}/${name}" redgem-relay)
      SERVER_RUN=("${work}/${name}" redgem-server)
      TESTER_RUN=("${work}/${name}" redgem-test)
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
  BIN_DIR="${bin}"
  PROGRAM_SUFFIX="${suffix}"
  EDITOR_RUN=("${bin}/redgem${suffix}")
  RELAY_RUN=("${bin}/redgem-relay${suffix}")
  SERVER_RUN=("${bin}/redgem-server${suffix}")
  TESTER_RUN=("${bin}/redgem-test${suffix}")
}
