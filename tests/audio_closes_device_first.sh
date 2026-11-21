#!/usr/bin/env bash
# Checks that the raylib the engine builds closes its audio device before the lock the device's
# thread takes (cmake/PatchRaylibAudioClose.cmake): the other way, a sound asked for while the game
# closes locks a destroyed lock, which ends an Android app.
#   audio_closes_device_first.sh <raylib>/src/raudio.c
set -euo pipefail
audio_file=$1
# The line of `text` inside CloseAudioDevice (InitAudioDevice also stops the device when it fails).
line_in_close() {
  awk -v text="$1" '/^void CloseAudioDevice\(/ { inside = 1 } inside && index($0, text) { print NR; exit }
                    inside && /^}/ { exit }' "$audio_file"
}
device_line=$(line_in_close 'ma_device_uninit(&AUDIO.System.device);')
lock_line=$(line_in_close 'ma_mutex_uninit(&AUDIO.System.lock);')
if [[ -z "$device_line" || -z "$lock_line" ]]; then
  echo "closing the audio device not found in $audio_file"
  exit 1
fi
if (( device_line > lock_line )); then
  echo "$audio_file destroys the audio lock (line $lock_line) before it stops the device (line $device_line)"
  exit 1
fi
echo "the audio device stops (line $device_line) before its lock goes (line $lock_line)"
