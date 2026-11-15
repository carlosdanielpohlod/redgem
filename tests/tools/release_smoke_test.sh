#!/usr/bin/env bash
# tools/release/smoke.sh on a Linux download made up here, whose programs are stand-ins that say
# what they were asked: it passes when the built game and the editor close themselves well, the
# tests pass and the system test leaves its picture, and fails, naming what failed, when one of
# them does not. --gl puts its libraries beside the programs and the built game.
#   release_smoke_test.sh <tools/release folder>
set -euo pipefail

RELEASE_TOOLS="$1"
# shellcheck source=tools/release/release_files.sh
source "${RELEASE_TOOLS}/release_files.sh"
SMOKE="${RELEASE_TOOLS}/smoke.sh"

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT
failures=0

fail() {
  echo "FAIL: $1" >&2
  failures=$((failures + 1))
}

# The editor: it builds a game into a program that, like the editor, ends well only when told to
# close itself (and fails with FAKE_PLAYER_FAILS); it makes and checks games.
write_editor() {
  cat > "$1" <<'EOF'
#!/usr/bin/env bash
needs_close() { [[ -n "${REDGEM_CLOSE_AFTER_FRAMES:-}" ]] || { echo "no REDGEM_CLOSE_AFTER_FRAMES"; exit 1; }; }
case "$1" in
  build)
    mkdir -p "$3"
    echo pak > "$3/game.rgpak"
    program="$3/$(basename "$2")"
    printf '%s\n' '#!/usr/bin/env bash' \
      '[[ -n "${REDGEM_CLOSE_AFTER_FRAMES:-}" ]] || exit 1' \
      '[[ -z "${FAKE_NEEDS_GL:-}" || -f "$(dirname "$0")/opengl32.dll" ]] || { echo "no opengl32.dll"; exit 1; }' \
      '[[ -z "${FAKE_PLAYER_FAILS:-}" ]] || { echo "the player crashed"; exit 1; }' > "${program}"
    chmod +x "${program}"
    ;;
  new) mkdir -p "$2/tests" ;;
  check) [[ -z "${FAKE_TESTS_FAIL:-}" ]] || { echo "FAIL walk_test.rb"; exit 1; } ;;
  *) needs_close; [[ -z "${FAKE_EDITOR_FAILS:-}" ]] || { echo "the editor crashed"; exit 1; } ;;
esac
EOF
}

# The test tool: it leaves the picture the system test takes, unless FAKE_NO_PICTURE.
write_tester() {
  cat > "$1" <<'EOF'
#!/usr/bin/env bash
[[ "$3" == --screenshots ]] || exit 2
mkdir -p "$4"
[[ -n "${FAKE_NO_PICTURE:-}" ]] || echo png > "$4/smoke_window.png"
EOF
}

make_download() {
  local root="${work}/tree/${PACKAGE_NAME}"
  mkdir -p "${root}/bin" "${root}/games/${DEMOS[0]}" "${work}/downloads"
  write_editor "${root}/bin/redgem"
  write_tester "${root}/bin/redgem-test"
  chmod +x "${root}/bin/redgem" "${root}/bin/redgem-test"
  echo scene > "${root}/games/${DEMOS[0]}/${DEMO_FILE}"
  tar -czf "${work}/downloads/${LINUX_DOWNLOAD}" -C "${work}/tree" "${PACKAGE_NAME}"
}

# Runs smoke.sh with the variables $3... set; $1 is "passes" or "fails", $2 what its output must
# hold. (The arguments after "--" are smoke.sh's own, before the download.)
expect() {
  local outcome="$1" text="$2" output status=0 variables=() options=()
  shift 2
  while [[ $# -gt 0 && "$1" != "--" ]]; do variables+=("$1"); shift; done
  if [[ $# -gt 0 ]]; then
    shift
    options=("$@")
  fi
  output="$(env "${variables[@]}" "${SMOKE}" "${options[@]}" "${work}/downloads/${LINUX_DOWNLOAD}" 2>&1)" ||
    status=$?
  if [[ "${outcome}" == passes && "${status}" -ne 0 ]]; then
    fail "expected smoke.sh to pass, it said: ${output}"
  elif [[ "${outcome}" == fails && "${status}" -eq 0 ]]; then
    fail "expected smoke.sh to fail, it said: ${output}"
  fi
  [[ "${output}" == *"${text}"* ]] || fail "expected \"${text}\" in: ${output}"
}

make_download
expect passes "${LINUX_DOWNLOAD} opens its windows."
expect fails "the player crashed" FAKE_PLAYER_FAILS=1
expect fails "the editor crashed" FAKE_EDITOR_FAILS=1
expect fails "FAIL walk_test.rb" FAKE_TESTS_FAIL=1
expect fails "left no smoke_window.png" FAKE_NO_PICTURE=1
# The libraries of --gl are beside the built game, and a --gl folder without them is refused.
mkdir -p "${work}/gl"
echo library > "${work}/gl/opengl32.dll"
expect fails "has no libgallium_wgl.dll" -- --gl "${work}/gl"
echo library > "${work}/gl/libgallium_wgl.dll"
expect fails "no opengl32.dll" FAKE_NEEDS_GL=1
expect passes "opens its windows." FAKE_NEEDS_GL=1 -- --gl "${work}/gl"

if [[ "${failures}" -gt 0 ]]; then
  echo "${failures} failure(s)" >&2
  exit 1
fi
echo "smoke.sh: all cases pass"
