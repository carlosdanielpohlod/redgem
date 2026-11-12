#!/usr/bin/env bash
# Measures the oldest glibc Linux programs run on: the newest GLIBC_ version among the symbols
# each one takes from the system (objdump -T), and prints it. Fails when that is newer than the
# floor the docs say (LINUX_GLIBC_FLOOR in release_files.sh), when a program needs the system's
# libstdc++ (it is linked into the programs, so that no distribution's libstdc++ matters), when
# it takes a symbol of libgcc_s newer than LINUX_LIBGCC_FLOOR, or when a doc says another floor.
#   tools/release/glibc_floor.sh build/src/redgem build/src/redgem-player ...
set -euo pipefail

# shellcheck source=tools/release/release_files.sh
source "$(dirname "${BASH_SOURCE[0]}")/release_files.sh"

GLIBC_MARK="GLIBC_"
LIBGCC_MARK="GCC_"
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

# The newest version marked $2 (GLIBC_, GCC_) among the dynamic symbols $1 (objdump -T's text);
# nothing when there is none.
newest_version() {
  { grep -oE "\\b$2[0-9]+(\\.[0-9]+)*" <<<"$1" || true; } | sed "s/^$2//" | newest
}

# The libraries headers $1 (objdump -p's text) name as needed, separated by spaces.
needed_libraries() { awk '/^ +NEEDED/ { printf "%s%s", sep, $2; sep = " " }' <<<"$1"; }

# Whether a program with symbols $1 and headers $2 needs libstdc++ from the system.
needs_system_cxx() {
  [[ "$(needed_libraries "$2")" == *"${CXX_LIBRARY}"* ]] || grep -qE "\\b(${CXX_MARKS})[0-9]" <<<"$1"
}

check_docs() {
  local doc claim="glibc ${LINUX_GLIBC_FLOOR} or newer"
  for doc in "${FLOOR_DOCS[@]}"; do
    grep -qF -- "${claim}" "${REPO_ROOT}/${doc}" || problem "${doc} does not say \"${claim}\""
  done
}

# Measures program $1, printing a line about it; adds its glibc version to FLOORS.
measure() {
  local program="$1" symbols headers floor libgcc
  [[ -f "${program}" ]] || { problem "${program}: missing"; return; }
  symbols="$(objdump -T "${program}")" || { problem "${program}: not a Linux program"; return; }
  headers="$(objdump -p "${program}")"
  floor="$(newest_version "${symbols}" "${GLIBC_MARK}")"
  libgcc="$(newest_version "${symbols}" "${LIBGCC_MARK}")"
  echo "$(basename "${program}"): glibc ${floor:-(none)}, libgcc ${libgcc:-(none)}; needs $(needed_libraries "${headers}")"
  if [[ -n "${floor}" ]]; then FLOORS+=("${floor}"); fi
  if needs_system_cxx "${symbols}" "${headers}"; then problem "${program}: needs the system's libstdc++"; fi
  if [[ -n "${libgcc}" ]] && ! at_most "${libgcc}" "${LINUX_LIBGCC_FLOOR}"; then
    problem "${program}: needs libgcc_s ${libgcc}, newer than ${LINUX_LIBGCC_FLOOR}"
  fi
}

main() {
  [[ $# -gt 0 ]] || { echo "usage: glibc_floor.sh <program>..." >&2; exit 2; }
  local program floor
  FLOORS=()
  for program in "$@"; do measure "${program}"; done
  floor="$(printf '%s\n' ${FLOORS[@]+"${FLOORS[@]}"} | newest)"
  echo "The newest glibc these programs need: ${floor:-(none)}"
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
