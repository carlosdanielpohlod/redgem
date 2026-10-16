#include "scene/gltf/extras_parser.hpp"

#include <cctype>

#include "scene/gltf/json_scanner.hpp"

namespace redgem {
namespace {

// How deep the arrays and objects of a value that is skipped may be nested: each level takes stack.
constexpr int kMaxNesting = 64;

bool starts_a_bool(char c) { return c == 't' || c == 'f'; }
bool starts_a_number(char c) { return c == '-' || std::isdigit(static_cast<unsigned char>(c)) != 0; }
bool opens_a_container(char c) { return c == '[' || c == '{'; }

// Reads the one JSON object of a node's extras: enough for Blender custom properties.
class ExtrasReader {
 public:
  explicit ExtrasReader(std::string_view text) : json_(text) {}

  void read_object(ExtrasParseResult& result) {
    json_.skip_whitespace();
    json_.expect('{');
    json_.skip_whitespace();
    if (!json_.try_consume('}')) read_members(result);
    json_.skip_whitespace();
    if (!json_.at_end()) json_.fail("trailing characters after object");
  }

 private:
  // The members of an object whose '{' was read, up to and including its '}'.
  void read_members(ExtrasParseResult& result) {
    do {
      const std::string key = read_key();
      read_member(key, result);
      json_.skip_whitespace();
    } while (json_.try_consume(','));
    json_.expect('}');
  }

  // A member's name and the ':' after it.
  std::string read_key() {
    json_.skip_whitespace();
    std::string key = json_.read_string();
    json_.skip_whitespace();
    json_.expect(':');
    json_.skip_whitespace();
    return key;
  }

  void read_member(const std::string& key, ExtrasParseResult& result) {
    const char c = json_.peek();
    if (c == '"') {
      result.properties[key] = json_.read_string();
    } else if (starts_a_bool(c)) {
      result.properties[key] = json_.read_bool();
    } else if (starts_a_number(c)) {
      result.properties[key] = json_.read_number();
    } else {
      skip_value();
      result.skipped_keys.push_back(key);
    }
  }

  // Skips arrays, objects and null without interpreting them.
  void skip_value() {
    const char c = json_.peek();
    if (c == '"') {
      json_.read_string();
    } else if (opens_a_container(c)) {
      skip_container(c);
    } else if (!json_.consume_any_literal()) {
      json_.read_number();
    }
  }

  // An array or an object, from its opening bracket to its closing one.
  void skip_container(char open) {
    const char close = open == '[' ? ']' : '}';
    if (++nesting_ > kMaxNesting) json_.fail("nested too deeply");
    json_.next();
    json_.skip_whitespace();
    if (!json_.try_consume(close)) skip_elements(open, close);
    --nesting_;
  }

  // What a container whose opening bracket was read holds, up to and including its closing one.
  void skip_elements(char open, char close) {
    do {
      json_.skip_whitespace();
      if (open == '{') read_key();
      skip_value();
      json_.skip_whitespace();
    } while (json_.try_consume(','));
    json_.expect(close);
  }

  JsonScanner json_;
  int nesting_ = 0;  // how many arrays and objects being skipped are open
};

bool is_blank(std::string_view text) {
  for (const char c : text) {
    if (!std::isspace(static_cast<unsigned char>(c))) return false;
  }
  return true;
}

}  // namespace

ExtrasParseResult parse_extras(std::string_view json) {
  ExtrasParseResult result;
  if (is_blank(json)) return result;
  try {
    ExtrasReader(json).read_object(result);
  } catch (const JsonError& error) {
    result = ExtrasParseResult{};
    result.ok = false;
    result.error = error.what();
  }
  return result;
}

}  // namespace redgem
