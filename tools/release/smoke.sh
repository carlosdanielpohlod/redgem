#!/usr/bin/env bash
# Opens windows from a download of a release, as its user would, before the release is published:
# a demo built and played by the player it carries, a new game's tests run (redgem check
# --run-tests), one system test played in a window, and the editor opened on a demo. The player
# and the editor close themselves after a few frames (REDGEM_CLOSE_AFTER_FRAMES, which every
# window reads) and must end well; the system test must leave its picture. smoke_test.sh needs no
# display; this one does: a graphics card, or a software one (Mesa's llvmpipe under Xvfb on Linux,
# Mesa's opengl32.dll beside the programs on Windows, which --gl puts there).
#   tools/release/smoke.sh dist/release/redgem-linux-x86_64.tar.gz          # Linux, with a display
#   tools/release/smoke.sh dist/release/redgem-linux-x86_64.AppImage
#   xvfb-run -a tools/release/smoke.sh dist/release/redgem-linux-x86_64.tar.gz # with no display
#   tools/release/smoke.sh --gl <folder of opengl32.dll> dist/release/redgem-windows-x86_64.zip
# What the programs save goes to a folder of the run's own (XDG_DATA_HOME, XDG_CONFIG_HOME), which
# is deleted with the rest of what it unpacked.
set -euo pipefail

# shellcheck source=tools/release/release_files.sh
source "$(dirname "${BASH_SOURCE[0]}")/release_files.sh"
# shellcheck source=tools/release/unpack_download.sh
source "$(dirname "${BASH_SOURCE[0]}")/unpack_download.sh"

CLOSE_AFTER_FRAMES=120     # what the player and the editor show before they close themselves
WINDOW_SECONDS=300         # the longest a window may stay open (a software card starts slowly)
TEMPLATE="third-person"    # a template whose game comes with tests of its own
SMOKE_GAME="smoke_game"
WINDOW_TEST="${REPO_ROOT}/tools/release/smoke_window_test.rb"
WINDOW_PICTURE="smoke_window.png"   # what that test takes (t.screenshot("smoke_window"))
GL_OPTION="--gl"
GL_LIBRARIES=(opengl32.dll libgallium_wgl.dll)   # what --gl copies beside every program

GL_DIR=""   # --gl: Mesa's libraries, copied beside the Windows programs

say() { echo "smoke: $*"; }

# Copies the software OpenGL of --gl, if any, into folder $1.
add_gl() {
  [[ -n "${GL_DIR}" ]] || return 0
  local library
  for library in "${GL_LIBRARIES[@]}"; do
    [[ -f "${GL_DIR}/${library}" ]] || { echo "${GL_OPTION} ${GL_DIR}: has no ${library}" >&2; exit 1; }
    cp "${GL_DIR}/${library}" "$1/"
  done
}

# The program a build of game folder $1 made in folder $2: named after the game's folder.
built_program() {
  local program
  program="$2/$(basename "$1")${PROGRAM_SUFFIX}"
  [[ -x "${program}" ]] || { echo "the build in $2 made no ${program}" >&2; exit 1; }
  echo "${program}"
}

# Runs a program that opens a window ($@) until it closes itself, and fails unless it ends well.
expect_window_closes() {
  REDGEM_CLOSE_AFTER_FRAMES="${CLOSE_AFTER_FRAMES}" expect_success timeout "${WINDOW_SECONDS}" "$@"
}

# A demo, built by the editor for this system and played by the download's player.
play_built_demo() {
  local demo="$1" built="$2" program
  expect_success "${EDITOR_RUN[@]}" build "${demo}" "${built}"
  add_gl "${built}"
  program="$(built_program "${demo}" "${built}")"
  say "playing ${program}"
  expect_window_closes "${program}"
}

# A new game's own tests, with no window, then one system test in a window, which leaves a picture.
run_tests() {
  local game="$1" shots="$2"
  expect_success "${EDITOR_RUN[@]}" new "${game}" --template "${TEMPLATE}"
  expect_success "${EDITOR_RUN[@]}" check "${game}" --run-tests
  expect_success timeout "${WINDOW_SECONDS}" "${TESTER_RUN[@]}" "${game}" "${WINDOW_TEST}" --screenshots "${shots}"
  [[ -s "${shots}/${WINDOW_PICTURE}" ]] || { echo "the system test left no ${WINDOW_PICTURE}" >&2; exit 1; }
}

read_options() {
  if [[ "${1:-}" == "${GL_OPTION}" ]]; then
    GL_DIR="${2:?${GL_OPTION} needs a folder}"
    shift 2
  fi
  ARCHIVE="${1:?usage: smoke.sh [${GL_OPTION} <folder>] <download>}"
}

main() {
  read_options "$@"
  local name demo
  name="$(basename "${ARCHIVE}")"
  # (A global: the trap runs after main has returned.)
  WORK_DIR="$(mktemp -d)"
  trap 'rm -rf "${WORK_DIR}"' EXIT
  export XDG_DATA_HOME="${WORK_DIR}/data" XDG_CONFIG_HOME="${WORK_DIR}/config"
  unpack "${ARCHIVE}" "${name}" "${WORK_DIR}"
  add_gl "${BIN_DIR}"
  demo="${ROOT}/games/${DEMOS[0]}"
  play_built_demo "${demo}" "${WORK_DIR}/built"
  run_tests "${WORK_DIR}/${SMOKE_GAME}" "${WORK_DIR}/shots"
  say "opening the editor on ${demo}"
  expect_window_closes "${EDITOR_RUN[@]}" "${demo}"
  echo "${name} opens its windows."
}

main "$@"
