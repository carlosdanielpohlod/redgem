#!/usr/bin/env bash
# Plays a game with one system test, then starts it again with another: what the first left behind
# (the game's save, the player's display settings, the players' bindings) is what the second
# starts with, as for a player who quits and comes back. Passes when both tests pass. The two runs
# share a folder of their own among the temporary files.
#   run_restart.sh <redgem-test> <game_dir> <first_test.rb> <second_test.rb> <screenshots_dir>
set -uo pipefail

TEST_TOOL="$1"
GAME="$2"
FIRST_TEST="$3"
SECOND_TEST="$4"
SCREENSHOTS="$5"

WORK="$(mktemp -d "${TMPDIR:-/tmp}/redgem-restart.XXXXXX")" || exit 1
trap 'rm -rf "${WORK}"' EXIT

"${TEST_TOOL}" "${GAME}" "${FIRST_TEST}" --screenshots "${SCREENSHOTS}" --work "${WORK}" || exit 1
"${TEST_TOOL}" "${GAME}" "${SECOND_TEST}" --screenshots "${SCREENSHOTS}" --work "${WORK}" --after "${FIRST_TEST}"
