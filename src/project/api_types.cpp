#include "project/api_types.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iterator>

namespace redgem::tools {
namespace {

// The names every type may use, besides the API's classes.
constexpr const char* kBuiltInTypes[] = {"Integer", "Float",   "Numeric", "String", "Symbol", "bool",
                                         "nil",     "untyped", "self",    "Proc",   "Class",  "Color"};
constexpr const char* kVoid = "void";
// The types that take others, and how many.
struct Generic {
  const char* name;
  std::size_t arity;
};
constexpr Generic kGenerics[] = {{"Array", 1}, {"Range", 1}, {"Hash", 2}};
// The language's marks.
constexpr const char* kArrow = "->";
constexpr const char* kBlockMark = "^";
constexpr const char* kUnion = "|";
constexpr const char* kComma = ",";
constexpr const char* kOpenList = "[";
constexpr const char* kCloseList = "]";
constexpr const char* kOpenCall = "(";
constexpr const char* kCloseCall = ")";
constexpr char kSpace = ' ';
// What Color is in RBS (api_rbs.cpp declares it).
constexpr const char* kColor = "Color";
constexpr const char* kRbsColor = "RedGem::color";
constexpr const char* kRbsBlockOpen = "{ ";
constexpr const char* kRbsBlockClose = " }";

bool is_name_start(char letter) { return std::isalpha(static_cast<unsigned char>(letter)) != 0 || letter == '_'; }

bool is_name_letter(char letter) {
  return is_name_start(letter) || std::isdigit(static_cast<unsigned char>(letter)) != 0 || letter == ':';
}

// How long the token at `at` is: a name (Animation::Graph), "->", or one letter.
std::size_t token_length(const std::string& text, std::size_t at) {
  if (!is_name_start(text[at])) return text.compare(at, std::strlen(kArrow), kArrow) == 0 ? std::strlen(kArrow) : 1;
  std::size_t end = at;
  while (end < text.size() && is_name_letter(text[end])) ++end;
  return end - at;
}

std::vector<std::string> tokens_of(const std::string& text) {
  std::vector<std::string> tokens;
  for (std::size_t at = 0; at < text.size();) {
    const std::size_t length = token_length(text, at);
    if (text[at] != kSpace) tokens.push_back(text.substr(at, length));
    at += length;
  }
  return tokens;
}

const Generic* generic_named(const std::string& name) {
  const auto* found = std::find_if(std::begin(kGenerics), std::end(kGenerics),
                                   [&name](const Generic& generic) { return name == generic.name; });
  return found == std::end(kGenerics) ? nullptr : found;
}

// Reads a type token by token, and keeps the first thing wrong with it.
class TypeReader {
 public:
  TypeReader(const std::string& text, const std::vector<std::string>& classes)
      : tokens_(tokens_of(text)), classes_(classes) {}

  std::string problem(TypePlace place) {
    if (place == TypePlace::Block) {
      read_block();
    } else {
      read_union(place == TypePlace::Returned);
    }
    if (at_ < tokens_.size()) fail("unexpected " + tokens_[at_]);
    return problem_;
  }

 private:
  std::vector<std::string> tokens_;
  const std::vector<std::string>& classes_;
  std::size_t at_ = 0;
  std::string problem_;

  const std::string& next() const {
    static const std::string kEnd;
    return at_ < tokens_.size() ? tokens_[at_] : kEnd;
  }
  bool take(const char* token) {
    const bool there = next() == token;
    if (there) ++at_;
    return there;
  }
  void expect(const char* token) {
    if (!take(token)) fail(std::string("expected ") + token);
  }
  void fail(const std::string& why) {
    if (problem_.empty()) problem_ = why;
    at_ = tokens_.size();  // (nothing after the first problem says more)
  }

  void read_union(bool returned) {
    read_single(returned);
    while (take(kUnion)) read_single(false);
  }

  void read_single(bool returned) {
    if (next() == kBlockMark) return fail("a block's type is only a block parameter's");
    if (take(kOpenList)) {
      read_list(kCloseList);  // a tuple
      return;
    }
    read_named(returned);
  }

  // ^(A, B) -> R
  void read_block() {
    expect(kBlockMark);
    expect(kOpenCall);
    if (!take(kCloseCall)) read_list(kCloseCall);
    expect(kArrow);
    read_single(true);
  }

  // Types up to `close`, separated by commas: how many.
  std::size_t read_list(const char* close) {
    std::size_t count = 0;
    do {
      read_union(false);
      ++count;
    } while (take(kComma));
    expect(close);
    return count;
  }

  void read_named(bool returned) {
    const std::string name = next();
    ++at_;
    if (name == kVoid) {
      if (!returned) fail("void is only returned");
      return;
    }
    if (const Generic* generic = generic_named(name)) return read_arguments(*generic);
    if (!is_known(name)) fail("unknown type " + (name.empty() ? std::string("(none)") : name));
  }

  void read_arguments(const Generic& generic) {
    expect(kOpenList);
    if (read_list(kCloseList) != generic.arity)
      fail(std::string(generic.name) + " takes " + std::to_string(generic.arity));
  }

  bool is_known(const std::string& name) const {
    const bool built_in =
        std::find(std::begin(kBuiltInTypes), std::end(kBuiltInTypes), name) != std::end(kBuiltInTypes);
    return built_in || std::find(classes_.begin(), classes_.end(), name) != classes_.end();
  }
};

bool opens(const std::string& token) { return token == kOpenList || token == kOpenCall; }
bool closes(const std::string& token) { return token == kCloseList || token == kCloseCall; }
bool parts_top(const std::string& token, int depth) { return depth == 0 && token == kUnion; }

// Whether a type is a union at its top (A | B), not only inside (Array[A | B]).
bool is_union(const std::string& type) {
  int depth = 0;
  for (const std::string& token : tokens_of(type)) {
    if (opens(token)) ++depth;
    if (closes(token)) --depth;
    if (parts_top(token, depth)) return true;
  }
  return false;
}

}  // namespace

std::string api_type_problem(const std::string& type, const std::vector<std::string>& classes, TypePlace place) {
  return TypeReader(type, classes).problem(place);
}

std::string rbs_type(const std::string& type) {
  std::string rbs;
  for (std::size_t at = 0; at < type.size();) {
    const std::size_t length = token_length(type, at);
    const std::string token = type.substr(at, length);
    rbs += token == kColor ? kRbsColor : token;
    at += length;
  }
  return rbs;
}

std::string rbs_returned(const std::string& type) {
  return is_union(type) ? kOpenCall + rbs_type(type) + kCloseCall : rbs_type(type);
}

std::string block_signature(const std::string& type) {
  const std::size_t mark = type.find(kBlockMark);
  if (mark == std::string::npos) return type;
  const std::size_t start = type.find_first_not_of(kSpace, mark + 1);
  return start == std::string::npos ? std::string() : type.substr(start);
}

std::string rbs_block(const std::string& type) {
  return kRbsBlockOpen + rbs_type(block_signature(type)) + kRbsBlockClose;
}

}  // namespace redgem::tools
