#!/usr/bin/env bash
# Prints the state of a tag's release on GitHub: draft, published or missing. Fails (with what gh
# said) when it cannot tell. Needs gh signed in to the repository (GH_TOKEN, GH_REPO).
#   tools/release/release_state.sh v0.6.0
set -euo pipefail

NOT_FOUND_MARK="release not found"   # what gh says when a tag has no release

main() {
  local tag="${1:?usage: release_state.sh <tag>}" state output status=0
  output="$(gh release view "${tag}" --json isDraft --jq .isDraft 2>&1)" || status=$?
  if [[ "${status}" -ne 0 ]]; then
    if [[ "${output}" == *"${NOT_FOUND_MARK}"* ]]; then
      echo missing
      return
    fi
    echo "${output}" >&2
    exit 1
  fi
  state=published
  if [[ "${output}" == "true" ]]; then state=draft; fi
  echo "${state}"
}

main "$@"
