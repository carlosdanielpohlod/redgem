#include <doctest/doctest.h>

#include <random>
#include <string>
#include <utility>
#include <vector>

#include "editor/scripts/ruby_tokens.hpp"
#include "editor/ui/code_colors.hpp"

using namespace redgem;
using Kind = RubyTokenKind;

namespace {

using Colored = std::vector<std::pair<std::string, Kind>>;

// The tokens of the code, as the text each one covers and its kind.
Colored colored(const std::string& code) {
  Colored result;
  for (const RubyToken& token : ruby_tokens(code)) {
    result.emplace_back(code.substr(token.begin, token.end - token.begin), token.kind);
  }
  return result;
}

bool has(const Colored& tokens, const std::string& text, Kind kind) {
  for (const auto& [token_text, token_kind] : tokens) {
    if (token_text == text && token_kind == kind) return true;
  }
  return false;
}

bool has_text(const Colored& tokens, const std::string& text) {
  for (const auto& token : tokens) {
    if (token.first == text) return true;
  }
  return false;
}

// Tokens in order, apart, not empty and inside the code.
bool well_formed(const std::vector<RubyToken>& tokens, std::size_t size) {
  std::size_t last_end = 0;
  for (const RubyToken& token : tokens) {
    if (token.begin < last_end || token.end <= token.begin || token.end > size) return false;
    last_end = token.end;
  }
  return true;
}

constexpr int kRandomInputs = 2000;
constexpr int kLongestRandomInput = 64;
constexpr unsigned kRandomSeed = 7;
constexpr int kLargeFileCopies = 2000;
// The characters Ruby gives meaning to, so that random code hits every rule of the scanner.
constexpr std::string_view kRubyCharacters = "\"'`#{}()[]<>%/?:@$=~-\\\n \tabcAZ09_.!,&|*+^;e";

}  // namespace

TEST_CASE("Ruby keywords, constants, numbers and comments get their colors") {
  const Colored tokens = colored(
      "class SpinTree < Behaviour\n"
      "  SPEED = 1_000.5e-3 # per second\n"
      "  def update(dt) = nil\n"
      "end\n");
  CHECK(has(tokens, "class", Kind::Keyword));
  CHECK(has(tokens, "SpinTree", Kind::Constant));
  CHECK(has(tokens, "Behaviour", Kind::Constant));
  CHECK(has(tokens, "SPEED", Kind::Constant));
  CHECK(has(tokens, "1_000.5e-3", Kind::Number));
  CHECK(has(tokens, "# per second", Kind::Comment));
  CHECK(has(tokens, "def", Kind::Keyword));
  CHECK(has(tokens, "nil", Kind::Keyword));
  CHECK(has(tokens, "end", Kind::Keyword));
  CHECK_FALSE(has_text(tokens, "update"));  // method names keep the text color
  CHECK_FALSE(has_text(tokens, "dt"));
}

TEST_CASE("a # inside a string is not a comment, and #{} colors the code inside it") {
  const Colored tokens = colored("puts \"a # b #{@count + 1} c\" # done\n");
  CHECK(has(tokens, "\"a # b ", Kind::String));
  CHECK(has(tokens, "#{", Kind::Interpolation));
  CHECK(has(tokens, "@count", Kind::Variable));
  CHECK(has(tokens, "1", Kind::Number));
  CHECK(has(tokens, "}", Kind::Interpolation));
  CHECK(has(tokens, " c\"", Kind::String));
  CHECK(has(tokens, "# done", Kind::Comment));
}

TEST_CASE("single quotes do not interpolate, and braces nest inside an interpolation") {
  CHECK(colored("x = 'a #{b} \\' c'") == Colored{{"'a #{b} \\' c'", Kind::String}});
  const Colored tokens = colored("\"#{list.map { |x| x }.size} items\"");
  CHECK(has(tokens, "}", Kind::Interpolation));
  CHECK(has(tokens, " items\"", Kind::String));
}

TEST_CASE("heredoc bodies are strings up to their closing word") {
  const Colored tokens = colored(
      "text = <<~EOS.strip\n"
      "  if \"#{name}\" then\n"
      "  EOS\n"
      "if done\n");
  CHECK(has(tokens, "<<~EOS", Kind::String));
  CHECK(has(tokens, "#{", Kind::Interpolation));
  CHECK(has(tokens, "\" then\n  EOS", Kind::String));  // the body's last line, then the closing word
  CHECK_FALSE(has(tokens, "then", Kind::Keyword));
  CHECK(has(tokens, "if", Kind::Keyword));      // after the heredoc, code again
  CHECK(colored("list << item") == Colored{});  // an append, not a heredoc
}

TEST_CASE("a backslash ending a heredoc line does not hide the closing word") {
  CHECK(colored("path = <<~'EOS'\n  C:\\\nEOS\nputs path\n") ==
        Colored{{"<<~'EOS'", Kind::String}, {"  C:\\\nEOS", Kind::String}});
}

TEST_CASE("a call without parentheses may start with a literal") {
  CHECK(colored("raise <<MSG\nCan't load\nMSG\n") ==
        Colored{{"<<MSG", Kind::String}, {"Can't load\nMSG", Kind::String}});
  CHECK(colored("p %w[a b]") == Colored{{"%w[a b]", Kind::String}});
  CHECK(colored("line.split /,/") == Colored{{"/,/", Kind::Regex}});
  CHECK(colored("class <<self\nend") ==
        Colored{{"class", Kind::Keyword}, {"self", Kind::Keyword}, {"end", Kind::Keyword}});
  // A slash taken for a regex by mistake colors no more than the rest of its line.
  CHECK(colored("half = width /2\nend") == Colored{{"/2", Kind::Regex}, {"end", Kind::Keyword}});
}

TEST_CASE("yield, super and __ENCODING__ are values") {
  CHECK(colored("total = yield / 2 + super % 3") ==
        Colored{{"yield", Kind::Keyword}, {"2", Kind::Number}, {"super", Kind::Keyword}, {"3", Kind::Number}});
  CHECK(colored("__ENCODING__") == Colored{{"__ENCODING__", Kind::Keyword}});
}

TEST_CASE("method names may end in ? and !") {
  const Colored tokens = colored("return if empty? || defined?(x)\nsave!\nx != y");
  CHECK(has(tokens, "defined?", Kind::Keyword));
  CHECK_FALSE(has_text(tokens, "empty"));
  CHECK_FALSE(has_text(tokens, "?"));
  CHECK_FALSE(has_text(tokens, "!"));
}

TEST_CASE("symbols are told apart from ternaries, scopes and hash keys") {
  CHECK(colored("a ? b : c") == Colored{});
  CHECK(colored("ready ? :go : :wait") == Colored{{":go", Kind::Symbol}, {":wait", Kind::Symbol}});
  CHECK(colored("{speed: 1, :name => :\"x y\"}") ==
        Colored{{"speed:", Kind::Symbol}, {"1", Kind::Number}, {":name", Kind::Symbol}, {":\"x y\"", Kind::Symbol}});
  CHECK(colored("Game::Scene") == Colored{{"Game", Kind::Constant}, {"Scene", Kind::Constant}});
  CHECK(colored("self.class") == Colored{{"self", Kind::Keyword}});  // after a dot, a method name
}

TEST_CASE("=begin and =end at line starts make a block comment") {
  const Colored tokens = colored("x = 1\n=begin\nend if\n=end\ny");
  CHECK(has(tokens, "=begin\nend if\n=end", Kind::Comment));
  CHECK_FALSE(has(tokens, "end", Kind::Keyword));
  CHECK(colored("a =begin") == Colored{{"begin", Kind::Keyword}});  // not at a line start
}

TEST_CASE("%-literals and character literals") {
  CHECK(colored("[%w[a b], %i(c d), %q{e {f}}, %(g)]") ==
        Colored{
            {"%w[a b]", Kind::String}, {"%i(c d)", Kind::Symbol}, {"%q{e {f}}", Kind::String}, {"%(g)", Kind::String}});
  CHECK(colored("a % b") == Colored{});
  CHECK(colored("x = ?a") == Colored{{"?a", Kind::String}});
}

TEST_CASE("a slash where a value goes opens a regex; after a value it divides") {
  CHECK(colored("ok = name =~ /^a#{b}c$/i") ==
        Colored{{"/^a", Kind::Regex}, {"#{", Kind::Interpolation}, {"}", Kind::Interpolation}, {"c$/i", Kind::Regex}});
  CHECK(colored("half = total / 2 / count") == Colored{{"2", Kind::Number}});
  CHECK(colored("%r{a/b}") == Colored{{"%r{a/b}", Kind::Regex}});
}

TEST_CASE("an unterminated string runs to the end of the code") {
  CHECK(colored("puts \"abc\nend") == Colored{{"\"abc\nend", Kind::String}});
  CHECK(colored("x = \"a #{b") == Colored{{"\"a ", Kind::String}, {"#{", Kind::Interpolation}});
  CHECK(colored("y = 'tail\\") == Colored{{"'tail\\", Kind::String}});
}

TEST_CASE("any input gives well-formed tokens") {
  std::mt19937 random(kRandomSeed);
  std::uniform_int_distribution<int> length(0, kLongestRandomInput);
  std::uniform_int_distribution<std::size_t> pick(0, kRubyCharacters.size() - 1);
  for (int i = 0; i < kRandomInputs; ++i) {
    std::string code;
    for (int n = length(random); n > 0; --n) code += kRubyCharacters[pick(random)];
    INFO(code);
    REQUIRE(well_formed(ruby_tokens(code), code.size()));
  }
  const std::string quotes(kLongestRandomInput * kLongestRandomInput, '"');
  std::string nested;
  for (int i = 0; i < kRandomInputs; ++i) nested += "\"#{";  // deeper than the scanner follows
  CHECK(well_formed(ruby_tokens(nested), nested.size()));
  CHECK(well_formed(ruby_tokens(quotes), quotes.size()));
}

TEST_CASE("the highlight is worked out again only when the code changes") {
  std::string code;
  for (int i = 0; i < kLargeFileCopies; ++i) code += "def a(x) = \"#{x}\" # n\n";
  RubyHighlightCache cache;
  const RubyHighlight& first = cache.of(code);
  const std::vector<RubyToken> tokens = first.tokens;
  CHECK(first.line_starts.size() == static_cast<std::size_t>(kLargeFileCopies) + 1);
  CHECK(cache.of(code).tokens.data() == first.tokens.data());  // the same, not worked out again
  code += "end";
  CHECK(cache.of(code).tokens.size() == tokens.size() + 1);
  CHECK(cache.of(code).line_starts.back() == code.size() - std::string("end").size());
}

TEST_CASE("every kind of Ruby code has a color of its own, none the plain text's") {
  const Kind kinds[] = {Kind::Keyword, Kind::String, Kind::Interpolation, Kind::Symbol,  Kind::Regex,
                        Kind::Comment, Kind::Number, Kind::Constant,      Kind::Variable};
  const auto same = [](const ImVec4& a, const ImVec4& b) { return a.x == b.x && a.y == b.y && a.z == b.z; };
  const ImVec4 text = ui::code_color(static_cast<Kind>(-1));  // no kind: the text color
  for (const Kind a : kinds) {
    CHECK_FALSE(same(ui::code_color(a), text));
    for (const Kind b : kinds) CHECK((a == b) == same(ui::code_color(a), ui::code_color(b)));
  }
}
