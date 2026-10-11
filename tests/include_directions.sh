#!/usr/bin/env bash
# Fails when a part of the engine includes a part above it. The engine is layers, each a folder of
# src/: a layer includes itself and the layers listed for it below, and nothing else. Whatever is
# not a layer (generated/, a library's own headers) is not looked at.
#   include_directions.sh [<src dir>]
set -uo pipefail

SRC="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/src}"

# What each layer may include. The order is the engine's: what a game is made of, what draws and
# scripts it, the program that plays it (app), then the tools (editor, project) and what games
# are tested with (testing), which nothing else may know.
declare -A ALLOWED=(
  [core]=""
  [scene]="core"
  [net]="core"
  [input]="core"
  [audio]="core"
  [physics]="core scene"
  [storage]="core scene"
  [render]="core scene"
  [script]="core scene net input audio physics storage render"
  [app]="core scene net input audio physics storage render script"
  [editor]="core scene net input audio physics storage render script app"
  [project]="core scene net input audio physics storage render script app editor"
  [testing]="core scene net input audio physics storage render script app editor project"
)

# Includes that break the table and are known, as "<file under src>:<included file>", each with a
# comment saying which step removes it. One that is no longer there fails the check too: the list
# only ever gets shorter.
KNOWN=()

[[ -d "${SRC}" ]] || { echo "no sources at ${SRC}"; exit 2; }

allowed() {
  local from="$1" to="$2" layer
  [[ "${from}" == "${to}" ]] && return 0
  for layer in ${ALLOWED[${from}]}; do
    [[ "${layer}" == "${to}" ]] && return 0
  done
  return 1
}

known() {
  local edge="$1" entry
  for entry in ${KNOWN[@]+"${KNOWN[@]}"}; do
    [[ "${entry}" == "${edge}" ]] && return 0
  done
  return 1
}

failed=0
checked=0
declare -A met=()

# Every folder of src/ is a layer with a line in the table.
for folder in "${SRC}"/*/; do
  layer="$(basename "${folder}")"
  [[ -v "ALLOWED[${layer}]" ]] && continue
  echo "src/${layer}/ is not in the table of layers (tests/include_directions.sh): say what it may include"
  failed=1
done

while IFS=: read -r file _ line; do
  relative="${file#"${SRC}"/}"
  from="${relative%%/*}"
  included="${line#*\"}"
  included="${included%%\"*}"
  to="${included%%/*}"
  # Only what names another layer: "generated/..." and a library's headers are nobody's layer.
  [[ -v "ALLOWED[${from}]" && -v "ALLOWED[${to}]" && "${included}" == */* ]] || continue
  checked=$((checked + 1))
  allowed "${from}" "${to}" && continue
  if known "${relative}:${included}"; then
    met["${relative}:${included}"]=1
    continue
  fi
  echo "forbidden include: src/${relative} includes ${included} (${from} may not know ${to})"
  failed=1
done < <(grep -rn --include='*.cpp' --include='*.hpp' -E '^[[:space:]]*#[[:space:]]*include[[:space:]]*"' "${SRC}")

for entry in ${KNOWN[@]+"${KNOWN[@]}"}; do
  [[ -v "met[${entry}]" ]] && continue
  echo "no longer there, take it off the list of known includes: ${entry}"
  failed=1
done

[[ "${checked}" -gt 0 ]] || { echo "no includes were found in ${SRC}: nothing was checked"; exit 2; }
[[ "${failed}" -eq 0 ]] || exit 1
echo "include directions: ${checked} includes of the engine's own headers, all allowed (${#KNOWN[@]} known exceptions)"
