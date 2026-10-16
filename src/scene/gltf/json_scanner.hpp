#pragma once

// Reads a JSON text piece by piece: where it is in the text, and the texts, numbers and literals
// found there. What the text gets wrong is thrown as a JsonError saying where.
// Implemented in json_scanner.cpp, and in json_scanner_strings.cpp for the texts in quotes.

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace redgem {

class JsonError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class JsonScanner {
 public:
  explicit JsonScanner(std::string_view text) : text_(text) {}

  bool at_end() const { return pos_ >= text_.size(); }
  // The character the scanner is at; an error at the end of the text.
  char peek() const;
  char next();
  // Steps over `c` when the scanner is at it; says whether it was.
  bool try_consume(char c);
  void expect(char c);
  void skip_whitespace();
  // Steps over a true, false or null when the scanner is at one; says whether it was.
  bool consume_any_literal();

  // A text in quotes, its escapes decoded (\uXXXX as UTF-8).
  std::string read_string();
  // A finite number, written as JSON writes them.
  double read_number();
  bool read_bool();

  [[noreturn]] void fail(const std::string& message) const;

 private:
  bool at_whitespace() const;
  bool consume_literal(std::string_view literal);
  // What follows a backslash in a string.
  void append_escape(std::string& out);
  unsigned read_hex4();
  // The characters of the number starting here.
  std::string_view number_text() const;

  std::string_view text_;
  std::size_t pos_ = 0;
};

}  // namespace redgem
