#!/usr/bin/env bash
# Attaches the four downloads in a folder to the draft release of a tag, making the draft when
# there is none yet. What the CI's release workflow runs on a pushed tag
# (.github/workflows/release.yml), with gh signed in (GH_TOKEN, GH_REPO) to the repository.
#   tools/release/attach_to_draft.sh <tag> <commit> <folder>
# (The commit is what a new tag points at when the draft is published; a tag that exists keeps
# its own.)
#
# It never publishes, and never changes a published release. The draft never holds fewer than
# four downloads because of it: the new ones go up under other names first (incoming.<name>),
# are checked (all four, at the sizes packed), and only then, the release checked again to still
# be a draft, take the place of the old ones. A run that fails on the way leaves the old ones.
set -euo pipefail

# shellcheck source=tools/release/release_files.sh
source "$(dirname "${BASH_SOURCE[0]}")/release_files.sh"

STATE="${REPO_ROOT}/tools/release/release_state.sh"
STAGING_PREFIX="incoming."           # the name a new download goes up under, before its swap
PRERELEASE_MARK="-"                  # v0.6.0-rc1: what follows the version names a pre-release
ASSETS_ENDPOINT="repos/{owner}/{repo}/releases/assets"

is_prerelease() { [[ "$1" == *"${PRERELEASE_MARK}"* ]]; }

# The release's assets, one per line: name, size and numeric id, separated by tabs.
list_assets() {
  gh release view "$1" --json assets --jq '.assets[] | [.name, .size, (.apiUrl | split("/") | last)] | @tsv'
}

# The id of asset $2 of release $1, or nothing when it has none of that name.
asset_id() {
  local name size id
  while IFS=$'\t' read -r name size id; do
    if [[ "${name}" == "$2" ]]; then echo "${id}"; fi
  done < <(list_assets "$1")
}

# Fails, removing what this run uploaded, unless the release is still a draft.
require_draft() {
  local tag="$1" moment="$2" state
  state="$("${STATE}" "${tag}")"
  if [[ "${state}" != "draft" ]]; then
    echo "The release ${tag} is ${state} ${moment}: the CI only changes drafts. Nothing was replaced." >&2
    remove_staged "${tag}"
    exit 1
  fi
}

remove_staged() {
  local download id
  for download in "${DOWNLOADS[@]}"; do
    id="$(asset_id "$1" "${STAGING_PREFIX}${download}" 2>/dev/null || true)"
    if [[ -n "${id}" ]]; then gh api -X DELETE "${ASSETS_ENDPOINT}/${id}" >/dev/null || true; fi
  done
}

# Makes the draft when the tag has no release. (Another run making it at the same moment is fine.)
ensure_draft() {
  local tag="$1" commit="$2" state flags=()
  state="$("${STATE}" "${tag}")"
  if [[ "${state}" == "published" ]]; then
    echo "The release ${tag} is published already: the CI only changes drafts. Nothing was attached." >&2
    exit 1
  fi
  if [[ "${state}" == "missing" ]]; then
    # (A pre-release never becomes the latest release the README's links serve.)
    if is_prerelease "${tag}"; then flags+=(--prerelease); fi
    gh release create "${tag}" --draft --target "${commit}" ${flags[@]+"${flags[@]}"} \
      --title "RedGem ${tag#v}" --notes "What changed: see CHANGELOG.md." ||
      [[ "$("${STATE}" "${tag}")" == "draft" ]]
    echo "Made the draft release ${tag}."
  fi
}

# Fails unless release $1 holds every download of folder $2 under prefix $3, at its size.
verify_assets() {
  local tag="$1" dir="$2" prefix="$3" listing download size problems=0
  listing="$(list_assets "${tag}")"
  for download in "${DOWNLOADS[@]}"; do
    size="$(wc -c < "${dir}/${download}" | tr -d ' ')"
    if ! has_asset "${listing}" "${prefix}${download}" "${size}"; then
      echo "  ${prefix}${download}: not on the release ${tag} at ${size} bytes" >&2
      problems=$((problems + 1))
    fi
  done
  [[ "${problems}" -eq 0 ]]
}

# Whether listing $1 (as list_assets prints it) has asset $2 of size $3.
has_asset() {
  local name size id
  while IFS=$'\t' read -r name size id; do
    if [[ "${name}" == "$2" && "${size}" == "$3" ]]; then return 0; fi
  done <<<"$1"
  return 1
}

main() {
  local tag="${1:?usage: attach_to_draft.sh <tag> <commit> <folder>}" commit="${2:?}" dir="${3:?}"
  local download staged=() old new
  "${REPO_ROOT}/tools/release/check_downloads.sh" "${dir}"
  ensure_draft "${tag}" "${commit}"

  # (A global: the trap runs after main has returned.)
  WORK_DIR="$(mktemp -d)"
  trap 'rm -rf "${WORK_DIR}"' EXIT
  for download in "${DOWNLOADS[@]}"; do
    cp "${dir}/${download}" "${WORK_DIR}/${STAGING_PREFIX}${download}"
    staged+=("${WORK_DIR}/${STAGING_PREFIX}${download}")
  done

  require_draft "${tag}" "before the upload"
  gh release upload "${tag}" "${staged[@]}" --clobber
  if ! verify_assets "${tag}" "${dir}" "${STAGING_PREFIX}"; then
    echo "The upload to ${tag} is incomplete: the old downloads stay." >&2
    remove_staged "${tag}"
    exit 1
  fi

  require_draft "${tag}" "before the swap"
  for download in "${DOWNLOADS[@]}"; do
    old="$(asset_id "${tag}" "${download}")"
    new="$(asset_id "${tag}" "${STAGING_PREFIX}${download}")"
    if [[ -n "${old}" ]]; then gh api -X DELETE "${ASSETS_ENDPOINT}/${old}" >/dev/null; fi
    gh api -X PATCH "${ASSETS_ENDPOINT}/${new}" -f name="${download}" >/dev/null
  done
  verify_assets "${tag}" "${dir}" ""
  echo "Attached the downloads to the draft release ${tag}."
}

main "$@"
