#!/usr/bin/env bash
# Runs a game on a dedicated server (redgem-server, with no display) with K rooms and a status
# port, and the server load test against it (see server_load.cpp): M = K x players-per-room bots
# fill its rooms and play for T seconds. Passes when the load test does; its results, with --out,
# are for tools/bench/check_thresholds.py. The server keeps to a folder of its own among the
# temporary files and to ports the system picks, and is stopped at the end.
#   run_server_load.sh <redgem-server> <redgem_server_load> <game_dir> <rooms> <players_per_room> <seconds> [--out <file>]
set -uo pipefail

SERVER="$1"
LOAD="$2"
GAME="$3"
ROOMS="$4"
PLAYERS_PER_ROOM="$5"
SECONDS_PLAYED="$6"
shift 6

ANY_FREE_PORT=0
LISTENING="game server listening on port "   # what the server says once it has its port
STATUS="status on "                          # what it says once its status port is open
LOOK_PAUSE=0.05                              # seconds between looks at what the server said
MAX_LOOKS=600                                # 30 seconds of looking, for a server that never says it

CLIENTS=$(( ROOMS * PLAYERS_PER_ROOM ))
WORK="$(mktemp -d "${TMPDIR:-/tmp}/redgem-server-load.XXXXXX")" || exit 1
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

# Every bot connects from this machine's address: the server lets all of them in.
env -u DISPLAY -u WAYLAND_DISPLAY "${SERVER}" "${GAME}" --port "${ANY_FREE_PORT}" \
  --rooms "${ROOMS}" --max-players "${PLAYERS_PER_ROOM}" --per-address "${CLIENTS}" \
  --status-port "${ANY_FREE_PORT}" --data "${WORK}/data" > "${LOG}" 2>&1 &
server=$!
if ! listening; then
  cat "${LOG}"
  echo "the server never listened"
  exit 1
fi
port="$(sed -n "s/.*${LISTENING}\([0-9]*\).*/\1/p" "${LOG}" | head -1)"
status_port="$(sed -n "s/.*${STATUS}[0-9.]*:\([0-9]*\).*/\1/p" "${LOG}" | head -1)"

"${LOAD}" --port "${port}" --status-port "${status_port}" --server-pid "${server}" \
  --clients "${CLIENTS}" --rooms "${ROOMS}" --seconds "${SECONDS_PLAYED}" "$@"
status=$?
kill -TERM "${server}"
wait "${server}"
server=""
if [ "${status}" -ne 0 ]; then
  echo "== the server's log"
  cat "${LOG}"
fi
[ "${status}" -eq 0 ]
