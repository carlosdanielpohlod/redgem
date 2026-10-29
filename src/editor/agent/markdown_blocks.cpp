#include "editor/agent/markdown_blocks.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <utility>

namespace redgem::agent {
namespace {

constexpr std::string_view kFence = "```";
constexpr char kHeadingMark = '#';
constexpr int kMaxHeadingLevel = 6;
constexpr std::string_view kQuoteMark = ">";
constexpr std::size_t kMinRuleMarks = 3;
constexpr std::size_t kSpacesPerDepth = 2;  // how far a nested list item is set in
constexpr char kCodeMark = '`';
constexpr char kEmphasisMark = '*';
constexpr char kEscape = '\\';
constexpr std::string_view kBulletMarks = "-*+";
constexpr std::size_t kMarkAndSpace = 2;  // "- " before an item's text, ". " after its number
constexpr std::string_view kNumberEnds = ".)";
constexpr std::string_view kRuleMarks = "-*_";
constexpr char kSpace = ' ';
constexpr char kLineEnd = '\n';
constexpr char kCarriageReturn = '\r';

std::size_t indentation(std::string_view line) {
  const std::size_t first = line.find_first_not_of(kSpace);
  return first == std::string_view::npos ? line.size() : first;
}

bool ends_in_space(std::string_view line) {
  return !line.empty() && std::isspace(static_cast<unsigned char>(line.back()));
}

std::string_view trimmed(std::string_view line) {
  line.remove_prefix(indentation(line));
  while (ends_in_space(line)) line.remove_suffix(1);
  return line;
}

// A line as written, without the carriage return of a Windows line end.
bool ends_with_return(std::string_view line) { return !line.empty() && line.back() == kCarriageReturn; }

std::string_view without_return(std::string_view line) {
  if (ends_with_return(line)) line.remove_suffix(1);
  return line;
}

bool blank(std::string_view line) { return trimmed(line).empty(); }

bool starts_with(std::string_view text, std::string_view start) { return text.substr(0, start.size()) == start; }

bool is_fence(std::string_view line) { return starts_with(trimmed(line), kFence); }

bool one_mark_repeated(std::string_view text) {
  std::size_t marks = 0;
  for (const char character : text) {
    if (character == text.front())
      ++marks;
    else if (character != kSpace)
      return false;
  }
  return marks >= kMinRuleMarks;
}

bool starts_with_rule_mark(std::string_view text) {
  return !text.empty() && kRuleMarks.find(text.front()) != std::string_view::npos;
}

bool is_rule(std::string_view line) {
  const std::string_view text = trimmed(line);
  return starts_with_rule_mark(text) && one_mark_repeated(text);
}

// "##" then a space: a heading of the level of the marks before the space.
bool heading_mark(std::string_view text, std::size_t level) {
  return level != std::string_view::npos && level > 0 && static_cast<int>(level) <= kMaxHeadingLevel &&
         text[level] == kSpace;
}

// "## Title": its level and text.
std::optional<Block> heading(std::string_view line) {
  const std::string_view text = trimmed(line);
  const std::size_t level = text.find_first_not_of(kHeadingMark);
  if (!heading_mark(text, level)) return std::nullopt;
  Block block;
  block.kind = BlockKind::Heading;
  block.level = static_cast<int>(level);
  block.code = std::string(trimmed(text.substr(level)));
  return block;
}

bool bullet_mark(std::string_view text) {
  return text.size() > 1 && kBulletMarks.find(text[0]) != std::string_view::npos && text[1] == kSpace;
}

// "- item" or "  * item" (one level deeper).
std::optional<Block> bullet(std::string_view line) {
  const std::string_view text = line.substr(indentation(line));
  if (!bullet_mark(text)) return std::nullopt;
  Block block;
  block.kind = BlockKind::Bullet;
  block.level = static_cast<int>(indentation(line) / kSpacesPerDepth);
  block.code = std::string(trimmed(text.substr(kMarkAndSpace)));
  return block;
}

std::size_t digits_at_start(std::string_view text) {
  std::size_t count = 0;
  while (count < text.size() && std::isdigit(static_cast<unsigned char>(text[count]))) ++count;
  return count;
}

bool number_mark(std::string_view text, std::size_t digits) {
  return digits > 0 && text.size() > digits + 1 && kNumberEnds.find(text[digits]) != std::string_view::npos &&
         text[digits + 1] == kSpace;
}

// "3. item" or "3) item".
std::optional<Block> numbered(std::string_view line) {
  const std::string_view text = line.substr(indentation(line));
  const std::size_t digits = digits_at_start(text);
  if (!number_mark(text, digits)) return std::nullopt;
  Block block;
  block.kind = BlockKind::Numbered;
  block.level = static_cast<int>(indentation(line) / kSpacesPerDepth);
  block.number = std::string(text.substr(0, digits));
  block.code = std::string(trimmed(text.substr(digits + kMarkAndSpace)));
  return block;
}

std::optional<Block> quote(std::string_view line) {
  const std::string_view text = trimmed(line);
  if (!starts_with(text, kQuoteMark)) return std::nullopt;
  Block block;
  block.kind = BlockKind::Quote;
  block.code = std::string(trimmed(text.substr(kQuoteMark.size())));
  return block;
}

// Whether lines of text after a block go on it: not after a rule or a heading.
bool takes_more_lines(BlockKind kind) { return kind != BlockKind::Rule && kind != BlockKind::Heading; }

// A line that starts a block of its own (other than a paragraph or code), else nullopt.
std::optional<Block> marked_block(std::string_view line) {
  if (is_rule(line)) return Block{BlockKind::Rule, 0, "", "", "", {}};
  if (auto block = heading(line)) return block;
  if (auto block = bullet(line)) return block;
  if (auto block = numbered(line)) return block;
  return quote(line);
}

// Blocks are put together with their raw text in `code`; all but code blocks have it turned into
// spans at the end.
class Builder {
 public:
  void take(std::string_view line) {
    if (in_code_) return code_line(line);
    if (is_fence(line)) return open_code(line);
    if (blank(line)) return close();
    if (auto block = marked_block(line)) return start(std::move(*block));
    continue_text(line);
  }

  std::vector<Block> finish() {
    for (Block& block : blocks_) {
      if (block.kind != BlockKind::Code) block.spans = parse_spans(std::exchange(block.code, std::string()));
    }
    return std::move(blocks_);
  }

 private:
  void code_line(std::string_view line) {
    if (is_fence(line)) {
      in_code_ = false;
      return close();
    }
    Block& code = blocks_.back();
    if (lines_in_code_ > 0) code.code += kLineEnd;
    code.code += line;
    ++lines_in_code_;
  }

  void open_code(std::string_view line) {
    Block block;
    block.kind = BlockKind::Code;
    block.language = std::string(trimmed(trimmed(line).substr(kFence.size())));
    blocks_.push_back(std::move(block));
    in_code_ = true;
    lines_in_code_ = 0;
  }

  void start(Block block) {
    open_ = takes_more_lines(block.kind);
    blocks_.push_back(std::move(block));
  }

  // Text that goes on the block before it (a paragraph or list item broken over lines), else starts
  // a paragraph.
  void continue_text(std::string_view line) {
    if (!open_) return start(Block{BlockKind::Paragraph, 0, "", "", std::string(trimmed(line)), {}});
    blocks_.back().code += kSpace;
    blocks_.back().code += trimmed(line);
  }

  void close() { open_ = false; }

  std::vector<Block> blocks_;
  bool open_ = false;  // the last block takes the next line of text
  bool in_code_ = false;
  std::size_t lines_in_code_ = 0;
};

// Inline styles: each mark turns its style on or off. Inside `code` nothing else is a mark.
class SpanReader {
 public:
  explicit SpanReader(std::string_view text) : text_(text) {}

  std::vector<Span> read() {
    for (index_ = 0; index_ < text_.size(); ++index_) take(text_[index_]);
    flush();
    return std::move(spans_);
  }

 private:
  void take(char character) {
    if (character == kCodeMark) return toggle(current_.code);
    if (current_.code) return keep(character);
    if (escapes_a_mark(character)) return keep(text_[++index_]);
    if (character != kEmphasisMark) return keep(character);
    if (next_is(kEmphasisMark)) {
      ++index_;
      return toggle(current_.bold);
    }
    toggle(current_.italic);
  }

  bool next_is(char character) const { return index_ + 1 < text_.size() && text_[index_ + 1] == character; }
  bool escapes_a_mark(char character) const {
    return character == kEscape && (next_is(kEmphasisMark) || next_is(kCodeMark) || next_is(kEscape));
  }
  void keep(char character) { current_.text += character; }

  void toggle(bool& style) {
    flush();
    style = !style;
  }

  void flush() {
    if (!current_.text.empty()) spans_.push_back(current_);
    current_.text.clear();
  }

  std::string_view text_;
  std::size_t index_ = 0;
  Span current_;
  std::vector<Span> spans_;
};

}  // namespace

std::vector<Block> parse_markdown(std::string_view text) {
  Builder builder;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t end = std::min(text.find(kLineEnd, start), text.size());
    builder.take(without_return(text.substr(start, end - start)));
    start = end + 1;
  }
  return builder.finish();
}

std::vector<Span> parse_spans(std::string_view text) { return SpanReader(text).read(); }

}  // namespace redgem::agent
