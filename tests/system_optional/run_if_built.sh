#!/usr/bin/env bash
# Runs a test that needs a file the repository does not carry (a scene built from downloaded
# assets). When the file is not there the test is not run, and the script says so and ends with
# the exit code ctest is told to read as "skipped" (SKIP_RETURN_CODE in tests/CMakeLists.txt).
#   run_if_built.sh <needed file> <command> [arguments...]
set -uo pipefail

SKIPPED=77

needed="$1"
shift
if [[ ! -f "${needed}" ]]; then
  echo "skipped: ${needed} is not there (docs/contributing.md says how it is built)"
  exit "${SKIPPED}"
fi
exec "$@"
