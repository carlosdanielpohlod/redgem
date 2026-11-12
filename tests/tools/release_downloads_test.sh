#!/usr/bin/env bash
# tools/release/check_downloads.sh on downloads made up here: five complete ones (and the players'
# symbols) pass, and a release with a download missing, or a download missing a program, fails and
# names what is wrong.
#   release_downloads_test.sh <tools/release folder>
set -euo pipefail

RELEASE_TOOLS="$1"
# shellcheck source=tools/release/release_files.sh
source "${RELEASE_TOOLS}/release_files.sh"
CHECK="${RELEASE_TOOLS}/check_downloads.sh"

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT
failures=0

fail() {
  echo "FAIL: $1" >&2
  failures=$((failures + 1))
}

# Makes the files $4... under folder $3 of a fresh folder $2 and packs that folder as download $1,
# in ${work}/downloads.
make_archive() {
  local name="$1" tree="${work}/trees/$2" root="$3" file
  shift 3
  rm -rf "${tree}"
  for file in "$@"; do
    mkdir -p "$(dirname "${tree}/${root}/${file}")"
    echo "${file}" > "${tree}/${root}/${file}"
  done
  mkdir -p "${work}/downloads"
  rm -f "${work}/downloads/${name}"
  case "${name}" in
    *.tar.gz) tar -czf "${work}/downloads/${name}" -C "${tree}" "${root}" ;;
    *.zip) (cd "${tree}" && zip -qr "${work}/downloads/${name}" "${root}") ;;
    *.AppImage) make_appimage "${work}/downloads/${name}" "${tree}/${root}" ;;
  esac
}

# A stand-in for an AppImage at $1: a program that, as a real one does, unpacks its files (those
# of folder $2) into squashfs-root when run with --appimage-extract.
make_appimage() {
  cat > "$1" <<EOF
#!/usr/bin/env bash
[[ "\$1" == --appimage-extract ]] || exit 1
mkdir -p squashfs-root && cp -R "$2/." squashfs-root/
EOF
}

make_download() {
  local name="$1" tree="$2"
  shift 2
  make_archive "${name}" "${tree}" "${PACKAGE_NAME}" "$@"
}

# The players' symbols, every Android one in them but those of the ABIs $@ left out.
make_symbols() {
  local android=() abi
  for abi in "${ANDROID_ABIS[@]}"; do
    [[ " $* " == *" ${abi} "* ]] || android+=("android/${abi}/${ANDROID_PLAYER}")
  done
  make_archive "${LINUX_SYMBOLS}" symbols-linux "${SYMBOLS_PACKAGE}" "${LINUX_PLAYER_SYMBOLS}"
  make_archive "${WINDOWS_SYMBOLS}" symbols-windows "${SYMBOLS_PACKAGE}" "${WINDOWS_PLAYER_SYMBOLS}"
  make_archive "${ANDROID_SYMBOLS}" symbols-android "${SYMBOLS_PACKAGE}" "${android[@]}"
}

# What every download holds besides its programs.
common_files() {
  local demo notice
  for demo in "${DEMOS[@]}"; do echo "games/${demo}/${DEMO_FILE}"; done
  for notice in "${NOTICES[@]}" README.txt; do echo "${notice}"; done
}

make_all() {
  local common linux=() windows=() mac=() program runtime
  mapfile -t common < <(common_files)
  for program in "${PROGRAMS[@]}"; do
    linux+=("bin/${program}")
    mac+=("RedGem.app/Contents/MacOS/${program}")
  done
  for runtime in "${WINDOWS_RUNTIME}" "${ANDROID_RUNTIMES[@]}"; do linux+=("bin/${runtime}"); done
  for program in "${WINDOWS_PROGRAMS[@]}"; do windows+=("${program}"); done
  mac+=("RedGem.app/Contents/Info.plist")
  make_download "${LINUX_DOWNLOAD}" linux "${linux[@]}" "${common[@]}"
  make_download "${APPIMAGE_DOWNLOAD}" appimage "${APPIMAGE_ENTRY}" "${APPIMAGE_DESKTOP}" "${APPIMAGE_ICON}" \
    "${linux[@]}" "${common[@]}"
  make_download "${WINDOWS_DOWNLOAD}" windows "${windows[@]}" "${common[@]}"
  make_download "${MAC_ARM_DOWNLOAD}" mac-arm "${mac[@]}" "${common[@]}"
  make_download "${MAC_INTEL_DOWNLOAD}" mac-intel "${mac[@]}" "${common[@]}"
  make_symbols
  LINUX_FILES=("${linux[@]}" "${common[@]}")
}

# Runs the check on the downloads made; $1 is "passes" or "fails", $2 what its output must hold.
expect() {
  local outcome="$1" text="$2" output status=0
  shift 2
  output="$("${CHECK}" "${work}/downloads" "$@" 2>&1)" || status=$?
  if [[ "${outcome}" == passes && "${status}" -ne 0 ]]; then
    fail "expected the check to pass, it said: ${output}"
  elif [[ "${outcome}" == fails && "${status}" -eq 0 ]]; then
    fail "expected the check to fail, it said: ${output}"
  fi
  [[ "${output}" == *"${text}"* ]] || fail "expected \"${text}\" in: ${output}"
}

make_all
expect passes "complete"
expect passes "complete" "${WINDOWS_DOWNLOAD}"

# A release without the Mac's downloads (as v0.5.0 was at first) is refused, naming both.
rm "${work}/downloads/${MAC_ARM_DOWNLOAD}"
expect fails "${MAC_ARM_DOWNLOAD}: missing"
mv "${work}/downloads/${MAC_INTEL_DOWNLOAD}" "${work}/elsewhere.zip"
expect fails "${MAC_INTEL_DOWNLOAD}: missing"
expect fails "2 problem(s)"
# (Checking only the ones there passes.)
expect passes "complete" "${LINUX_DOWNLOAD}" "${WINDOWS_DOWNLOAD}"

# A Linux download without the Android players is refused.
make_all
make_download "${LINUX_DOWNLOAD}" linux "${LINUX_FILES[@]:0:6}"
expect fails "has no ${PACKAGE_NAME}/bin/${ANDROID_RUNTIMES[0]}"

# An AppImage without its menu entry is refused; its files are at its top, not in a folder.
make_all
make_download "${APPIMAGE_DOWNLOAD}" appimage "${APPIMAGE_ENTRY}" "${APPIMAGE_ICON}" "${LINUX_FILES[@]}"
expect fails "${APPIMAGE_DOWNLOAD}: has no ${APPIMAGE_DESKTOP}"
expect passes "complete" "${LINUX_DOWNLOAD}"

# Symbols without an Android ABI's player are refused, naming it.
make_all
make_symbols "${ANDROID_ABIS[0]}"
expect fails "has no ${SYMBOLS_PACKAGE}/android/${ANDROID_ABIS[0]}/${ANDROID_PLAYER}"

# A name that is not a download's, or a file that is not an archive.
make_all
expect fails "not a download of a release" "redgem-linux.tar.gz"
echo "not an archive" > "${work}/downloads/${WINDOWS_DOWNLOAD}"
expect fails "${WINDOWS_DOWNLOAD}: not a readable archive"

if [[ "${failures}" -gt 0 ]]; then
  echo "${failures} failure(s)" >&2
  exit 1
fi
echo "check_downloads.sh: all cases pass"
