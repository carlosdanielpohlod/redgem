#!/usr/bin/env bash
# Starts a program that opens a window, lets it draw for a while and takes a picture of the whole
# screen: how the Mac's CI shows the editor it built. Fails when the program ended before the
# picture was taken, or when no picture could be taken. What the program said is printed, and
# kept beside the picture (<picture>.log).
#   capture_window.sh <picture.png> <seconds> <command> [arguments...]
set -uo pipefail

PICTURE="$1"
SHOWN_SECONDS="$2"
shift 2
LOG="${PICTURE%.png}.log"

mkdir -p "$(dirname "${PICTURE}")"
"$@" > "${LOG}" 2>&1 &
program=$!
sleep "${SHOWN_SECONDS}"

if ! kill -0 "${program}" 2>/dev/null; then
  wait "${program}"
  status=$?
  echo "$* ended with ${status} before the picture was taken. It said:"
  cat "${LOG}"
  exit 1
fi

screencapture -x "${PICTURE}"
captured=$?
kill "${program}"
wait "${program}" 2>/dev/null
echo "$* said:"
cat "${LOG}"
[[ "${captured}" -eq 0 && -s "${PICTURE}" ]] || { echo "no picture of the screen was taken"; exit 1; }
echo "the screen is in ${PICTURE}"
