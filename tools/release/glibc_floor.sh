#!/usr/bin/env bash
# Measures the oldest glibc Linux programs run on: the newest GLIBC_ version among the symbols
# each one takes from the system (objdump -T), and prints it. Fails when that is newer than the
# floor the docs say (LINUX_GLIBC_FLOOR in release_files.sh), when a program needs the system's
# libstdc++ (it is linked into the programs, so that no distribution's libstdc++ matters), or when
# a doc says another floor.
#   tools/release/glibc_floor.sh build/src/redgem build/src/redgem-player ...
set -euo pipefail

# shellcheck source=tools/release/release_files.sh
source "$(dirname "${BASH_SOURCE[0]}")/release_files.sh"

GLIBC_MARK="GLIBC_"
CXX_MARKS="GLIBCXX_|CXXABI_"   # the versions of libstdc++'s symbols
CXX_LIBRARY="libstdc++.so"
# The docs that say the floor, each as "glibc <floor> or newer".
FLOOR_DOCS=(tools/release/README-linux.txt docs/install.md KNOWN_LIMITATIONS.md)

problems=0

problem() {
  echo "  $1" >&2
  problems=$((problems + 1))
}

# The newest of the versions on standard input, one per line.
newest() { sort -V | tail -n 1; }

# Whether version $1 is no newer than version $2.
at_most() { [[ "$(printf '%s\n%s\n' "$1" "$2" | newest)" == "$2" ]]; }

# The glibc versions program $1 takes symbols of, one per line, newest last (none for a program
# that takes nothing from glibc).
glibc_versions() {
  local symbols
  symbols="$(objdump -T "$1")"
  { grep -oE "\\b${GLIBC_MARK}[0-9]+(\\.[0-9]+)+" <<<"${symbols}" || true; } | sed "s/^${GLIBC_MARK}//" | sort -uV
}

# The libraries program $1 needs, one per line.
needed_libraries() {
  local headers
  headers="$(objdump -p "$1")"
  awk '/^ +NEEDED/ { print $2 }' <<<"${headers}"
}

# Whether program $1 needs libstdc++ from the system.
needs_system_cxx() {
  local symbols
  symbols="$(objdump -T "$1")"
  grep -qF "${CXX_LIBRARY}" <<<"$(needed_libraries "$1")" || grep -qE "\\b(${CXX_MARKS})[0-9]" <<<"${symbols}"
}

check_docs() {
  local doc claim="glibc ${LINUX_GLIBC_FLOOR} or newer"
  for doc in "${FLOOR_DOCS[@]}"; do
    grep -qF -- "${claim}" "${REPO_ROOT}/${doc}" || problem "${doc} does not say \"${claim}\""
  done
}

main() {
  [[ $# -gt 0 ]] || { echo "usage: glibc_floor.sh <program>..." >&2; exit 2; }
  local program floor all=()
  for program in "$@"; do
    [[ -f "${program}" ]] || { problem "${program}: missing"; continue; }
    floor="$(glibc_versions "${program}" | newest)"
    echo "$(basename "${program}"): glibc ${floor:-(none)}; needs $(needed_libraries "${program}" | paste -sd ' ')"
    if [[ -n "${floor}" ]]; then all+=("${floor}"); fi
    if needs_system_cxx "${program}"; then problem "${program}: needs the system's libstdc++"; fi
  done
  floor="$(printf '%s\n' "${all[@]}" | newest)"
  echo "The newest glibc these programs need: ${floor}"
  if [[ -n "${floor}" ]] && ! at_most "${floor}" "${LINUX_GLIBC_FLOOR}"; then
    problem "they need glibc ${floor}, newer than the ${LINUX_GLIBC_FLOOR} the docs say"
  fi
  check_docs
  if [[ "${problems}" -gt 0 ]]; then
    echo "The glibc floor is not the one the docs say: ${problems} problem(s), above." >&2
    exit 1
  fi
  echo "They run on glibc ${LINUX_GLIBC_FLOOR} or newer, as the docs say."
}

main "$@"
