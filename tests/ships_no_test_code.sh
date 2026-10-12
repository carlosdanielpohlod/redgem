#!/usr/bin/env bash
# Fails when the program games ship as knows anything of tests. What tests are written with lives
# in another library and another program (src/testing, redgem-test); this looks into the program
# that was built for the words only that code has.
#   ships_no_test_code.sh <redgem>
set -uo pipefail

PROGRAM="$1"
# The Ruby modules of tests, the flags that once ran them, a native and a log line of theirs.
MARKERS=("TestInput" "TestScreen" "SystemTest" "RedGemTest" "--ruby-test" "--test" "--headless" "leave_note" "system test" "test-saves" "redgem-test-notes")

[[ -f "${PROGRAM}" ]] || { echo "no program at ${PROGRAM}"; exit 2; }
command -v strings >/dev/null || { echo "strings is needed (binutils)"; exit 2; }
# (An empty listing would find nothing wrong with anything.)
texts="$(strings -a "${PROGRAM}")"
[[ -n "${texts}" ]] || { echo "strings read nothing from ${PROGRAM}"; exit 2; }

found=0
for marker in "${MARKERS[@]}"; do
  lines="$(grep -F -- "${marker}" <<<"${texts}" | head -5)"
  [[ -z "${lines}" ]] && continue
  found=1
  echo "test code in ${PROGRAM}: \"${marker}\""
  echo "${lines}"
done

[[ "${found}" -eq 0 ]] || exit 1
echo "no test code in ${PROGRAM}"
