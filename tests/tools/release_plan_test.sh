#!/usr/bin/env bash
# tools/release/plan_release.sh: which tag a run of the release workflow releases, and whether it
# attaches the downloads to a draft release or only keeps them as the run's artifacts.
#   release_plan_test.sh <tools/release folder>
set -euo pipefail

RELEASE_TOOLS="$1"
# shellcheck source=tools/release/release_files.sh
source "${RELEASE_TOOLS}/release_files.sh"
VERSION="$(engine_version)"
COMMIT="0123abc"
failures=0

fail() {
  echo "FAIL: $1" >&2
  failures=$((failures + 1))
}

# Runs the plan with the variables given as name=value ($2...) and checks its output holds $1,
# and that it succeeded (or, for expect_refusal, failed).
run_plan() {
  local outcome="$1" expected="$2" output status=0
  shift 2
  output="$(env -i PATH="${PATH}" COMMIT="${COMMIT}" "$@" \
    "${RELEASE_TOOLS}/plan_release.sh" 2>&1)" || status=$?
  [[ "${output}" == *"${expected}"* ]] || fail "$* expected \"${expected}\", got: ${output}"
  if [[ "${outcome}" == passes && "${status}" -ne 0 ]]; then fail "$* failed: ${output}"; fi
  if [[ "${outcome}" == fails && "${status}" -eq 0 ]]; then fail "$* passed: ${output}"; fi
  if [[ "${outcome}" == fails && "${output}" == *"attach="* ]]; then fail "$* still planned: ${output}"; fi
  return 0
}
expect_plan() { run_plan passes "$@"; }
expect_refusal() { run_plan fails "$@"; }

expect_plan $'tag=v'"${VERSION}"$'\ncommit='"${COMMIT}"$'\nattach=true' \
  EVENT=push REF_TYPE=tag REF_NAME="v${VERSION}"
expect_plan "tag=v${VERSION}-rc1" EVENT=push REF_TYPE=tag REF_NAME="v${VERSION}-rc1"
expect_refusal "is not version ${VERSION}" EVENT=push REF_TYPE=tag REF_NAME="v${VERSION}9"
expect_refusal "is not version ${VERSION}" EVENT=push REF_TYPE=tag REF_NAME="v0.0.1"

# A branch's push is never a release's.
expect_plan $'tag=v'"${VERSION}"$'\ncommit='"${COMMIT}"$'\nattach=false' EVENT=push REF_TYPE=branch REF_NAME=main

# Only a pushed tag attaches: a run started by hand only keeps the downloads as artifacts.
expect_plan $'tag=v'"${VERSION}"$'\ncommit='"${COMMIT}"$'\nattach=false' EVENT=workflow_dispatch

# A release pull request only keeps them too, and its branch must name the version.
expect_plan $'tag=v'"${VERSION}"$'\ncommit='"${COMMIT}"$'\nattach=false' \
  EVENT=pull_request HEAD_REF="release-${VERSION}"
expect_plan "attach=false" EVENT=pull_request HEAD_REF="release-${VERSION}-rc1"
expect_refusal "The branch release-0.0.1 is not version ${VERSION}" EVENT=pull_request HEAD_REF="release-0.0.1"
expect_refusal "is not version ${VERSION}" EVENT=pull_request HEAD_REF="release-${VERSION}9"

if [[ "${failures}" -gt 0 ]]; then
  echo "${failures} failure(s)" >&2
  exit 1
fi
echo "plan_release.sh: all cases pass"
