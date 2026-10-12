#!/usr/bin/env bash
# Fails when a program that is not the test tool knows anything of tests: the one games ship as
# (redgem-player), the one they are made with (redgem). What tests are written with lives in
# another library and another program (src/testing, redgem-test); this looks into the programs
# that were built for the words only that code has.
#   ships_no_test_code.sh <program>...
set -uo pipefail

# The Ruby modules of tests, the flags that once ran them, a native and a log line of theirs.
MARKERS=("TestInput" "TestScreen" "SystemTest" "RedGemTest" "--ruby-test" "--test" "--headless" "leave_note" "system test" "test-saves" "redgem-test-notes")

command -v strings >/dev/null || { echo "strings is needed (binutils)"; exit 2; }
[[ $# -gt 0 ]] || { echo "usage: ships_no_test_code.sh <program>..."; exit 2; }

found=0
for PROGRAM in "$@"; do
  [[ -f "${PROGRAM}" ]] || { echo "no program at ${PROGRAM}"; exit 2; }
  # (An empty listing would find nothing wrong with anything.)
  texts="$(strings -a "${PROGRAM}")"
  [[ -n "${texts}" ]] || { echo "strings read nothing from ${PROGRAM}"; exit 2; }
  for marker in "${MARKERS[@]}"; do
    lines="$(grep -F -- "${marker}" <<<"${texts}" | head -5)"
    [[ -z "${lines}" ]] && continue
    found=1
    echo "test code in ${PROGRAM}: \"${marker}\""
    echo "${lines}"
  done
done

[[ "${found}" -eq 0 ]] || exit 1
echo "no test code in $*"
