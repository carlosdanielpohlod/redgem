#!/usr/bin/env bash
# Runs every fuzz target of a build made with REDGEM_FUZZ=ON for a while, each on its seeds
# (tests/fuzz/corpus/<name>) and on what earlier runs found (in <work_dir>/corpus/<name>, which
# grows). A crash, a sanitizer finding, a leak, an input that takes too long or too much memory
# fails it; the input that did is left in <work_dir>/findings, to add to the target's corpus once
# the code is fixed, where its replay keeps it tested.
#   cmake -S . -B build-fuzz -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
#     -DCMAKE_BUILD_TYPE=RelWithDebInfo -DREDGEM_FUZZ=ON -DREDGEM_SANITIZERS=address,undefined
#   cmake --build build-fuzz --target redgem_fuzz_targets
#   tests/fuzz/run_fuzzers.sh build-fuzz 60 fuzz-work
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${1:?usage: run_fuzzers.sh <build_dir> <seconds_each> <work_dir>}"
SECONDS_EACH="${2:?usage: run_fuzzers.sh <build_dir> <seconds_each> <work_dir>}"
WORK_DIR="${3:?usage: run_fuzzers.sh <build_dir> <seconds_each> <work_dir>}"
TARGETS=(gltf image_header wire http_response json_scanner)
# One input taking longer than this (seconds), or more memory than this (MB), is a finding.
INPUT_TIMEOUT=10
MEMORY_LIMIT_MB=2048
# The longest input made up, in bytes (scene files need room; the others take what they get).
MAX_INPUT_BYTES=65536

export ASAN_OPTIONS="detect_leaks=1:halt_on_error=1"
export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1:suppressions=${REPO_ROOT}/tools/sanitizers/ubsan.supp"
export LSAN_OPTIONS="suppressions=${REPO_ROOT}/tools/sanitizers/lsan.supp"

# What the runs found, cut down to the fewest inputs that reach the same code: kept between runs,
# the corpus would otherwise only grow.
minimize() {
  local target="$1" corpus="${WORK_DIR}/corpus/$1" smaller="${WORK_DIR}/corpus/$1.smaller"
  rm -rf "${smaller}" && mkdir -p "${smaller}"
  "${BUILD_DIR}/tests/fuzz/redgem_fuzz_${target}" -merge=1 -timeout="${INPUT_TIMEOUT}" -rss_limit_mb="${MEMORY_LIMIT_MB}" \
    "${smaller}" "${corpus}" >/dev/null 2>&1 && rm -rf "${corpus}" && mv "${smaller}" "${corpus}"
}

mkdir -p "${WORK_DIR}/findings"
failed=()
for target in "${TARGETS[@]}"; do
  echo "== ${target} (${SECONDS_EACH} s)"
  mkdir -p "${WORK_DIR}/corpus/${target}"
  "${BUILD_DIR}/tests/fuzz/redgem_fuzz_${target}" -max_total_time="${SECONDS_EACH}" -timeout="${INPUT_TIMEOUT}" \
    -rss_limit_mb="${MEMORY_LIMIT_MB}" -max_len="${MAX_INPUT_BYTES}" -print_final_stats=1 \
    -artifact_prefix="${WORK_DIR}/findings/${target}-" \
    "${WORK_DIR}/corpus/${target}" "${REPO_ROOT}/tests/fuzz/corpus/${target}" || failed+=("${target}")
  minimize "${target}"
done

if [[ ${#failed[@]} -gt 0 ]]; then
  echo "findings in: ${failed[*]} (inputs in ${WORK_DIR}/findings)" >&2
  exit 1
fi
