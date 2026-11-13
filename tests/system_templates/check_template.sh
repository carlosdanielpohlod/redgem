#!/usr/bin/env bash
# Makes a new game from a template, as `redgem new` does for anyone, and runs the tests the game
# comes with, as its developer (or the CI example, .github/workflows/test.yml) runs them:
# `redgem check <game> --run-tests`. Passes when the check passes and ran at least one test.
#   check_template.sh <redgem> <template>
set -uo pipefail

REDGEM="$1"
TEMPLATE="$2"
GAME_NAME="New Game"   # a name with a space and capitals, as run_template.sh makes
# What the check says of the tests when every one of them passed (and there was one).
ALL_PASSED='^tests: [1-9][0-9]* passed, 0 failed$'

WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT

"${REDGEM}" new "${WORK}/${GAME_NAME}" --template "${TEMPLATE}" || exit 1
said="$("${REDGEM}" check "${WORK}/${GAME_NAME}" --run-tests)"
status=$?
echo "${said}"
if [[ "${status}" -ne 0 ]]; then
  echo "the check of a new ${TEMPLATE} game failed (exit code ${status})" >&2
  exit 1
fi
if ! grep -Eq "${ALL_PASSED}" <<< "${said}"; then
  echo "the check of a new ${TEMPLATE} game ran none of its tests" >&2
  exit 1
fi
