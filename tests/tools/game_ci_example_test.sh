#!/usr/bin/env bash
# The CI example every new game gets (templates/_shared/.github/workflows/test.yml): it downloads
# the Linux download a release has (the name release_files.sh gives it), runs the editor where
# that download keeps it, and checks the game with its tests. The manual (docs/testing.md) shows
# the file as it is.
#   game_ci_example_test.sh <repository root>
set -euo pipefail

REPO="$1"
# shellcheck source=tools/release/release_files.sh
source "${REPO}/tools/release/release_files.sh"
WORKFLOW="${REPO}/templates/_shared/.github/workflows/test.yml"
MANUAL="${REPO}/docs/testing.md"
LATEST_DOWNLOADS="https://github.com/carlosdanielpohlod/redgem/releases/latest/download"
failures=0

fail() {
  echo "FAIL: $1" >&2
  failures=$((failures + 1))
}

# Fails unless the workflow says text $1 ($2 says what that is).
expect_in_workflow() {
  grep -qF -- "$1" "${WORKFLOW}" || fail "the workflow does not have ${2}: $1"
}

expect_in_workflow "${LATEST_DOWNLOADS}/${LINUX_DOWNLOAD}" "the latest release's Linux download"
expect_in_workflow "/${PACKAGE_NAME}/bin/redgem\" check" "the editor where the download keeps it"
expect_in_workflow "--run-tests" "the game's tests"

workflow="$(cat "${WORKFLOW}")"
manual="$(cat "${MANUAL}")"
[[ "${manual}" == *"${workflow}"* ]] || fail "docs/testing.md does not show the workflow as it is"

if [[ "${failures}" -gt 0 ]]; then
  echo "${failures} failure(s)" >&2
  exit 1
fi
echo "the CI example: all cases pass"
