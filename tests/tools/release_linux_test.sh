#!/usr/bin/env bash
# The Linux downloads' scripts on programs made up here: tools/release/glibc_floor.sh (with an
# objdump that prints what each case says) passes programs within the docs' floor and refuses one
# that needs a newer glibc, the system's libstdc++ or a newer libgcc_s; AppRun.sh starts the
# editor, or the program named first.
#   release_linux_test.sh <tools/release folder>
set -euo pipefail

RELEASE_TOOLS="$1"
# shellcheck source=tools/release/release_files.sh
source "${RELEASE_TOOLS}/release_files.sh"
FLOOR="${RELEASE_TOOLS}/glibc_floor.sh"

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT
failures=0

fail() {
  echo "FAIL: $1" >&2
  failures=$((failures + 1))
}

# An objdump that prints <program>.T for -T and <program>.p for -p.
mkdir -p "${work}/tools"
cat > "${work}/tools/objdump" <<'EOF'
#!/usr/bin/env bash
case "$1" in
  -T) cat "$2.T" ;;
  -p) cat "$2.p" ;;
  *) exit 2 ;;
esac
EOF
chmod +x "${work}/tools/objdump"

# Makes program $1 taking the symbol versions $2 (space separated) and needing libraries $3.
make_program() {
  local program="${work}/$1" version library
  echo "program" > "${program}"
  : > "${program}.T"
  for version in $2; do
    echo "0000000000000000      DF *UND*  0000000000000000 (${version}) some_function" >> "${program}.T"
  done
  : > "${program}.p"
  for library in $3; do echo "  NEEDED               ${library}" >> "${program}.p"; done
}

# Runs glibc_floor.sh on programs $3...; $1 is "passes" or "fails", $2 what its output must hold.
expect() {
  local outcome="$1" text="$2" output status=0 program programs=()
  shift 2
  for program in "$@"; do programs+=("${work}/${program}"); done
  output="$(PATH="${work}/tools:${PATH}" "${FLOOR}" "${programs[@]}" 2>&1)" || status=$?
  if [[ "${outcome}" == passes && "${status}" -ne 0 ]]; then
    fail "expected glibc_floor.sh to pass, it said: ${output}"
  elif [[ "${outcome}" == fails && "${status}" -eq 0 ]]; then
    fail "expected glibc_floor.sh to fail, it said: ${output}"
  fi
  [[ "${output}" == *"${text}"* ]] || fail "expected \"${text}\" in: ${output}"
}

make_program old "GLIBC_2.2.5 GLIBC_2.4 GLIBC_2.17 GCC_3.0" "libc.so.6 libgcc_s.so.1"
make_program floor "GLIBC_2.4 GLIBC_${LINUX_GLIBC_FLOOR} GLIBC_PRIVATE" "libc.so.6 libm.so.6"
# (2.4 is older than 2.17: versions are compared as numbers, not as text.)
expect passes "The newest glibc these programs need: 2.17" old
expect passes "need: ${LINUX_GLIBC_FLOOR}" old floor

make_program newer "GLIBC_2.4 GLIBC_9.99" "libc.so.6"
expect fails "they need glibc 9.99, newer than the ${LINUX_GLIBC_FLOOR} the docs say" old newer

make_program shared_cxx "GLIBC_2.17" "libstdc++.so.6 libc.so.6"
expect fails "needs the system's libstdc++" shared_cxx
make_program cxx_symbols "GLIBC_2.17 GLIBCXX_3.4.30 CXXABI_1.3" "libc.so.6"
expect fails "needs the system's libstdc++" cxx_symbols

make_program new_libgcc "GLIBC_2.17 GCC_99.0.0" "libc.so.6 libgcc_s.so.1"
expect fails "needs libgcc_s 99.0.0" new_libgcc

expect fails "missing" absent

# AppRun.sh in a made-up AppImage folder whose programs say their name and arguments.
app="${work}/app"
mkdir -p "${app}/bin"
for program in "${PROGRAMS[@]}"; do
  printf '#!/bin/sh\necho "%s $*"\n' "${program}" > "${app}/bin/${program}"
  chmod +x "${app}/bin/${program}"
done
install -m 755 "${RELEASE_TOOLS}/AppRun.sh" "${app}/${APPIMAGE_ENTRY}"

expect_app() {
  local expected="$1" output
  shift
  output="$(env -u APPDIR "${app}/${APPIMAGE_ENTRY}" "$@")" || fail "AppRun $* failed"
  [[ "${output}" == "${expected}" ]] || fail "AppRun $* ran \"${output}\", not \"${expected}\""
}
expect_app "redgem "
expect_app "redgem check my game" check "my game"
expect_app "redgem-server --help" redgem-server --help
expect_app "redgem-relay --port 1" redgem-relay --port 1
expect_app "redgem redgem" redgem

if [[ "${failures}" -gt 0 ]]; then
  echo "${failures} failure(s)" >&2
  exit 1
fi
echo "glibc_floor.sh and AppRun.sh: all cases pass"
