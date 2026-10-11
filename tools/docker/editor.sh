#!/usr/bin/env bash
# Runs RedGem from Docker: builds the image the first time (a few minutes), then starts it on your
# X11 display as your own user. Takes the same arguments as the redgem program.
#   tools/docker/editor.sh                                 project hub
#   tools/docker/editor.sh games/platformer                open a demo
#   tools/docker/editor.sh new ~/RedGemGames/my_game       create a game
# Your games folder (~/RedGemGames) and your settings are at the same paths inside the container,
# so paths under them mean the same thing in both places. Nothing else of your home is visible.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."

if ! command -v docker > /dev/null; then
  echo "Docker is not installed: see https://docs.docker.com/engine/install/" >&2
  exit 1
fi
if [[ -z "${DISPLAY:-}" ]]; then
  echo "No X11 display found (DISPLAY is empty). The editor needs a Linux desktop session." >&2
  exit 1
fi

# The folders the container shares with you must exist first, or Docker creates them as root.
mkdir -p "${HOME}/RedGemGames" "${HOME}/.config/redgem"

# Let local containers draw on this display for as long as the editor runs.
xhost +local:docker > /dev/null
trap 'xhost -local:docker > /dev/null' EXIT

docker compose run --rm --user "$(id -u):$(id -g)" -e "HOME=${HOME}" \
  -v "${HOME}/RedGemGames:${HOME}/RedGemGames" -v "${HOME}/.config/redgem:${HOME}/.config/redgem" \
  editor build/src/redgem "$@"
