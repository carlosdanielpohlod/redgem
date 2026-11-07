#!/usr/bin/env bash
# Decides what a run of the release workflow (.github/workflows/release.yml) releases, from what
# started it, and prints it as the job's outputs (name=value lines):
#   tag      the release's tag: the one pushed, or v<the version in CMakeLists.txt>
#   commit   what a new tag points at when the draft is published
#   attach   true when the downloads go to the tag's draft release, false when they are only kept
#            as the run's artifacts (a dry run, or a pull request from a fork, whose token cannot
#            write)
# It reads what GitHub says about the run: EVENT (github.event_name), REF_TYPE, REF_NAME, COMMIT,
# DRY_RUN (the dispatch's input), HEAD_REPO (a pull request's) and GITHUB_REPOSITORY. A pushed tag
# that is not v<version> or v<version>-<something> fails: CMakeLists.txt was not bumped.
#   EVENT=push REF_TYPE=tag REF_NAME=v0.6.0 COMMIT=<sha> tools/release/plan_release.sh
set -euo pipefail

# shellcheck source=tools/release/release_files.sh
source "$(dirname "${BASH_SOURCE[0]}")/release_files.sh"

TAG_PREFIX="v"

is_tag_push() { [[ "${EVENT}" == "push" && "${REF_TYPE:-}" == "tag" ]]; }
is_dispatch() { [[ "${EVENT}" == "workflow_dispatch" ]]; }
is_dry_run() { [[ "${DRY_RUN:-true}" == "true" ]]; }
is_pull_request() { [[ "${EVENT}" == "pull_request" ]]; }
is_own_pull_request() { [[ "${HEAD_REPO:-}" == "${GITHUB_REPOSITORY:-}" ]]; }

# Whether tag $1 names version $2: v0.6.0, or a pre-release of it such as v0.6.0-rc1.
tag_names_version() { [[ "$1" == "${TAG_PREFIX}$2" || "$1" == "${TAG_PREFIX}$2-"* ]]; }

is_real_dispatch() { is_dispatch && ! is_dry_run; }
is_own_release_pull_request() { is_pull_request && is_own_pull_request; }

should_attach() { is_tag_push || is_real_dispatch || is_own_release_pull_request; }

main() {
  local version tag attach=false
  : "${EVENT:?}" "${COMMIT:?}"
  version="$(engine_version)"
  [[ -n "${version}" ]] || { echo "no version in CMakeLists.txt" >&2; exit 1; }
  tag="${TAG_PREFIX}${version}"
  if is_tag_push; then
    tag="${REF_NAME:?}"
    if ! tag_names_version "${tag}" "${version}"; then
      echo "The tag ${tag} is not version ${version}, which CMakeLists.txt says: bump it first." >&2
      exit 1
    fi
  fi
  if should_attach; then attach=true; fi
  echo "tag=${tag}"
  echo "commit=${COMMIT}"
  echo "attach=${attach}"
}

main "$@"
