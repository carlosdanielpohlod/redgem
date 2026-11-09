#!/usr/bin/env bash
# Decides what a run of the release workflow (.github/workflows/release.yml) releases, from what
# started it, and prints it as the job's outputs (name=value lines):
#   tag      the release's tag: the one pushed, or v<the version in CMakeLists.txt>
#   commit   what the downloads are built from (a pull request's head, not its merge with main),
#            and what a new tag points at when the draft is published
#   attach   true only for a pushed tag: its downloads go to the tag's draft release. A release-*
#            pull request or a run started by hand only keeps them as the run's artifacts, so the
#            draft only ever holds downloads built from its tag.
# It reads what GitHub says about the run: EVENT (github.event_name), REF_TYPE, REF_NAME, COMMIT
# and HEAD_REF (a pull request's branch). It fails, before anything is built, on a pushed tag that
# is not v<version> or v<version>-<something>, and on a release-<version> branch whose version is
# not the one in CMakeLists.txt: CMakeLists.txt was not bumped.
#   EVENT=push REF_TYPE=tag REF_NAME=v0.6.0 COMMIT=<sha> tools/release/plan_release.sh
set -euo pipefail

# shellcheck source=tools/release/release_files.sh
source "$(dirname "${BASH_SOURCE[0]}")/release_files.sh"

TAG_PREFIX="v"
BRANCH_PREFIX="release-"

is_tag_push() { [[ "${EVENT}" == "push" && "${REF_TYPE:-}" == "tag" ]]; }
is_pull_request() { [[ "${EVENT}" == "pull_request" ]]; }

# Whether tag $1 names version $2: v0.6.0, or a pre-release of it such as v0.6.0-rc1.
tag_names_version() { [[ "$1" == "${TAG_PREFIX}$2" || "$1" == "${TAG_PREFIX}$2-"* ]]; }
# Whether branch $1 is the release branch of version $2: release-0.6.0 (or release-0.6.0-rc1).
branch_names_version() { [[ "$1" == "${BRANCH_PREFIX}$2" || "$1" == "${BRANCH_PREFIX}$2-"* ]]; }

refuse() {
  echo "$1 is not version $2, which CMakeLists.txt says: bump it first." >&2
  exit 1
}

main() {
  local version tag attach=false
  : "${EVENT:?}" "${COMMIT:?}"
  version="$(engine_version)"
  [[ -n "${version}" ]] || { echo "no version in CMakeLists.txt" >&2; exit 1; }
  tag="${TAG_PREFIX}${version}"
  if is_tag_push; then
    tag="${REF_NAME:?}"
    tag_names_version "${tag}" "${version}" || refuse "The tag ${tag}" "${version}"
    attach=true
  fi
  if is_pull_request; then
    branch_names_version "${HEAD_REF:?}" "${version}" || refuse "The branch ${HEAD_REF}" "${version}"
  fi
  echo "tag=${tag}"
  echo "commit=${COMMIT}"
  echo "attach=${attach}"
}

main "$@"
