#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace redgem {

// What a stretch of Ruby code is, for coloring it in the script tabs. Code that is none of these
// (method and local variable names, operators, spaces) gets no token and keeps the text color.
enum class RubyTokenKind {
  Keyword,        // def, end, if, self, nil...
  String,         // "...", '...', %w[...], heredocs, ?a
  Interpolation,  // the #{ and } around code inside a string
  Symbol,         // :name, :"name", name: (a hash key or keyword argument)
  Regex,          // /.../, %r{...}
  Comment,        // # ..., =begin ... =end, what follows __END__
  Number,         // 42, 1_000, 0x1F, 1.5e-3
  Constant,       // Behaviour, DEGREES_PER_SECOND
  Variable,       // @name, @@name, $name
};

// A colored stretch: the bytes [begin, end) of the code.
struct RubyToken {
  std::size_t begin = 0;
  std::size_t end = 0;
  RubyTokenKind kind = RubyTokenKind::String;
};

// The colored stretches of Ruby code, in order and apart. Never fails: code Ruby would not accept
// (an unclosed string, a stray quote) still gets tokens, the way it shows while being typed.
std::vector<RubyToken> ruby_tokens(std::string_view code);

// Code's tokens and where its lines begin.
struct RubyHighlight {
  std::vector<RubyToken> tokens;
  std::vector<std::size_t> line_starts;  // the offset of each line's first byte; the first is 0
};

// The highlight of a script's code, worked out again only when the code changed: the script tabs
// ask for it every frame.
class RubyHighlightCache {
 public:
  const RubyHighlight& of(const std::string& code);

 private:
  std::string code_;
  RubyHighlight highlight_;
  bool filled_ = false;
};

}  // namespace redgem
