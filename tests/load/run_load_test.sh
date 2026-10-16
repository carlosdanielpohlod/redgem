#!/usr/bin/env bash
# Runs a game on a dedicated server (redgem-server, with no display) and the load test against it
# (see load_test.cpp): bots join one of its rooms and play while what it costs is measured. Passes
# when the load test does. The server keeps to a folder of its own among the temporary files and a
# port the system picks, and is stopped at the end.
#   run_load_test.sh <redgem-server> <redgem_load_test> <game_dir> [what the load test takes...]
set -uo pipefail

SERVER="$1"
LOAD_TEST="$2"
GAME="$3"
shift 3

ANY_FREE_PORT=0
PLAYERS_PER_ROOM=15                          # the most a room holds: every bot in one room
LISTENING="game server listening on port "   # what the server says once it has its port
LOOK_PAUSE=0.05                              # seconds between looks at what the server said
MAX_LOOKS=600                                # 30 seconds of looking, for a server that never says it

WORK="$(mktemp -d "${TMPDIR:-/tmp}/redgem-load.XXXXXX")" || exit 1
LOG="${WORK}/server.log"
server=""

cleanup() {
  [[ -n "${server}" ]] && kill "${server}" 2>/dev/null
  rm -rf "${WORK}"
}
trap cleanup EXIT

listening() {
  local look
  for (( look = 0; look < MAX_LOOKS; look++ )); do
    grep -qF -- "${LISTENING}" "${LOG}" && return 0
    kill -0 "${server}" 2>/dev/null || return 1
    sleep "${LOOK_PAUSE}"
  done
  return 1
}

env -u DISPLAY -u WAYLAND_DISPLAY "${SERVER}" "${GAME}" --port "${ANY_FREE_PORT}" \
  --max-players "${PLAYERS_PER_ROOM}" --data "${WORK}/data" > "${LOG}" 2>&1 &
server=$!
if ! listening; then
  cat "${LOG}"
  echo "the server never listened"
  exit 1
fi
port="$(sed -n "s/.*${LISTENING}\([0-9]*\).*/\1/p" "${LOG}" | head -1)"

"${LOAD_TEST}" --port "${port}" --server-pid "${server}" "$@"
status=$?
kill -TERM "${server}"
wait "${server}"
server=""
[ "${status}" -eq 0 ]
