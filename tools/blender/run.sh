#!/usr/bin/env bash
# Runs a scene builder in headless Blender with a neutral HOME, so the saved .blend files
# do not embed the developer's home directory. The HOME is a new, empty folder every run, gone
# when the run ends: nothing one run left behind reaches the next, or another one going on.
#   tools/blender/run.sh build_platformer.py games/platformer
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BLENDER="${BLENDER:-blender}"

if [[ $# -lt 2 ]]; then
  echo "usage: $0 <builder.py> <output dir>" >&2
  exit 2
fi

NEUTRAL_HOME="$(mktemp -d "${TMPDIR:-/tmp}/redgem-blender.XXXXXX")"
trap 'rm -rf "${NEUTRAL_HOME}"' EXIT
# Heavy scenes can eat all the memory and freeze the machine: with BLENDER_MEMORY_MAX (e.g. 6500M)
# Blender runs under that cap and is killed if it goes over.
LIMIT=()
if [[ -n "${BLENDER_MEMORY_MAX:-}" ]] && command -v systemd-run >/dev/null; then
  LIMIT=(systemd-run --user --scope -q -p "MemoryMax=${BLENDER_MEMORY_MAX}" -p MemorySwapMax=0)
fi
HOME="${NEUTRAL_HOME}" "${LIMIT[@]}" "${BLENDER}" -b --factory-startup --python "${SCRIPT_DIR}/$1" -- "$2"
