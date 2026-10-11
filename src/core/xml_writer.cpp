#include "core/xml_writer.hpp"

namespace redgem {

namespace {

constexpr const char* kDeclaration = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
constexpr const char* kIndent = "  ";

// The characters XML gives a meaning to, and how each is written when it is only text.
struct Escape {
  char character;
  const char* written;
};
constexpr Escape kEscapes[] = {{'&', "&amp;"}, {'<', "&lt;"}, {'>', "&gt;"}, {'"', "&quot;"}};

std::string written_out(char c) {
  for (const Escape& escape : kEscapes) {
    if (escape.character == c) return escape.written;
  }
  return std::string(1, c);
}

std::string indentation(int depth) {
  std::string spaces;
  for (int level = 0; level < depth; ++level) spaces += kIndent;
  return spaces;
}

}  // namespace

std::string xml_escaped(const std::string& text) {
  std::string out;
  for (const char c : text) out += written_out(c);
  return out;
}

XmlElement& XmlElement::attribute(const std::string& name, const std::string& value) {
  attributes_.emplace_back(name, value);
  return *this;
}

XmlElement& XmlElement::child(const std::string& name) {
  children_.push_back(std::make_unique<XmlElement>(name));
  return *children_.back();
}

std::string XmlElement::text(int depth) const {
  std::string out = indentation(depth) + "<" + name_;
  for (const auto& [name, value] : attributes_) out += " " + name + "=\"" + xml_escaped(value) + "\"";
  if (children_.empty()) return out + "/>\n";
  out += ">\n";
  for (const auto& child : children_) out += child->text(depth + 1);
  return out + indentation(depth) + "</" + name_ + ">\n";
}

std::string XmlElement::document() const { return kDeclaration + text(); }

}  // namespace redgem
