#!/usr/bin/env bash
# Runs two copies of a game at once, each with its own system test: multiplayer, tested for real.
# Passes when both tests pass. With a relay port, a relay runs beside them for as long as they do.
# The relay is the engine's own program (redgem relay); the games are run by the test tool.
#   run_pair.sh <redgem> <redgem-test> <game_dir> <first_test.rb> <second_test.rb> <screenshots_dir> [relay_port]
set -uo pipefail

ENGINE="$1"
TEST_TOOL="$2"
GAME="$3"
FIRST_TEST="$4"
SECOND_TEST="$5"
SCREENSHOTS="$6"
RELAY_PORT="${7:-}"

# Where a test that opens a room leaves its code for the other: a code from an earlier run opens nothing.
rm -rf "${TMPDIR:-/tmp}/redgem-test-notes"
relay=""
if [[ -n "${RELAY_PORT}" ]]; then
  "${ENGINE}" relay --port "${RELAY_PORT}" &
  relay=$!
fi
"${TEST_TOOL}" "${GAME}" "${FIRST_TEST}" --screenshots "${SCREENSHOTS}" &
first=$!
"${TEST_TOOL}" "${GAME}" "${SECOND_TEST}" --screenshots "${SCREENSHOTS}"
second_status=$?
wait "${first}"
first_status=$?
[[ -n "${relay}" ]] && kill "${relay}" 2>/dev/null

echo "first test exited with ${first_status}, second with ${second_status}"
[ "${first_status}" -eq 0 ] && [ "${second_status}" -eq 0 ]
