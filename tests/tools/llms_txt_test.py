"""tools/docs/llms_txt.py: the manual's index for coding assistants lists every page of the
navigation with what it is about, and the full text holds every page without its front matter."""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "docs"))
import llms_txt  # noqa: E402


class LlmsTxtTest(unittest.TestCase):
    def setUp(self):
        self.manual = llms_txt.read_manual()

    def test_every_page_of_the_navigation_is_listed_with_its_description(self):
        index = llms_txt.index_text(self.manual)
        self.assertIn("## Scripting in Ruby", index)
        api_page = "https://carlosdanielpohlod.github.io/redgem-site/manual/scripting/api/"
        self.assertIn(f"- [API reference]({api_page}): ", index)
        for page in self.manual.pages:
            self.assertIn(f"[{page.title}](", index)
            self.assertTrue(llms_txt.description_of(self.manual, page), page.path)

    def test_pages_beside_the_sections_are_in_none_of_them(self):
        index = llms_txt.index_text(self.manual)
        sections = index.split("\n## ")
        self.assertNotIn("troubleshooting/", next(part for part in sections if part.startswith("Finishing a game")))
        self.assertIn("troubleshooting/", sections[-1])

    def test_a_quoted_description_reads_without_its_quotes(self):
        self.assertEqual(llms_txt.unquoted('"a: \\"b\\""'), 'a: "b"')

    def test_the_full_text_is_every_page_without_its_front_matter(self):
        full = llms_txt.full_text(self.manual)
        self.assertNotIn("\ndescription: ", full)
        self.assertIn("Page: https://carlosdanielpohlod.github.io/redgem-site/manual/command-line/", full)
        self.assertEqual(full.count("\nPage: "), len(self.manual.pages))

    def test_a_page_without_a_description_is_described_by_its_first_sentence(self):
        self.assertEqual(llms_txt.first_sentence("# Title\n\nOne [thing](x.md). Two.\n"), "One thing.")

    def test_both_files_are_written(self):
        with tempfile.TemporaryDirectory() as out:
            self.assertEqual(llms_txt.main([out]), 0)
            self.assertTrue((Path(out) / "llms.txt").read_text(encoding="utf-8").startswith("# RedGem"))
            self.assertTrue((Path(out) / "llms-full.txt").exists())


if __name__ == "__main__":
    unittest.main()
