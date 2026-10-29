// Agents' markdown as the blocks and inline styles the Agent panel draws.
#include <doctest/doctest.h>

#include <string>

#include "editor/agent/markdown_blocks.hpp"
#include "editor/agent/text_diff.hpp"

using namespace redgem::agent;

namespace {

std::string plain(const std::vector<Span>& spans) {
  std::string text;
  for (const Span& span : spans) text += span.text;
  return text;
}

}  // namespace

TEST_CASE("paragraphs are lines of text between blank lines, joined") {
  const auto blocks = parse_markdown("First line\nsame paragraph.\n\nSecond one.");
  REQUIRE(blocks.size() == 2);
  CHECK(blocks[0].kind == BlockKind::Paragraph);
  CHECK(plain(blocks[0].spans) == "First line same paragraph.");
  CHECK(plain(blocks[1].spans) == "Second one.");
}

TEST_CASE("headings, rules and quotes") {
  const auto blocks = parse_markdown("## Plan\n---\n> careful\nText");
  REQUIRE(blocks.size() == 3);
  CHECK(blocks[0].kind == BlockKind::Heading);
  CHECK(blocks[0].level == 2);
  CHECK(plain(blocks[0].spans) == "Plan");
  CHECK(blocks[1].kind == BlockKind::Rule);
  CHECK(blocks[2].kind == BlockKind::Quote);
  CHECK(plain(blocks[2].spans) == "careful Text");  // (a line of text goes on the quote before it)
  CHECK(parse_markdown("#nospace")[0].kind == BlockKind::Paragraph);
}

TEST_CASE("fenced code keeps its text as written, with its language") {
  const auto blocks = parse_markdown("Look:\n```ruby\ndef start\n  @speed = 2 # **not bold**\nend\n```\nAfter.");
  REQUIRE(blocks.size() == 3);
  CHECK(blocks[1].kind == BlockKind::Code);
  CHECK(blocks[1].language == "ruby");
  CHECK(blocks[1].code == "def start\n  @speed = 2 # **not bold**\nend");
  CHECK(plain(blocks[2].spans) == "After.");
}

TEST_CASE("a code block still being streamed runs to the end of the text") {
  const auto blocks = parse_markdown("```\nline one\n");
  REQUIRE(blocks.size() == 1);
  CHECK(blocks[0].code == "line one\n");
}

TEST_CASE("bulleted and numbered items, nested by their indentation") {
  const auto blocks = parse_markdown("- one\n  - inner\n* two\n  continued\n1. first\n12) twelfth");
  REQUIRE(blocks.size() == 5);
  CHECK(blocks[0].kind == BlockKind::Bullet);
  CHECK(blocks[0].level == 0);
  CHECK(blocks[1].level == 1);
  CHECK(plain(blocks[2].spans) == "two continued");
  CHECK(blocks[3].kind == BlockKind::Numbered);
  CHECK(blocks[3].number == "1");
  CHECK(blocks[4].number == "12");
  CHECK(plain(blocks[4].spans) == "twelfth");
}

TEST_CASE("inline styles: bold, italic and code, and escaped marks") {
  const auto spans = parse_spans("Use **Scene.find** with `name` *now* \\*literally\\*");
  REQUIRE(spans.size() == 7);
  CHECK(spans[0].text == "Use ");
  CHECK(spans[1].bold);
  CHECK(spans[1].text == "Scene.find");
  CHECK(spans[3].code);
  CHECK(spans[3].text == "name");
  CHECK(spans[5].italic);
  CHECK(spans[6].text == " *literally*");
  CHECK_FALSE(spans[6].italic);
}

TEST_CASE("inside code nothing else is a mark") {
  const auto spans = parse_spans("`a * b ** c`");
  REQUIRE(spans.size() == 1);
  CHECK(spans[0].code);
  CHECK(spans[0].text == "a * b ** c");
}

TEST_CASE("Windows line ends and empty text") {
  CHECK(plain(parse_markdown("a\r\nb\r\n")[0].spans) == "a b");
  CHECK(parse_markdown("").empty());
}

TEST_CASE("a change shows as the lines taken out and put in, with a little around them") {
  const auto lines = diff_lines("a\nb\nc\nd\ne\nf\n", "a\nb\nc\nD\ne\nf\n");
  REQUIRE(lines.size() == 6);
  CHECK(lines[0].mark == DiffMark::Kept);
  CHECK(lines[0].text == "b");
  CHECK(lines[2].mark == DiffMark::Removed);
  CHECK(lines[2].text == "d");
  CHECK(lines[3].mark == DiffMark::Added);
  CHECK(lines[3].text == "D");
  CHECK(lines[5].text == "f");
}

TEST_CASE("a new file is all added; a huge change is cut short") {
  const auto added = diff_lines("", "x\ny\n");
  REQUIRE(added.size() == 2);
  CHECK(added[1].mark == DiffMark::Added);
  std::string many;
  for (std::size_t line = 0; line < kMaxDiffLines + 5; ++line) many += std::to_string(line) + "\n";
  const auto cut = diff_lines("", many);
  REQUIRE(cut.size() == kMaxDiffLines + 1);
  CHECK(cut.back().mark == DiffMark::Skipped);
  CHECK(cut.back().text == "5");
}
