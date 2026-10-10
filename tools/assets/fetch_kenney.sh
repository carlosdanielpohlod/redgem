#!/usr/bin/env bash
# Downloads the CC0 Kenney packs used by games/city into .deps/assets (not committed).
# https://kenney.nl - all packs are Creative Commons Zero (public domain).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ASSETS_DIR="${REPO_ROOT}/.deps/assets"

declare -A PACKS=(
  [city-kit-roads]="https://kenney.nl/media/pages/assets/city-kit-roads/74288c9459-1787042796/kenney_city-kit-roads.zip"
  [car-kit]="https://kenney.nl/media/pages/assets/car-kit/1a312ec241-1775131960/kenney_car-kit.zip"
  [nature-kit]="https://kenney.nl/media/pages/assets/nature-kit/37ac38a37b-1677698939/kenney_nature-kit.zip"
  [city-kit-commercial]="https://kenney.nl/media/pages/assets/city-kit-commercial/a742d900eb-1753115042/kenney_city-kit-commercial_2.1.zip"
)

mkdir -p "${ASSETS_DIR}"
for pack in "${!PACKS[@]}"; do
  target="${ASSETS_DIR}/${pack}"
  if [[ -f "${target}/License.txt" ]]; then
    echo "${pack}: already present"
    continue
  fi
  archive="$(mktemp --suffix=.zip)"
  curl -sSfL -o "${archive}" "${PACKS[${pack}]}"
  rm -rf "${target}"
  mkdir -p "${target}"
  unzip -q "${archive}" -d "${target}"
  rm -f "${archive}"
  grep -q "CC0" "${target}/License.txt" || { echo "${pack}: unexpected license" >&2; exit 1; }
  echo "${pack}: downloaded"
done
