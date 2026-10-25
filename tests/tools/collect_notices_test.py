"""tools/licenses/collect_notices.py: the notices are made from the dependencies' license files
wherever the build keeps them, and the check fails on a committed file that differs."""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "licenses"))
import collect_notices  # noqa: E402

# What a stand-in license file says, for every project.
LICENSE_TEXT = "Permission is granted."


def fake_deps(root):
    """Every license file the projects name under root, each holding LICENSE_TEXT."""
    for _, _, _, license_file in collect_notices.PROJECTS:
        if not license_file.startswith(collect_notices.DEPS):
            continue
        path = Path(license_file.replace(collect_notices.DEPS, root))
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(LICENSE_TEXT, encoding="utf-8")


class CollectNoticesTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        fake_deps(self.folder.name)
        self.text = collect_notices.notices(self.folder.name)

    def test_the_license_files_are_read_from_the_folder_given(self):
        self.assertIn(f"### raylib\n\n```\n{LICENSE_TEXT}\n```", self.text)

    def test_a_file_that_matches_passes_the_check(self):
        committed = Path(self.folder.name) / "notices.md"
        committed.write_text(self.text, encoding="utf-8")
        collect_notices.check(self.text, str(committed))

    def test_a_stale_file_fails_the_check(self):
        committed = Path(self.folder.name) / "notices.md"
        committed.write_text(self.text + "an old line\n", encoding="utf-8")
        with self.assertRaises(SystemExit) as stop:
            collect_notices.check(self.text, str(committed))
        self.assertEqual(stop.exception.code, collect_notices.EXIT_STALE)


if __name__ == "__main__":
    unittest.main()
