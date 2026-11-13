#!/usr/bin/env bash
# Fails when a program that is not the test tool knows anything of tests: the one games ship as
# (redgem-player), the one they are made with (redgem). What tests are written with lives in
# another library and another program (src/testing, redgem-test); this looks into the programs
# that were built for the words only that code has.
#   ships_no_test_code.sh [--templates <templates dir>] <program>...
# redgem carries the templates as they are, and a template's game comes with tests of its own
# (<templates dir>/<template>/tests/*_test.rb, which say SystemTest and RedGemTest): with
# --templates, a text that is one of those files' lines, whole, is theirs, not the program's.
set -uo pipefail

# The Ruby modules of tests, the flags that once ran them, a native and a log line of theirs.
MARKERS=("TestInput" "TestScreen" "SystemTest" "RedGemTest" "--ruby-test" "--test" "--headless" "leave_note" "system test" "test-saves" "redgem-test-notes")
TEMPLATES_OPTION="--templates"

command -v strings >/dev/null || { echo "strings is needed (binutils)"; exit 2; }

# The lines of the tests the templates' games come with (none without --templates).
templates_tests=""
if [[ "${1:-}" == "${TEMPLATES_OPTION}" ]]; then
  [[ -d "${2:-}" ]] || { echo "no templates at ${2:-}"; exit 2; }
  templates_tests="$(cat "$2"/*/tests/*_test.rb)"
  shift 2
fi
[[ $# -gt 0 ]] || { echo "usage: ships_no_test_code.sh [${TEMPLATES_OPTION} <dir>] <program>..."; exit 2; }

# The texts of a program that are not lines of the templates' tests.
own_texts() {
  if [[ -z "${templates_tests}" ]]; then cat; else grep -vxF -f <(printf '%s\n' "${templates_tests}"); fi
}

found=0
for PROGRAM in "$@"; do
  [[ -f "${PROGRAM}" ]] || { echo "no program at ${PROGRAM}"; exit 2; }
  # (An empty listing would find nothing wrong with anything.)
  texts="$(strings -a "${PROGRAM}")"
  [[ -n "${texts}" ]] || { echo "strings read nothing from ${PROGRAM}"; exit 2; }
  texts="$(own_texts <<<"${texts}")"
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
