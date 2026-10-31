// A Ruby lexer good enough to color code while it is typed. It reads the code once, left to right,
// and guesses the few things only a parser knows (is this "/" a division or a regex? this "?" a
// ternary or a character?) from what came just before, as editors do.

#include "editor/scripts/ruby_tokens.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
#include <utility>

namespace redgem {
namespace {

using Kind = RubyTokenKind;

// Strings in interpolations in strings...: past this depth "#{" is plain text, so that no input
// can run the scanner's recursion out of stack.
constexpr std::size_t kMaxNesting = 32;
constexpr unsigned char kFirstNonAscii = 0x80;  // UTF-8 bytes of names written in other alphabets
constexpr std::string_view kBlockCommentBegin = "=begin";
constexpr std::string_view kBlockCommentEnd = "=end";
constexpr std::string_view kDataSection = "__END__";  // the rest of the file is data, not code
constexpr std::string_view kInterpolationOpen = "#{";
constexpr std::string_view kScope = "::";
constexpr std::string_view kHeredocStart = "<<";
constexpr std::string_view kHeredocIndents = "~-";  // <<~ and <<-: the closing word may be indented
constexpr std::string_view kQuotes = "\"'`";
constexpr std::string_view kOpeners = "([{<";  // %w(...) and the like close with the matching one
constexpr std::string_view kClosers = ")]}>";
constexpr std::string_view kMethodSuffixes = "?!";  // empty?, save!
constexpr std::string_view kBlanks = " \t\r";
constexpr std::string_view kExponentSigns = "+-";
constexpr std::string_view kExponents = "eE";
constexpr std::string_view kRadixPrefixes = "xXbBoO";  // 0x1F, 0b101, 0o17: no exponent
constexpr std::string_view kClosingBrackets = ")]}";
// Lengths, in bytes, of openings the scanner steps over.
constexpr std::size_t kTypedPercentOpener = 2;       // %w[: the bracket comes after the letter
constexpr std::size_t kCharacterLiteral = 2;         // ?a
constexpr std::size_t kEscapedCharacterLiteral = 3;  // ?\n
constexpr std::size_t kQuotedSymbolOpening = 2;      // :"
constexpr std::size_t kClassVariableSigils = 2;      // @@

constexpr std::string_view kKeywords[] = {
    "BEGIN",  "END",   "alias", "and",    "begin", "break",    "case",     "class",     "def",    "defined?", "do",
    "else",   "elsif", "end",   "ensure", "false", "for",      "if",       "in",        "module", "next",     "nil",
    "not",    "or",    "redo",  "rescue", "retry", "return",   "self",     "super",     "then",   "true",     "undef",
    "unless", "until", "when",  "while",  "yield", "__FILE__", "__LINE__", "__method__"};
// The keywords that are values: what follows them is an operator ("self / 2"), not an operand.
constexpr std::string_view kValueKeywords[] = {"end",  "false",    "nil",      "self",
                                               "true", "__FILE__", "__LINE__", "__method__"};

// %-literals: the letter after "%", what it makes and whether "#{}" works inside.
struct PercentLiteral {
  char letter;
  Kind kind;
  bool interpolates;
};
constexpr PercentLiteral kPercentLiterals[] = {
    {'q', Kind::String, false}, {'Q', Kind::String, true},  {'w', Kind::String, false},
    {'W', Kind::String, true},  {'i', Kind::Symbol, false}, {'I', Kind::Symbol, true},
    {'r', Kind::Regex, true},   {'s', Kind::Symbol, false}, {'x', Kind::String, true}};

bool is_in(std::string_view set, char c) { return c != '\0' && set.find(c) != std::string_view::npos; }
bool is_digit(char c) { return c >= '0' && c <= '9'; }
bool is_upper(char c) { return c >= 'A' && c <= 'Z'; }
bool is_lower(char c) { return c >= 'a' && c <= 'z'; }
bool is_non_ascii(char c) { return static_cast<unsigned char>(c) >= kFirstNonAscii; }
bool is_name_start(char c) { return is_lower(c) || is_upper(c) || c == '_' || is_non_ascii(c); }
bool is_name_char(char c) { return is_name_start(c) || is_digit(c); }
bool is_symbol_name_char(char c) { return is_name_char(c) || c == '@' || c == '$'; }
bool starts_symbol_name(char c) { return is_symbol_name_char(c) && !is_digit(c); }
// Anything but a space, a control character or the end of the code.
bool is_visible(char c) { return static_cast<unsigned char>(c) > ' '; }
// What can open a %-literal: punctuation (not a letter, a digit, a space or the end).
bool is_delimiter(char c) { return is_visible(c) && !is_name_char(c); }

template <typename Words>
bool is_one_of(const Words& words, std::string_view word) {
  return std::find(std::begin(words), std::end(words), word) != std::end(words);
}

char closer_of(char opener) {
  const std::size_t at = kOpeners.find(opener);
  return at == std::string_view::npos ? opener : kClosers[at];
}

constexpr PercentLiteral kBarePercent{'\0', Kind::String, true};  // %(...): a string, like "..."

// The %-literal of a letter (%w...), when what follows it can open one.
const PercentLiteral* percent_literal(char letter, char opener) {
  if (!is_delimiter(opener)) return nullptr;
  for (const PercentLiteral& literal : kPercentLiterals) {
    if (literal.letter == letter) return &literal;
  }
  return nullptr;
}

// What closes a string-like literal, and what it is.
struct Literal {
  char open;   // nests inside it, as in %w(a (b) c); the same as `close` for quotes
  char close;  // '\n' for a line of a heredoc
  bool interpolates;
  Kind kind;
};

// A heredoc begun on the current line: its body starts on the next one.
struct Heredoc {
  std::string terminator;
  bool indented_end;
  bool interpolates;
};

// A heredoc's opening (<<~NAME, <<-'NAME', <<NAME), and the offset just past it.
struct HeredocOpening {
  Heredoc heredoc;
  std::size_t end;
};

// A %-literal's opening: its type, and how far past the "%" its opening delimiter is.
struct PercentOpening {
  PercentLiteral type;
  std::size_t opener_ahead;
};

bool closes_quote(char quote, char c) { return quote == '\0' || c == quote; }

class Scanner {
 public:
  explicit Scanner(std::string_view code) : code_(code) {}

  std::vector<RubyToken> run() {
    while (!at_end()) scan_one();
    return std::move(tokens_);
  }

 private:
  using Rule = bool (Scanner::*)();

  bool at_end() const { return pos_ >= code_.size(); }
  char peek(std::size_t ahead = 0) const { return pos_ + ahead < code_.size() ? code_[pos_ + ahead] : '\0'; }
  bool starts_with(std::string_view text) const { return code_.substr(pos_, text.size()) == text; }
  bool starts_with_word(std::string_view word) const { return starts_with(word) && !is_name_char(peek(word.size())); }
  bool at_line_start() const { return pos_ == 0 || code_[pos_ - 1] == '\n'; }
  void skip_line() {
    while (!at_end() && peek() != '\n') ++pos_;
  }
  template <typename Test>
  void skip_while(Test test) {
    while (!at_end() && test(peek())) ++pos_;
  }

  void scan_one() {
    // Tried in order on each piece of code; the first that recognizes it consumes it.
    static constexpr Rule kRules[] = {
        &Scanner::scan_newline,         &Scanner::scan_block_comment, &Scanner::scan_data_section,
        &Scanner::scan_comment,         &Scanner::scan_blank,         &Scanner::scan_quoted,
        &Scanner::scan_heredoc_opening, &Scanner::scan_percent,       &Scanner::scan_regex,
        &Scanner::scan_character,       &Scanner::scan_scope,         &Scanner::scan_symbol,
        &Scanner::scan_variable,        &Scanner::scan_number,        &Scanner::scan_word,
        &Scanner::scan_punctuation};
    for (const Rule rule : kRules) {
      if ((this->*rule)()) return;
    }
    ++pos_;  // (scan_punctuation takes anything: this only keeps the loop certain to end)
  }

  bool continues_last(std::size_t begin, Kind kind) const {
    return !tokens_.empty() && tokens_.back().end == begin && tokens_.back().kind == kind;
  }
  // The code from `begin` to here is a `kind`: one token with the one before when they touch.
  void add(std::size_t begin, Kind kind) {
    if (pos_ <= begin) return;
    if (continues_last(begin, kind)) {
      tokens_.back().end = pos_;
      return;
    }
    tokens_.push_back({begin, pos_, kind});
  }
  // `c` is here, where a value may go.
  bool opens_value(char c) const { return peek() == c && operand_next_; }
  // A value was read: what follows is an operator or the end of the expression.
  void ended_operand() {
    operand_next_ = false;
    after_dot_ = false;
  }

  bool scan_newline() {
    if (peek() != '\n') return false;
    ++pos_;
    operand_next_ = true;
    after_dot_ = false;
    scan_heredoc_bodies();
    return true;
  }

  bool opens_block_comment() const { return at_line_start() && starts_with_word(kBlockCommentBegin); }
  // =begin at the start of a line, through the line that starts with =end.
  bool scan_block_comment() {
    if (!opens_block_comment()) return false;
    const std::size_t begin = pos_;
    skip_line();
    while (!at_end()) {
      ++pos_;  // the newline
      if (starts_with_word(kBlockCommentEnd)) break;
      skip_line();
    }
    skip_line();
    add(begin, Kind::Comment);
    return true;
  }

  bool opens_data_section() const { return at_line_start() && starts_with_word(kDataSection); }
  bool scan_data_section() {
    if (!opens_data_section()) return false;
    const std::size_t begin = pos_;
    pos_ = code_.size();
    add(begin, Kind::Comment);
    return true;
  }

  bool scan_comment() {
    if (peek() != '#') return false;
    const std::size_t begin = pos_;
    skip_line();
    add(begin, Kind::Comment);
    return true;
  }

  bool scan_blank() {
    if (!is_in(kBlanks, peek())) return false;
    ++pos_;
    return true;
  }

  // "...", `...` and '...' (only the last without interpolation).
  bool scan_quoted() {
    const char quote = peek();
    if (!is_in(kQuotes, quote)) return false;
    const std::size_t begin = pos_++;
    scan_body({quote, quote, quote != '\'', Kind::String}, begin);
    ended_operand();
    return true;
  }

  bool opens_interpolation(const Literal& literal) const {
    return literal.interpolates && starts_with(kInterpolationOpen) && nesting_ < kMaxNesting;
  }
  bool closes(const Literal& literal, char c, int depth) const { return c == literal.close && depth == 0; }
  // One step through a literal's inside: true once its closing character was taken.
  bool step_body(const Literal& literal, int& depth) {
    const char c = peek();
    ++pos_;
    if (c == '\\') {
      pos_ = std::min(pos_ + 1, code_.size());  // the escaped character, whatever it is
      return false;
    }
    if (closes(literal, c, depth)) return true;
    if (c == literal.close) --depth;
    if (c == literal.open) ++depth;  // (only when open and close differ: a quote closed above)
    return false;
  }
  // A literal's inside from here through its closing character, or to the end of the code when
  // it never closes. Its code starts at `begin` (its opening).
  void scan_body(const Literal& literal, std::size_t begin) {
    int depth = 0;
    while (!at_end()) {
      if (opens_interpolation(literal)) {
        add(begin, literal.kind);
        scan_interpolation();
        begin = pos_;
      } else if (step_body(literal, depth)) {
        break;
      }
    }
    add(begin, literal.kind);
  }

  // "#{ code }": the delimiters, and the code between them colored as code.
  void scan_interpolation() {
    const std::size_t open = pos_;
    pos_ += kInterpolationOpen.size();
    add(open, Kind::Interpolation);
    ++nesting_;
    operand_next_ = true;
    after_dot_ = false;
    scan_code_to_closing_brace();
    --nesting_;
    if (peek() != '}') return;
    const std::size_t close = pos_++;
    add(close, Kind::Interpolation);
  }
  bool closes_interpolation(int depth) const { return peek() == '}' && depth == 0; }
  // The code of an interpolation, up to the "}" that closes it (a block's or a hash's braces nest).
  void scan_code_to_closing_brace() {
    int depth = 0;
    while (!at_end() && !closes_interpolation(depth)) {
      if (peek() == '{') ++depth;
      if (peek() == '}') --depth;
      scan_one();
    }
  }

  std::optional<HeredocOpening> heredoc_opening() const {
    const bool indented = is_in(kHeredocIndents, peek(kHeredocStart.size()));
    const std::size_t quote_at = kHeredocStart.size() + (indented ? 1 : 0);
    const char quote = is_in(kQuotes, peek(quote_at)) ? peek(quote_at) : '\0';
    const std::size_t name_at = quote_at + (quote == '\0' ? 0 : 1);
    std::size_t name_end = name_at;
    while (is_name_char(peek(name_end))) ++name_end;
    if (!is_name_start(peek(name_at)) || !closes_quote(quote, peek(name_end))) return std::nullopt;
    const std::string name(code_.substr(pos_ + name_at, name_end - name_at));
    const std::size_t end = pos_ + name_end + (quote == '\0' ? 0 : 1);
    return HeredocOpening{{name, indented, quote != '\''}, end};
  }
  // "x << y" is a shift or an append; "<<~NAME" anywhere, and "<<NAME" where a value goes, open
  // heredocs.
  bool is_heredoc(const std::optional<HeredocOpening>& opening) const {
    return opening && (opening->heredoc.indented_end || operand_next_);
  }
  bool scan_heredoc_opening() {
    if (!starts_with(kHeredocStart)) return false;
    const std::optional<HeredocOpening> opening = heredoc_opening();
    if (!is_heredoc(opening)) return false;
    const std::size_t begin = pos_;
    pos_ = opening->end;
    add(begin, Kind::String);
    heredocs_.push_back(opening->heredoc);
    ended_operand();
    return true;
  }

  bool ends_heredoc(const Heredoc& heredoc) const {
    const std::size_t line_end = std::min(code_.find('\n', pos_), code_.size());
    std::string_view line = code_.substr(pos_, line_end - pos_);
    if (heredoc.indented_end) line.remove_prefix(std::min(line.find_first_not_of(kBlanks), line.size()));
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    return line == heredoc.terminator;
  }
  // The heredocs opened on the line just ended: their lines, then each one's closing word.
  void scan_heredoc_bodies() {
    std::vector<Heredoc> opened;
    opened.swap(heredocs_);
    for (const Heredoc& heredoc : opened) {
      while (!at_end() && !ends_heredoc(heredoc)) scan_body({'\n', '\n', heredoc.interpolates, Kind::String}, pos_);
      const std::size_t terminator = pos_;
      skip_line();
      add(terminator, Kind::String);
    }
  }

  std::optional<PercentOpening> percent_opening() const {
    if (!opens_value('%')) return std::nullopt;
    if (is_delimiter(peek(1))) return PercentOpening{kBarePercent, 1};
    const PercentLiteral* type = percent_literal(peek(1), peek(kTypedPercentOpener));
    if (type == nullptr) return std::nullopt;
    return PercentOpening{*type, kTypedPercentOpener};
  }
  // %w[a b], %i(x y), %q{...}, %(...), %r{...}: where a value goes ("a % b" is a remainder).
  bool scan_percent() {
    const std::optional<PercentOpening> opening = percent_opening();
    if (!opening) return false;
    const PercentLiteral& type = opening->type;
    const char opener = peek(opening->opener_ahead);
    const std::size_t begin = pos_;
    pos_ += opening->opener_ahead + 1;
    scan_body({opener, closer_of(opener), type.interpolates, type.kind}, begin);
    if (type.kind == Kind::Regex) scan_regex_flags();
    ended_operand();
    return true;
  }

  // The letters after a regex's closing slash (/x/i), part of it.
  void scan_regex_flags() {
    const std::size_t begin = pos_;
    skip_while(is_lower);
    add(begin, Kind::Regex);
  }
  // /.../ where a value goes ("a / b" is a division).
  bool scan_regex() {
    if (!opens_value('/')) return false;
    const std::size_t begin = pos_++;
    scan_body({'/', '/', true, Kind::Regex}, begin);
    scan_regex_flags();
    ended_operand();
    return true;
  }

  // ?a and ?\n are one-character strings where a value goes; "a ? b : c" is a ternary.
  std::size_t character_literal_length() const {
    return peek(1) == '\\' ? kEscapedCharacterLiteral : kCharacterLiteral;
  }
  bool is_character_literal() const {
    return opens_value('?') && is_visible(peek(1)) && !is_name_char(peek(character_literal_length()));
  }
  bool scan_character() {
    if (!is_character_literal()) return false;
    const std::size_t begin = pos_;
    pos_ = std::min(pos_ + character_literal_length(), code_.size());
    add(begin, Kind::String);
    ended_operand();
    return true;
  }

  // A::B: B is a name inside A.
  bool scan_scope() {
    if (!starts_with(kScope)) return false;
    pos_ += kScope.size();
    operand_next_ = true;
    after_dot_ = true;
    return true;
  }

  // :name, :name?, :@name and :"name". A ":" alone, as in "a ? b : c", is left to scan_punctuation.
  bool scan_symbol() {
    if (peek() != ':') return false;
    if (is_in(kQuotes, peek(1))) return scan_quoted_symbol();
    if (!starts_symbol_name(peek(1))) return false;
    const std::size_t begin = pos_++;
    skip_while(is_symbol_name_char);
    take_method_suffix();
    add(begin, Kind::Symbol);
    ended_operand();
    return true;
  }
  bool scan_quoted_symbol() {
    const std::size_t begin = pos_;
    pos_ += kQuotedSymbolOpening;
    const char quote = code_[begin + 1];
    scan_body({quote, quote, quote != '\'', Kind::Symbol}, begin);
    ended_operand();
    return true;
  }

  bool starts_instance_variable() const {
    const std::size_t at_signs = peek(1) == '@' ? kClassVariableSigils : 1;
    return peek() == '@' && is_name_start(peek(at_signs));
  }
  bool starts_global_variable() const { return peek() == '$' && is_visible(peek(1)); }
  bool starts_variable() const { return starts_global_variable() || starts_instance_variable(); }
  // @name, @@name, $name and the one-character globals ($0, $!).
  bool scan_variable() {
    if (!starts_variable()) return false;
    const bool global = starts_global_variable();
    const std::size_t begin = pos_++;
    if (global && !is_name_char(peek())) ++pos_;  // $!
    if (peek() == '@') ++pos_;                    // @@name
    skip_while(is_name_char);
    add(begin, Kind::Variable);
    ended_operand();
    return true;
  }

  bool has_radix_prefix(std::size_t begin) const {
    return code_[begin] == '0' && begin + 1 < code_.size() && is_in(kRadixPrefixes, code_[begin + 1]);
  }
  // The sign of an exponent, as in 1.5e-3 (not in 0x1e-1, a subtraction).
  bool at_exponent_sign(std::size_t begin) const {
    return is_in(kExponentSigns, peek()) && is_digit(peek(1)) && is_in(kExponents, code_[pos_ - 1]) &&
           !has_radix_prefix(begin);
  }
  // 42, 1_000, 0x1F, 3.14, 1.5e-3, 2r, 3i: digits and letters, then a fraction and an exponent.
  bool scan_number() {
    if (!is_digit(peek())) return false;
    const std::size_t begin = pos_;
    skip_while(is_name_char);
    if (peek() == '.' && is_digit(peek(1))) {
      ++pos_;
      skip_while(is_name_char);
    }
    if (at_exponent_sign(begin)) {
      ++pos_;
      skip_while(is_name_char);
    }
    add(begin, Kind::Number);
    ended_operand();
    return true;
  }

  // The ? or ! that ends a method name (empty?, save!), but not "!=": x!=y compares.
  void take_method_suffix() {
    if (is_in(kMethodSuffixes, peek()) && peek(1) != '=') ++pos_;
  }
  // "name:" is a hash key or a keyword argument; "A::B" is not.
  bool at_label() const { return peek() == ':' && peek(1) != ':'; }
  void add_word(std::size_t begin) {
    const std::string_view word = code_.substr(begin, pos_ - begin);
    const bool keyword = !after_dot_ && is_one_of(kKeywords, word);
    if (keyword) {
      add(begin, Kind::Keyword);
    } else if (is_upper(word.front())) {
      add(begin, Kind::Constant);
    }
    ended_operand();
    if (keyword) operand_next_ = !is_one_of(kValueKeywords, word);
  }
  bool scan_word() {
    if (!is_name_start(peek())) return false;
    const std::size_t begin = pos_;
    skip_while(is_name_char);
    take_method_suffix();
    if (!at_label()) {
      add_word(begin);
      return true;
    }
    ++pos_;
    add(begin, Kind::Symbol);
    operand_next_ = true;
    after_dot_ = false;
    return true;
  }

  // A "." calling a method, not one of a range (1..5).
  bool is_method_dot() const { return peek() == '.' && peek(1) != '.' && (pos_ == 0 || code_[pos_ - 1] != '.'); }
  // Operators, brackets and anything else: after a closing bracket comes an operator, after the
  // others a value.
  bool scan_punctuation() {
    const bool dot = is_method_dot();
    const bool closing = is_in(kClosingBrackets, peek());
    ++pos_;
    operand_next_ = !closing;
    after_dot_ = dot;
    return true;
  }

  std::string_view code_;
  std::size_t pos_ = 0;
  std::vector<RubyToken> tokens_;
  std::vector<Heredoc> heredocs_;  // opened on the current line
  std::size_t nesting_ = 0;        // interpolations the scanner is inside
  bool operand_next_ = true;       // a value may come next: "/" opens a regex, "%" a literal, "?" a character
  bool after_dot_ = false;         // a "." was just read: what follows is a method name, never a keyword
};

std::vector<std::size_t> line_starts_of(const std::string& code) {
  std::vector<std::size_t> starts{0};
  for (std::size_t at = code.find('\n'); at != std::string::npos; at = code.find('\n', at + 1)) {
    starts.push_back(at + 1);
  }
  return starts;
}

}  // namespace

std::vector<RubyToken> ruby_tokens(std::string_view code) { return Scanner(code).run(); }

const RubyHighlight& RubyHighlightCache::of(const std::string& code) {
  if (filled_ && code == code_) return highlight_;
  code_ = code;
  highlight_.tokens = ruby_tokens(code_);
  highlight_.line_starts = line_starts_of(code_);
  filled_ = true;
  return highlight_;
}

}  // namespace redgem
