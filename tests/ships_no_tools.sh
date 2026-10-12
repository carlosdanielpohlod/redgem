#!/usr/bin/env bash
# Fails when a program that goes to other people carries what it has no use for. Each program is
# made of the libraries it needs and no more (src/CMakeLists.txt); this looks into the program that
# was built for words only the other libraries have.
#   ships_no_tools.sh player <redgem-player>   what players receive: none of the tools games are made with
#   ships_no_tools.sh relay <redgem-relay>     what runs on a server: no tools, and nothing that draws or scripts
set -uo pipefail

KIND="${1:-}"
PROGRAM="${2:-}"

# The tools, in the order they are reported: the library the editor's interface is drawn with,
# texts of the editor and of the project hub, the packer of phone apps and the SDK tools it runs,
# the scripts of the new-game templates, the model importer, the command line, the licenses that
# a build writes beside a game, and the relay's log line.
TOOLS=(
  "Dear ImGui" "ImGui" "imgui.ini"
  "Build Game" "Hierarchy" "Inspector" "Unsaved changes"
  "New Game..." "Create game" "No games yet. Create one on the left."
  "apksigner" "keytool" "zipalign" "AndroidManifest.xml" "android-debug.keystore"
  "class Spinner < Behaviour" "class GameMenu < Behaviour"
  "cannot import" "cannot build for" "redgem build <game_dir>" "--template"
  "THIRD_PARTY_LICENSES"
  "relay listening on port"
)
# What plays a game: the window and graphics library, the scripting language, the physics, the
# model loader, the engine's own Ruby and its log lines.
GAME=("raylib" "GLFW" "mruby" "Jolt" "cgltf" "class Behaviour" "graphics card: ")

case "${KIND}" in
  player) MARKERS=("${TOOLS[@]}") ;;
  # (The relay is the one program its own log line belongs in.)
  relay) MARKERS=("${TOOLS[@]/relay listening on port/}" "${GAME[@]}") ;;
  *) echo "usage: ships_no_tools.sh player|relay <program>"; exit 2 ;;
esac

[[ -f "${PROGRAM}" ]] || { echo "no program at ${PROGRAM}"; exit 2; }
command -v strings >/dev/null || { echo "strings is needed (binutils)"; exit 2; }
# (An empty listing would find nothing wrong with anything.)
texts="$(strings -a "${PROGRAM}")"
[[ -n "${texts}" ]] || { echo "strings read nothing from ${PROGRAM}"; exit 2; }

found=0
for marker in "${MARKERS[@]}"; do
  [[ -n "${marker}" ]] || continue
  lines="$(grep -F -- "${marker}" <<<"${texts}" | head -3)"
  [[ -z "${lines}" ]] && continue
  found=1
  echo "found in ${PROGRAM}: \"${marker}\""
  echo "${lines}"
done

[[ "${found}" -eq 0 ]] || exit 1
echo "nothing a ${KIND} has no use for in ${PROGRAM}"
