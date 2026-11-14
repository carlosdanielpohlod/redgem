#!/usr/bin/env python3
"""Holds what the benchmarks (redgem_bench) and the server load test (redgem_server_load)
measured to the ceilings in tests/bench/thresholds.json, and fails when one is clearly exceeded.

    python3 -I tools/bench/check_thresholds.py bench.json load.json
    python3 -I tools/bench/check_thresholds.py --thresholds tests/bench/thresholds.json \\
        --summary "$GITHUB_STEP_SUMMARY" bench-results/*.json

The ceilings are generous on purpose: CI machines are shared and noisy. A value over its ceiling
is a warning; only one over the ceiling plus the file's tolerance (a share of the ceiling) fails
the check, as does a ceiling whose result is missing (a benchmark that no longer runs). A result
with no ceiling is listed and passes. It prints a Markdown table, and appends it to --summary.
"""
import argparse
import json
import os
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_THRESHOLDS = os.path.join(REPO_ROOT, "tests", "bench", "thresholds.json")
# The statistics a ceiling may hold a result to.
STATISTICS = ("median", "p95")

PASSED = "ok"
WARNED = "over ceiling (within tolerance)"
FAILED = "FAILED"
MISSING = "FAILED: no result"
UNCHECKED = "no ceiling"


class ThresholdError(Exception):
    """A results or thresholds file that cannot be used."""


def read_json(path):
    try:
        with open(path, encoding="utf-8") as file:
            return json.load(file)
    except (OSError, ValueError) as error:
        raise ThresholdError(f"cannot read {path}: {error}") from error


def merged_results(documents):
    """Every result of the documents, by name; a name measured twice is an error."""
    results = {}
    for document in documents:
        for name, result in document.get("results", {}).items():
            if name in results:
                raise ThresholdError(f"{name} is in more than one results file")
            results[name] = result
    return results


def verdict(value, ceiling, tolerance):
    """PASSED, WARNED or FAILED for a value held to a ceiling."""
    if value > ceiling * (1.0 + tolerance):
        return FAILED
    return WARNED if value > ceiling else PASSED


def check_one(name, ceilings, result, tolerance):
    """The rows of one ceiling: one per statistic it names, or one saying the result is missing."""
    if result is None:
        return [(name, "-", None, None, MISSING)]
    rows = []
    for statistic in STATISTICS:
        if statistic not in ceilings:
            continue
        value = result.get(statistic)
        status = MISSING if value is None else verdict(value, ceilings[statistic], tolerance)
        rows.append((name, statistic, value, ceilings[statistic], status))
    return rows


def check(results, thresholds):
    """Rows of (name, statistic, value, ceiling, status) for every ceiling and every result."""
    tolerance = thresholds.get("tolerance", 0.0)
    ceilings = thresholds.get("ceilings", {})
    rows = []
    for name, limits in ceilings.items():
        rows.extend(check_one(name, limits, results.get(name), tolerance))
    for name in results:
        if name not in ceilings:
            rows.append((name, "median", results[name].get("median"), None, UNCHECKED))
    return rows


def failed(rows):
    return [row for row in rows if row[-1].startswith(FAILED)]


def number(value):
    return "-" if value is None else f"{value:.3f}"


def table(rows, tolerance):
    lines = [
        f"Benchmarks: a value fails over its ceiling plus {tolerance:.0%}.",
        "",
        "| name | statistic | value | ceiling | status |",
        "|---|---|---|---|---|",
    ]
    for name, statistic, value, ceiling, status in rows:
        lines.append(f"| {name} | {statistic} | {number(value)} | {number(ceiling)} | {status} |")
    return "\n".join(lines) + "\n"


def run(result_paths, thresholds_path, summary_path=None):
    """Checks the results files against the thresholds file: the exit code, 0 when nothing failed."""
    thresholds = read_json(thresholds_path)
    rows = check(merged_results(read_json(path) for path in result_paths), thresholds)
    text = table(rows, thresholds.get("tolerance", 0.0))
    print(text)
    if summary_path:
        with open(summary_path, "a", encoding="utf-8") as summary:
            summary.write(text)
    for name, statistic, *_ in failed(rows):
        print(f"{name} {statistic}: over its ceiling", file=sys.stderr)
    return 1 if failed(rows) else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("results", nargs="+", help="results files written by redgem_bench or redgem_server_load")
    parser.add_argument("--thresholds", default=DEFAULT_THRESHOLDS, help="the ceilings (tests/bench/thresholds.json)")
    parser.add_argument("--summary", help="a Markdown file to append the table to ($GITHUB_STEP_SUMMARY)")
    arguments = parser.parse_args()
    try:
        return run(arguments.results, arguments.thresholds, arguments.summary)
    except ThresholdError as error:
        print(error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
