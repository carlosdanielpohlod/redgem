#!/usr/bin/env bash
# Runs two copies of a game at once, each with its own system test: multiplayer, tested for real.
# Passes when both tests pass. With a relay port, a relay runs beside them for as long as they do.
#   run_pair.sh <redgem> <game_dir> <first_test.rb> <second_test.rb> <screenshots_dir> [relay_port]
set -uo pipefail

ENGINE="$1"
GAME="$2"
FIRST_TEST="$3"
SECOND_TEST="$4"
SCREENSHOTS="$5"
RELAY_PORT="${6:-}"

# Where a test that opens a room leaves its code for the other: a code from an earlier run opens nothing.
rm -f /tmp/redgem_test_room_code.txt
relay=""
if [[ -n "${RELAY_PORT}" ]]; then
  "${ENGINE}" relay --port "${RELAY_PORT}" &
  relay=$!
fi
"${ENGINE}" "${GAME}" --test "${FIRST_TEST}" --screenshots "${SCREENSHOTS}" &
first=$!
"${ENGINE}" "${GAME}" --test "${SECOND_TEST}" --screenshots "${SCREENSHOTS}"
second_status=$?
wait "${first}"
first_status=$?
[[ -n "${relay}" ]] && kill "${relay}" 2>/dev/null

echo "first test exited with ${first_status}, second with ${second_status}"
[ "${first_status}" -eq 0 ] && [ "${second_status}" -eq 0 ]
