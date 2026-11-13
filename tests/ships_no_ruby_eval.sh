#!/usr/bin/env bash
# Fails when a program carries the editor's Ruby evaluation (project/ruby_eval.hpp), which only
# development builds have (the CMake option REDGEM_RUBY_EVAL): a release build of redgem, the
# player and the game server must have none of its texts.
#   ships_no_ruby_eval.sh <program>...
set -uo pipefail

# The tool's name, the option that turns it on and the line that prints its token.
MARKERS=("game_eval" "--ruby-eval" "ruby eval token")

command -v strings >/dev/null || { echo "strings is needed (binutils)"; exit 2; }
[[ $# -gt 0 ]] || { echo "usage: ships_no_ruby_eval.sh <program>..."; exit 2; }

found=0
for PROGRAM in "$@"; do
  [[ -f "${PROGRAM}" ]] || { echo "no program at ${PROGRAM}"; exit 2; }
  # (An empty listing would find nothing wrong with anything.)
  texts="$(strings -a "${PROGRAM}")"
  [[ -n "${texts}" ]] || { echo "strings read nothing from ${PROGRAM}"; exit 2; }
  for marker in "${MARKERS[@]}"; do
    lines="$(grep -F -- "${marker}" <<<"${texts}" | head -3)"
    [[ -z "${lines}" ]] && continue
    found=1
    echo "ruby eval in ${PROGRAM}: \"${marker}\""
    echo "${lines}"
  done
done

[[ "${found}" -eq 0 ]] || exit 1
echo "no ruby eval in $*"
