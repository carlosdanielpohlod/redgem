#!/usr/bin/env bash
# Attaches the four downloads in a folder to the draft release of a tag, making the draft when
# there is none yet. It never publishes: the owner does, from the draft on GitHub. A release
# that is already published is never changed: the script fails instead. What the CI's release
# workflow runs (.github/workflows/release.yml), with gh signed in (GH_TOKEN) to the repository.
#   tools/release/attach_to_draft.sh <tag> <commit> <folder>
# (The commit is what a new tag points at when the draft is published; a tag that exists keeps
# its own.)
set -euo pipefail

# shellcheck source=tools/release/release_files.sh
source "$(dirname "${BASH_SOURCE[0]}")/release_files.sh"

NOT_FOUND_MARK="release not found"   # what gh says when a tag has no release
PRERELEASE_MARK="-"                  # v0.6.0-rc1: what follows the version names a pre-release

is_prerelease() { [[ "$1" == *"${PRERELEASE_MARK}"* ]]; }

main() {
  local tag="${1:?usage: attach_to_draft.sh <tag> <commit> <folder>}" commit="${2:?}" dir="${3:?}"
  local files=() download state error release_flags=()
  "${REPO_ROOT}/tools/release/check_downloads.sh" "${dir}"
  for download in "${DOWNLOADS[@]}"; do files+=("${dir}/${download}"); done

  # (A global: the trap runs after main has returned.)
  ERROR_FILE="$(mktemp)"
  trap 'rm -f "${ERROR_FILE}"' EXIT
  error="${ERROR_FILE}"
  if state="$(gh release view "${tag}" --json isDraft --jq .isDraft 2>"${error}")"; then
    if [[ "${state}" != "true" ]]; then
      echo "The release ${tag} is published already: the CI only changes drafts. Nothing was attached." >&2
      exit 1
    fi
    gh release upload "${tag}" "${files[@]}" --clobber
    echo "Attached the downloads to the draft release ${tag}."
  elif grep -Fq "${NOT_FOUND_MARK}" "${error}"; then
    # (A pre-release, such as v0.6.0-rc1, never becomes the latest release the README links to.)
    if is_prerelease "${tag}"; then release_flags+=(--prerelease); fi
    gh release create "${tag}" "${files[@]}" --draft --target "${commit}" ${release_flags[@]+"${release_flags[@]}"} \
      --title "RedGem ${tag#v}" --notes "What changed: see CHANGELOG.md."
    echo "Made the draft release ${tag} with the downloads."
  else
    cat "${error}" >&2
    exit 1
  fi
}

main "$@"
