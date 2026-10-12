#!/usr/bin/env bash
# Runs two copies of a game at once, each with its own system test: multiplayer, tested for real.
# Passes when both tests pass. With "relay" as the last word, a relay runs beside them for as long
# as they do. The relay is the engine's own program (redgem-relay); the games are run by the test
# tool.
# Every run keeps to itself, so that several can go on at once: a folder of its own among the
# temporary files (for the games' saves and the notes the two tests leave each other), and ports
# nobody else has. The tests read the ports from notes left here before they start: relay_port,
# discovery_port.
#   run_pair.sh <redgem-relay> <redgem-test> <game_dir> <first_test.rb> <second_test.rb> <screenshots_dir> [relay]
set -uo pipefail

RELAY="$1"
TEST_TOOL="$2"
GAME="$3"
FIRST_TEST="$4"
SECOND_TEST="$5"
SCREENSHOTS="$6"
WITH_RELAY="${7:-}"

FIRST_PORT=20000                         # ports are tried from here on: below the ones systems hand out
PORT_COUNT=10000
PORT_TRIES=20
RELAY_READY="relay listening on port"    # what the relay says once it has its port
READY_PAUSE=0.05                         # seconds between looks at what the relay said

WORK="$(mktemp -d "${TMPDIR:-/tmp}/redgem-pair.XXXXXX")" || exit 1
NOTES="${WORK}/notes"                    # where redgem-test --work keeps a run's notes
RELAY_LOG="${WORK}/relay.log"
mkdir -p "${NOTES}"
relay=""

cleanup() {
  [[ -n "${relay}" ]] && kill "${relay}" 2>/dev/null
  rm -rf "${WORK}"
}
trap cleanup EXIT

leave_note() { printf '%s' "$2" > "${NOTES}/$1"; }

candidate_port() { echo $(( FIRST_PORT + RANDOM % PORT_COUNT )); }

# Waits for the relay's word that it listens; fails when it ended without saying it (its port was taken).
relay_listening() {
  while kill -0 "${relay}" 2>/dev/null; do
    grep -q "${RELAY_READY}" "${RELAY_LOG}" && return 0
    sleep "${READY_PAUSE}"
  done
  return 1
}

# Starts the relay on a port nobody has. The relay itself is who finds out: it either gets the port
# and says so, or ends.
start_relay() {
  local try port
  for (( try = 0; try < PORT_TRIES; try++ )); do
    port="$(candidate_port)"
    "${RELAY}" --port "${port}" > "${RELAY_LOG}" 2>&1 &
    relay=$!
    if relay_listening; then
      leave_note relay_port "${port}"
      return 0
    fi
    relay=""
  done
  echo "no port for the relay in ${PORT_TRIES} tries"
  return 1
}

# A port no program on this machine waits for datagrams on (as far as `ss` tells, where there is one).
free_datagram_port() {
  local try port
  for (( try = 0; try < PORT_TRIES; try++ )); do
    port="$(candidate_port)"
    if ! command -v ss > /dev/null || [[ -z "$(ss -Huln "sport = :${port}")" ]]; then
      echo "${port}"
      return 0
    fi
  done
  return 1
}

if [[ "${WITH_RELAY}" == "relay" ]]; then
  start_relay || exit 1
fi
discovery_port="$(free_datagram_port)" || { echo "no port to look for games on"; exit 1; }
leave_note discovery_port "${discovery_port}"

"${TEST_TOOL}" "${GAME}" "${FIRST_TEST}" --screenshots "${SCREENSHOTS}" --work "${WORK}" &
first=$!
"${TEST_TOOL}" "${GAME}" "${SECOND_TEST}" --screenshots "${SCREENSHOTS}" --work "${WORK}"
second_status=$?
wait "${first}"
first_status=$?
[[ -f "${RELAY_LOG}" ]] && cat "${RELAY_LOG}"

echo "first test exited with ${first_status}, second with ${second_status}"
[ "${first_status}" -eq 0 ] && [ "${second_status}" -eq 0 ]
