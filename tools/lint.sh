#!/usr/bin/env bash
# Runs every linter the project uses and fails when any of them complains:
#   C++     clang-tidy (function size, complexity, magic numbers, bug patterns), cppcheck, clang-format
#   Ruby    rubocop
#   Python  ruff
#   Shell   shellcheck
# The Python-packaged ones are installed into .deps/lint-venv on first use; rubocop and cppcheck
# come from your system (gem install rubocop; your package manager's cppcheck).
#   tools/lint.sh              everything
#   tools/lint.sh cpp ruby     only some (cpp, ruby, python, shell, format)
#   tools/lint.sh --fix format reformat the C++ in place
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV="${REPO_ROOT}/.deps/lint-venv"
BUILD_DIR="${REPO_ROOT}/build"   # for compile_commands.json
JOBS="$(( ($(nproc) + 1) / 2 ))" # half the processors: linting must not starve the machine
cd "${REPO_ROOT}" || exit 1

fix=false
wanted=()
for argument in "$@"; do
  if [[ "${argument}" == "--fix" ]]; then fix=true; else wanted+=("${argument}"); fi
done
[[ ${#wanted[@]} -eq 0 ]] && wanted=(format cpp ruby python shell)

if [[ ! -x "${VENV}/bin/clang-tidy" ]]; then
  python3 -m venv "${VENV}" && "${VENV}/bin/pip" install -q clang-tidy clang-format ruff shellcheck-py || exit 1
fi

cpp_sources() { git ls-files 'src/*.cpp' 'src/*.hpp' 'tests/*.cpp' 'tests/*.hpp'; }
failed=()

run() {
  local name="$1"
  shift
  echo "== ${name}"
  "$@" || failed+=("${name}")
}

lint_format() {
  if ${fix}; then
    cpp_sources | xargs "${VENV}/bin/clang-format" -i
  else
    cpp_sources | xargs "${VENV}/bin/clang-format" --dry-run --Werror
  fi
}

lint_cpp() {
  [[ -f "${BUILD_DIR}/compile_commands.json" ]] || { echo "build the engine first (cmake -S . -B build)"; return 1; }
  git ls-files 'src/*.cpp' | xargs -P "${JOBS}" -n 4 "${VENV}/bin/clang-tidy" -p "${BUILD_DIR}" --quiet --warnings-as-errors='*' &&
    cppcheck --quiet --error-exitcode=1 --std=c++17 --language=c++ --enable=warning,performance,portability \
      --inline-suppr --suppress=missingIncludeSystem -I src -j "${JOBS}" src
}

lint_ruby() { rubocop --format quiet ruby games templates tests; }
lint_python() { "${VENV}/bin/ruff" check tools; }
lint_shell() { git ls-files '*.sh' | xargs "${VENV}/bin/shellcheck"; }

for linter in "${wanted[@]}"; do
  case "${linter}" in
    format) run "clang-format" lint_format ;;
    cpp) run "clang-tidy and cppcheck" lint_cpp ;;
    ruby) run "rubocop" lint_ruby ;;
    python) run "ruff" lint_python ;;
    shell) run "shellcheck" lint_shell ;;
    *) echo "unknown linter ${linter} (cpp, ruby, python, shell, format)"; exit 2 ;;
  esac
done

if [[ ${#failed[@]} -gt 0 ]]; then
  echo "failed: ${failed[*]}"
  exit 1
fi
echo "all clean"
