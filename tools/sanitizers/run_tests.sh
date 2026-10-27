#!/usr/bin/env bash
# Runs the tests that open no window in a build made with sanitizers, stopping at the first
# finding (the continuous integration's Sanitizers workflow runs it):
#   cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DREDGEM_SANITIZERS=address,undefined
#   cmake --build build-asan && tools/sanitizers/run_tests.sh build-asan address
#   (the same with -DREDGEM_SANITIZERS=thread, build-tsan and "thread")
# What is reported inside the libraries the engine is built from is listed in the suppression
# files beside this script; what is reported in the engine's own code is fixed.
# The system tests (game windows, drawn by a software renderer) are left out: they take too long
# with sanitizers on.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SUPPRESSIONS="${REPO_ROOT}/tools/sanitizers"
BUILD_DIR="${1:?usage: run_tests.sh <build_dir> <address|thread|coverage>}"
KIND="${2:?usage: run_tests.sh <build_dir> <address|thread|coverage>}"
# The ctest tests run as well: the Ruby tests, the command lines of the relay and the server, the
# checks of what the programs carry, and the fuzz targets' corpora (where an input that once
# crashed a target, but not on every processor, is caught).
CTEST_TESTS='^(ruby|relay|server|app|layering|tools|fuzz)\.'

# (Each finding ends the program with an error, so that the test fails; symbols for the reports.)
COMMON="halt_on_error=1:print_stacktrace=1:symbolize=1"
export UBSAN_OPTIONS="${COMMON}:suppressions=${SUPPRESSIONS}/ubsan.supp"
case "${KIND}" in
  address)
    export ASAN_OPTIONS="${COMMON}:detect_leaks=1:strict_string_checks=1:check_initialization_order=1:strict_init_order=1"
    export LSAN_OPTIONS="suppressions=${SUPPRESSIONS}/lsan.supp:print_suppressions=0"
    ;;
  thread)
    export TSAN_OPTIONS="${COMMON}:second_deadlock_stack=1:suppressions=${SUPPRESSIONS}/tsan.supp"
    ;;
  # (No sanitizer: the same tests in a build that measures coverage.)
  coverage) ;;
  *)
    echo "unknown sanitizer ${KIND} (address, thread, coverage)" >&2
    exit 2
    ;;
esac

# (Each one runs even when one before it failed: one finding does not hide the others.)
status=0
"${BUILD_DIR}/tests/redgem_tests" || status=1
"${BUILD_DIR}/tests/redgem_testing_tests" || status=1
ctest --test-dir "${BUILD_DIR}" --output-on-failure -j 2 -R "${CTEST_TESTS}" || status=1
exit "${status}"
