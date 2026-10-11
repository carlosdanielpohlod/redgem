#include "core/xml_writer.hpp"

namespace redgem {

namespace {

constexpr const char* kDeclaration = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
constexpr const char* kIndent = "  ";

std::string indentation(int depth) {
  std::string spaces;
  for (int level = 0; level < depth; ++level) spaces += kIndent;
  return spaces;
}

}  // namespace

std::string xml_escaped(const std::string& text) {
  std::string out;
  for (const char c : text) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += c;
    }
  }
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
