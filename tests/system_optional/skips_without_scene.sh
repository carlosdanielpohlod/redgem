#!/usr/bin/env bash
# Checks run_if_built.sh: a test whose scene is missing is not run and ends as "skipped", one
# whose scene is there is run and ends as the test does.
#   skips_without_scene.sh <run_if_built.sh> <the exit code that means skipped>
set -uo pipefail

RUN_IF_BUILT="$1"
SKIPPED="$2"
TEST_FAILED=3   # what the stand-in test below ends with

work="$(mktemp -d "${TMPDIR:-/tmp}/redgem-optional.XXXXXX")" || exit 1
trap 'rm -rf "${work}"' EXIT
touch "${work}/scene.glb"

fail() {
  echo "$1"
  exit 1
}

said="$("${RUN_IF_BUILT}" "${work}/missing.glb" touch "${work}/ran")"
status=$?
[[ "${status}" -eq "${SKIPPED}" ]] || fail "a missing scene ended with ${status}, not ${SKIPPED}"
[[ "${said}" == "skipped: ${work}/missing.glb"* ]] || fail "a missing scene was not named: ${said}"
[[ ! -e "${work}/ran" ]] || fail "the test ran without its scene"

"${RUN_IF_BUILT}" "${work}/scene.glb" touch "${work}/ran" || fail "a test with its scene did not pass"
[[ -e "${work}/ran" ]] || fail "the test did not run with its scene there"
"${RUN_IF_BUILT}" "${work}/scene.glb" bash -c "exit ${TEST_FAILED}"
[[ $? -eq "${TEST_FAILED}" ]] || fail "a failing test did not end as it failed"
echo "optional tests are skipped without their scene and run with it"
