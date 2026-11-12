#!/usr/bin/env bash
# Checks that the downloads of a release are all there and hold what they must: the programs, the
# players for other systems (in the Linux ones), the demos and the licenses. It reads the
# archives' lists of files, so it checks the downloads of every system on any machine; the
# AppImage lists its files by unpacking itself, which takes a Linux x86_64 machine.
#   tools/release/check_downloads.sh dist/release                  # all five downloads
#   tools/release/check_downloads.sh dist/release redgem-macos-arm64.zip   # only the ones named
# Every problem is printed before it fails, so one missing download does not hide another.
set -euo pipefail

# shellcheck source=tools/release/release_files.sh
source "$(dirname "${BASH_SOURCE[0]}")/release_files.sh"

APPIMAGE_UNPACKED="squashfs-root"   # where an AppImage's --appimage-extract puts its files

problems=0

problem() {
  echo "  $1" >&2
  problems=$((problems + 1))
}

# What AppImage $1 holds, one file per line: a copy of it unpacks itself in a folder of its own
# (--appimage-extract, which needs no FUSE).
list_appimage() {
  local unpacked
  unpacked="$(mktemp -d "${WORK_DIR}/appimage.XXXXXX")"
  cp "$1" "${unpacked}/download"
  chmod +x "${unpacked}/download"
  (cd "${unpacked}" && ./download --appimage-extract > /dev/null) || return 1
  (cd "${unpacked}/${APPIMAGE_UNPACKED}" && find . -mindepth 1 | sed 's|^\./||')
}

# What archive $1 lists, one file per line.
list_archive() {
  case "$1" in
    *.tar.gz) tar -tzf "$1" ;;
    *.zip) unzip -Z1 "$1" ;;
    *.AppImage) list_appimage "$1" ;;
  esac
}

# The Linux programs and the players for other systems, in folder $1.
linux_programs() {
  local program runtime
  for program in "${PROGRAMS[@]}"; do echo "$1/${program}"; done
  for runtime in "${WINDOWS_RUNTIME}" "${ANDROID_RUNTIMES[@]}"; do echo "$1/${runtime}"; done
}

# What download $1 must hold, one path per line (inside the archive). The archives hold one
# folder; the AppImage's files are at its top.
required_files() {
  local root="${PACKAGE_NAME}/" bin program demo notice
  case "$1" in
    "${LINUX_DOWNLOAD}")
      linux_programs "${root}bin"
      ;;
    "${APPIMAGE_DOWNLOAD}")
      root=""
      echo "${APPIMAGE_ENTRY}"
      echo "${APPIMAGE_DESKTOP}"
      echo "${APPIMAGE_ICON}"
      linux_programs bin
      ;;
    "${WINDOWS_DOWNLOAD}")
      for program in "${WINDOWS_PROGRAMS[@]}"; do echo "${root}${program}"; done
      ;;
    "${MAC_ARM_DOWNLOAD}" | "${MAC_INTEL_DOWNLOAD}")
      bin="${root}RedGem.app/Contents/MacOS"
      echo "${root}RedGem.app/Contents/Info.plist"
      for program in "${PROGRAMS[@]}"; do echo "${bin}/${program}"; done
      ;;
  esac
  for demo in "${DEMOS[@]}"; do echo "${root}games/${demo}/${DEMO_FILE}"; done
  for notice in "${NOTICES[@]}" README.txt; do echo "${root}${notice}"; done
}

is_known_download() {
  local download
  for download in "${DOWNLOADS[@]}"; do
    [[ "$1" == "${download}" ]] && return 0
  done
  return 1
}

check_download() {
  local dir="$1" name="$2" listing file
  if ! is_known_download "${name}"; then
    problem "${name}: not a download of a release (they are: ${DOWNLOADS[*]})"
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
  done < <(required_files "${name}")
}

main() {
  local dir="${1:?usage: check_downloads.sh <folder> [download...]}" name
  shift
  local names=("$@")
  [[ ${#names[@]} -gt 0 ]] || names=("${DOWNLOADS[@]}")
  # (A global: the trap runs after main has returned.)
  WORK_DIR="$(mktemp -d)"
  trap 'rm -rf "${WORK_DIR}"' EXIT
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
