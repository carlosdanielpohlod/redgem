#!/usr/bin/env bash
# Builds a game for Android, installs it on the phone or emulator adb sees, starts it and shows
# what the game logs.
#   tools/android/run_on_device.sh games/platformer
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
GAME="${1:?usage: run_on_device.sh <game_dir>}"
OUT="${REPO_ROOT}/build/android-out"

apk="$("${REPO_ROOT}/build/src/redgem" build "${GAME}" "${OUT}" --target android | sed -n 's/.*adb install //p')"
[[ -n "${apk}" ]] || { echo "the build failed" >&2; exit 1; }
# The app's id is in the APK; ask the SDK's own tool for it.
aapt2="$(find "${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}/build-tools" -name aapt2 | sort -V | tail -1)"
package="$("${aapt2}" dump packagename "${apk}")"
adb install -r "${apk}"
adb shell am force-stop "${package}"
adb logcat -c
adb shell am start -n "${package}/android.app.NativeActivity"
echo "Running ${package}. Its log (Ctrl+C to stop watching):"
adb logcat -s redgem raylib
