#!/usr/bin/env bash
# Runs two copies of a game at once, each with its own system test: multiplayer, tested for real.
# Passes when both tests pass.
#   run_pair.sh <redgem> <game_dir> <first_test.rb> <second_test.rb> <screenshots_dir>
set -uo pipefail

ENGINE="$1"
GAME="$2"
FIRST_TEST="$3"
SECOND_TEST="$4"
SCREENSHOTS="$5"

"${ENGINE}" "${GAME}" --test "${FIRST_TEST}" --screenshots "${SCREENSHOTS}" &
first=$!
"${ENGINE}" "${GAME}" --test "${SECOND_TEST}" --screenshots "${SCREENSHOTS}"
second_status=$?
wait "${first}"
first_status=$?

echo "first test exited with ${first_status}, second with ${second_status}"
[ "${first_status}" -eq 0 ] && [ "${second_status}" -eq 0 ]
