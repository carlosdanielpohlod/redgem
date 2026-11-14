"""tools/bench/check_thresholds.py: results are held to their ceilings, a value fails only past the
tolerance, a missing result fails, and the committed thresholds file is one it can read."""

import json
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "tools" / "bench"))
import check_thresholds  # noqa: E402

CEILING = 10.0
TOLERANCE = 0.5
UNDER = 9.0
WITHIN_TOLERANCE = 14.0  # over the ceiling, under the ceiling plus half of it
PAST_TOLERANCE = 16.0
FAILURE = 1


def thresholds(**ceilings):
    return {"tolerance": TOLERANCE, "ceilings": ceilings}


def result(median, p95=None):
    return {"median": median, "p95": median if p95 is None else p95}


def statuses(results, limits):
    return [row[-1] for row in check_thresholds.check(results, limits)]


class VerdictTest(unittest.TestCase):
    def test_a_value_under_its_ceiling_passes(self):
        self.assertEqual(check_thresholds.verdict(UNDER, CEILING, TOLERANCE), check_thresholds.PASSED)

    def test_a_value_within_the_tolerance_warns(self):
        self.assertEqual(check_thresholds.verdict(WITHIN_TOLERANCE, CEILING, TOLERANCE), check_thresholds.WARNED)

    def test_a_value_past_the_tolerance_fails(self):
        self.assertEqual(check_thresholds.verdict(PAST_TOLERANCE, CEILING, TOLERANCE), check_thresholds.FAILED)

    def test_a_ceiling_of_zero_fails_anything_over_it(self):
        self.assertEqual(check_thresholds.verdict(1, 0, TOLERANCE), check_thresholds.FAILED)
        self.assertEqual(check_thresholds.verdict(0, 0, TOLERANCE), check_thresholds.PASSED)


class CheckTest(unittest.TestCase):
    def test_each_statistic_a_ceiling_names_is_checked(self):
        limits = thresholds(a={"median": CEILING, "p95": CEILING})
        found = statuses({"a": result(UNDER, PAST_TOLERANCE)}, limits)
        self.assertEqual(found, [check_thresholds.PASSED, check_thresholds.FAILED])

    def test_a_statistic_without_a_ceiling_is_not_checked(self):
        found = statuses({"a": result(UNDER, PAST_TOLERANCE)}, thresholds(a={"median": CEILING}))
        self.assertEqual(found, [check_thresholds.PASSED])

    def test_a_ceiling_without_its_result_fails(self):
        self.assertEqual(statuses({}, thresholds(a={"median": CEILING})), [check_thresholds.MISSING])

    def test_a_result_without_a_ceiling_passes(self):
        self.assertEqual(statuses({"new": result(PAST_TOLERANCE)}, thresholds()), [check_thresholds.UNCHECKED])

    def test_a_result_measured_twice_is_refused(self):
        documents = [{"results": {"a": result(UNDER)}}, {"results": {"a": result(UNDER)}}]
        with self.assertRaises(check_thresholds.ThresholdError):
            check_thresholds.merged_results(documents)


class RunTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)

    def write(self, name, document):
        path = Path(self.folder.name) / name
        path.write_text(json.dumps(document), encoding="utf-8")
        return str(path)

    def run_check(self, measured):
        limits = self.write("thresholds.json", thresholds(a={"median": CEILING}, b={"median": CEILING}))
        bench = self.write("bench.json", {"suite": "bench", "results": {"a": result(measured)}})
        load = self.write("load.json", {"suite": "load", "results": {"b": result(UNDER)}})
        summary = Path(self.folder.name) / "summary.md"
        return check_thresholds.run([bench, load], limits, str(summary)), summary.read_text(encoding="utf-8")

    def test_results_of_several_files_within_their_ceilings_pass(self):
        code, summary = self.run_check(WITHIN_TOLERANCE)
        self.assertEqual(code, 0)
        self.assertIn("| a | median | 14.000 | 10.000 |", summary)

    def test_one_result_past_its_tolerance_fails_the_run(self):
        code, _ = self.run_check(PAST_TOLERANCE)
        self.assertEqual(code, FAILURE)

    def test_an_unreadable_file_is_an_error(self):
        with self.assertRaises(check_thresholds.ThresholdError):
            check_thresholds.run([str(Path(self.folder.name) / "none.json")], check_thresholds.DEFAULT_THRESHOLDS)


class CommittedThresholdsTest(unittest.TestCase):
    def test_every_ceiling_is_a_number_for_a_known_statistic(self):
        limits = check_thresholds.read_json(check_thresholds.DEFAULT_THRESHOLDS)
        self.assertGreaterEqual(limits["tolerance"], 0)
        for name, ceilings in limits["ceilings"].items():
            with self.subTest(name=name):
                self.assertTrue(ceilings)
                self.assertTrue(set(ceilings) <= set(check_thresholds.STATISTICS))
                self.assertTrue(all(isinstance(value, (int, float)) for value in ceilings.values()))


if __name__ == "__main__":
    unittest.main()
