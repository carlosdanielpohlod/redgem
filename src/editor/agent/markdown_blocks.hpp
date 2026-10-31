#pragma once

// The markdown agents answer in, as blocks the Agent panel draws: paragraphs, headings, fenced code,
// bulleted and numbered list items, quotes and rules, with **bold**, *italic* and `code` inside
// their text. Enough for what agents write; tables and links are shown as their text.

#include <string>
#include <string_view>
#include <vector>

namespace redgem::agent {

struct Span {
  std::string text;
  bool bold = false;
  bool italic = false;
  bool code = false;
};

enum class BlockKind { Paragraph, Heading, Code, Bullet, Numbered, Quote, Rule };

struct Block {
  BlockKind kind = BlockKind::Paragraph;
  int level = 0;            // a heading's (1 for #), a list item's depth (0 at the margin)
  std::string number;       // a numbered item's ("3")
  std::string language;     // a code block's, as its fence names it
  std::string code;         // a code block's text, as written
  std::vector<Span> spans;  // the text of the others
};

std::vector<Block> parse_markdown(std::string_view text);
// The inline styles of one block's text.
std::vector<Span> parse_spans(std::string_view text);

}  // namespace redgem::agent
