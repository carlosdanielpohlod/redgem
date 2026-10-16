#!/usr/bin/env bash
# Runs a game on a dedicated server and two copies of it that join the server, each driven by its
# own system test: online play with nobody hosting, tested for real. Passes when both tests pass,
# the server closes their room once both left, and it stops cleanly when asked to (SIGTERM). A
# test that expects the server's game to keep a player's coins names that player (its identity)
# in a note, saved_identity: the server's save (a SQLite database, read with Python's sqlite3) must
# then hold one coin for it.
# The server is the engine's own program (redgem-server), run with no display, as on a machine
# that has none; the games are run by the test tool. Every run keeps to itself: a folder of its
# own among the temporary files (for the games' saves, the server's and the notes the tests leave
# each other), and a port the system picks for the server, which the tests read from a note
# (server_port).
#   run_with_server.sh <redgem-server> <redgem-test> <game_dir> <first_test.rb> <second_test.rb> <screenshots_dir>
set -uo pipefail

SERVER="$1"
TEST_TOOL="$2"
GAME="$3"
FIRST_TEST="$4"
SECOND_TEST="$5"
SCREENSHOTS="$6"

ANY_FREE_PORT=0
PLAYERS_PER_ROOM=2                       # both players fit in one room: quick play must put them together
LISTENING="game server listening on port "   # what the server says once it has its port
ROOM_CLOSED="closed: everyone left"      # ... once the players left their room
STOPPED="game server stopped"            # ... once it stopped
LOOK_PAUSE=0.05                          # seconds between looks at what the server said
MAX_LOOKS=600                            # 30 seconds of looking, for a server that never says it

WORK="$(mktemp -d "${TMPDIR:-/tmp}/redgem-server.XXXXXX")" || exit 1
NOTES="${WORK}/notes"                    # where redgem-test --work keeps a run's notes
LOG="${WORK}/server.log"
DATA="${WORK}/server-data"               # the server's data folder (--data): its game's save
SAVED_IDENTITY="${NOTES}/saved_identity"
SAVE="${DATA}/save.sqlite3"
mkdir -p "${NOTES}"
server=""

cleanup() {
  [[ -n "${server}" ]] && kill "${server}" 2>/dev/null
  rm -rf "${WORK}"
}
trap cleanup EXIT

# Waits for the server's word `text`; fails when it ended without saying it, or never does.
server_said() {
  local look
  for (( look = 0; look < MAX_LOOKS; look++ )); do
    grep -qF -- "$1" "${LOG}" && return 0
    kill -0 "${server}" 2>/dev/null || return 1
    sleep "${LOOK_PAUSE}"
  done
  return 1
}

env -u DISPLAY -u WAYLAND_DISPLAY "${SERVER}" "${GAME}" --port "${ANY_FREE_PORT}" \
  --max-players "${PLAYERS_PER_ROOM}" --data "${DATA}" > "${LOG}" 2>&1 &
server=$!
if ! server_said "${LISTENING}"; then
  cat "${LOG}"
  echo "the server never listened"
  exit 1
fi
port="$(sed -n "s/.*${LISTENING}\([0-9]*\).*/\1/p" "${LOG}" | head -1)"
printf '%s' "${port}" > "${NOTES}/server_port"

"${TEST_TOOL}" "${GAME}" "${FIRST_TEST}" --screenshots "${SCREENSHOTS}" --work "${WORK}" &
first=$!
"${TEST_TOOL}" "${GAME}" "${SECOND_TEST}" --screenshots "${SCREENSHOTS}" --work "${WORK}"
second_status=$?
wait "${first}"
first_status=$?

server_said "${ROOM_CLOSED}"
room_status=$?
kill -TERM "${server}"
wait "${server}"
server_status=$?
server=""
cat "${LOG}"
grep -qF -- "${STOPPED}" "${LOG}"
stopped_status=$?
saved_status=0
if [[ -f "${SAVED_IDENTITY}" ]]; then
  python3 -I "$(dirname "${BASH_SOURCE[0]}")/kept_coin.py" "${SAVE}" "$(cat "${SAVED_IDENTITY}")"
  saved_status=$?
fi

echo "first test exited with ${first_status}, second with ${second_status}; room closed: ${room_status};" \
  "server exited with ${server_status}, said it stopped: ${stopped_status}; kept the player's coin: ${saved_status}"
[ "${first_status}" -eq 0 ] && [ "${second_status}" -eq 0 ] && [ "${room_status}" -eq 0 ] &&
  [ "${server_status}" -eq 0 ] && [ "${stopped_status}" -eq 0 ] && [ "${saved_status}" -eq 0 ]
