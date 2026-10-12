#!/usr/bin/env bash
# Checks tools/blender/run.sh: Blender is given a HOME that is not the developer's, new for every
# run and gone when the run ends. A stand-in plays Blender: it notes the HOME it was given.
#   blender_home.sh <tools/blender/run.sh>
set -uo pipefail

RUN="$1"

work="$(mktemp -d "${TMPDIR:-/tmp}/redgem-blender-test.XXXXXX")" || exit 1
trap 'rm -rf "${work}"' EXIT

cat > "${work}/blender" <<'STAND_IN'
#!/usr/bin/env bash
# Notes the HOME it runs with, and whether that is an empty folder.
echo "${HOME}" > "${NOTE}"
[[ -d "${HOME}" && -z "$(ls -A "${HOME}")" ]] || exit 1
touch "${HOME}/left-behind"
STAND_IN
chmod +x "${work}/blender"

fail() {
  echo "$1"
  exit 1
}

# One run of the tool; prints the HOME the stand-in was given.
home_of_a_run() {
  NOTE="${work}/home" BLENDER="${work}/blender" "${RUN}" builder.py "${work}/out" \
    || fail "the run failed: its HOME was not a new, empty folder"
  cat "${work}/home"
}

first="$(home_of_a_run)" || fail "${first}"
second="$(home_of_a_run)" || fail "${second}"
[[ -n "${first}" && "${first}" != "${HOME}" ]] || fail "Blender ran in the developer's HOME"
[[ "${first}" != "${second}" ]] || fail "two runs shared a HOME: ${first}"
[[ ! -e "${first}" && ! -e "${second}" ]] || fail "a run's HOME was left behind"
echo "each run had a HOME of its own, removed afterwards"
