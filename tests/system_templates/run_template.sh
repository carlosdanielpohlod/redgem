#!/usr/bin/env bash
# Makes a new game from a template, as `redgem new` does for anyone, and plays it with a system
# test. Passes when the game is created and the test passes.
#   run_template.sh <redgem> <redgem-test> <template> <test.rb> <screenshots_dir>
set -uo pipefail

REDGEM="$1"
TEST_TOOL="$2"
TEMPLATE="$3"
TEST_FILE="$4"
SCREENSHOTS="$5"
GAME_NAME="New Game"   # a name with a space and capitals: what a title and an id are made from

WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT

"${REDGEM}" new "${WORK}/${GAME_NAME}" --template "${TEMPLATE}" || exit 1
"${TEST_TOOL}" "${WORK}/${GAME_NAME}" "${TEST_FILE}" --screenshots "${SCREENSHOTS}" --work "${WORK}/run"
