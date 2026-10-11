#!/usr/bin/env bash
# Runs clang-tidy on some C++ files or folders only and prints one line per finding: what to
# use while working on a part of the engine (tools/lint.sh checks everything).
#   tools/lint_cpp_files.sh src/net src/project/cli.cpp
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JOBS="$(( ($(nproc) + 1) / 2 ))"
cd "${REPO_ROOT}" || exit 1

files=()
for target in "$@"; do
  if [[ -d "${target}" ]]; then
    # (A file deleted and not committed yet is still listed: it is not there to lint.)
    while IFS= read -r file; do
      if [[ -f "${file}" ]]; then files+=("${file}"); fi
    done < <(git ls-files "${target}/*.cpp"; git ls-files --others --exclude-standard "${target}/*.cpp")
  else
    files+=("${target}")
  fi
done
[[ ${#files[@]} -gt 0 ]] || { echo "usage: lint_cpp_files.sh <file or folder>..."; exit 2; }

# (android_main.cpp is only built for Android: its headers are not on this machine's compiler.)
printf '%s\n' "${files[@]}" | grep -v 'android_main.cpp' | xargs -P "${JOBS}" -n 2 .deps/lint-venv/bin/clang-tidy -p build --quiet 2>/dev/null |
  grep -E "warning:|error:" | sed "s|${REPO_ROOT}/||" | sort -u
