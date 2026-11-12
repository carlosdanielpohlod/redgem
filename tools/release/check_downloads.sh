#!/usr/bin/env bash
# Checks that the downloads of a release are all there and hold what they must: the programs, the
# players for other systems (in the Linux one), the demos and the licenses; and that the players'
# symbols (redgem-symbols-<system>.zip) hold each player as it was linked. It only reads the
# archives' lists of files, so it checks the downloads of every system on any machine.
#   tools/release/check_downloads.sh dist/release                  # all four downloads and the symbols
#   tools/release/check_downloads.sh dist/release redgem-macos-arm64.zip   # only the ones named
# Every problem is printed before it fails, so one missing download does not hide another.
set -euo pipefail

# shellcheck source=tools/release/release_files.sh
source "$(dirname "${BASH_SOURCE[0]}")/release_files.sh"

problems=0

problem() {
  echo "  $1" >&2
  problems=$((problems + 1))
}

# What archive $1 lists, one file per line.
list_archive() {
  case "$1" in
    *.tar.gz) tar -tzf "$1" ;;
    *.zip) unzip -Z1 "$1" ;;
  esac
}

# What download $1 must hold, one path per line (inside the archive).
required_files() {
  local root="${PACKAGE_NAME}" bin program runtime demo notice
  case "$1" in
    "${LINUX_DOWNLOAD}")
      bin="${root}/bin"
      for program in "${PROGRAMS[@]}"; do echo "${bin}/${program}"; done
      for runtime in "${WINDOWS_RUNTIME}" "${ANDROID_RUNTIMES[@]}"; do echo "${bin}/${runtime}"; done
      ;;
    "${WINDOWS_DOWNLOAD}")
      for program in "${WINDOWS_PROGRAMS[@]}"; do echo "${root}/${program}"; done
      ;;
    "${MAC_ARM_DOWNLOAD}" | "${MAC_INTEL_DOWNLOAD}")
      bin="${root}/RedGem.app/Contents/MacOS"
      echo "${root}/RedGem.app/Contents/Info.plist"
      for program in "${PROGRAMS[@]}"; do echo "${bin}/${program}"; done
      ;;
  esac
  for demo in "${DEMOS[@]}"; do echo "${root}/games/${demo}/${DEMO_FILE}"; done
  for notice in "${NOTICES[@]}" README.txt; do echo "${root}/${notice}"; done
}

# What symbols archive $1 must hold.
required_symbols() {
  local root="${SYMBOLS_PACKAGE}" abi
  case "$1" in
    "${LINUX_SYMBOLS}") echo "${root}/${LINUX_PLAYER_SYMBOLS}" ;;
    "${WINDOWS_SYMBOLS}") echo "${root}/${WINDOWS_PLAYER_SYMBOLS}" ;;
    "${ANDROID_SYMBOLS}")
      for abi in "${ANDROID_ABIS[@]}"; do echo "${root}/android/${abi}/${ANDROID_PLAYER}"; done
      ;;
  esac
}

is_symbols() {
  local symbols
  for symbols in "${SYMBOL_DOWNLOADS[@]}"; do
    [[ "$1" == "${symbols}" ]] && return 0
  done
  return 1
}

# What asset $1 of a release must hold.
required() {
  if is_symbols "$1"; then required_symbols "$1"; else required_files "$1"; fi
}

is_known_download() {
  local download
  for download in "${RELEASE_ASSETS[@]}"; do
    [[ "$1" == "${download}" ]] && return 0
  done
  return 1
}

check_download() {
  local dir="$1" name="$2" listing file
  if ! is_known_download "${name}"; then
    problem "${name}: not a download of a release (they are: ${RELEASE_ASSETS[*]})"
    return
  fi
  if [[ ! -s "${dir}/${name}" ]]; then
    problem "${name}: missing from ${dir}"
    return
  fi
  if ! listing="$(list_archive "${dir}/${name}")"; then
    problem "${name}: not a readable archive"
    return
  fi
  while IFS= read -r file; do
    grep -Fxq -- "${file}" <<<"${listing}" || problem "${name}: has no ${file}"
  done < <(required "${name}")
}

main() {
  local dir="${1:?usage: check_downloads.sh <folder> [download...]}" name
  shift
  local names=("$@")
  [[ ${#names[@]} -gt 0 ]] || names=("${RELEASE_ASSETS[@]}")
  for name in "${names[@]}"; do
    check_download "${dir}" "${name}"
  done
  if [[ "${problems}" -gt 0 ]]; then
    echo "The downloads in ${dir} are not ready for a release: ${problems} problem(s), above." >&2
    exit 1
  fi
  echo "The downloads in ${dir} are complete: ${names[*]}"
}

main "$@"
