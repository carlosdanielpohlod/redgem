#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace redgem {

// An XML element being put together: its attributes and the elements inside it. Values are
// escaped when written, so they can hold anything.
//   XmlElement root("manifest");
//   root.attribute("package", "com.mystudio.game");
//   root.child("application").attribute("android:label", title);
//   root.document();
class XmlElement {
 public:
  explicit XmlElement(std::string name) : name_(std::move(name)) {}

  // Sets an attribute and returns this element, so several can be chained.
  XmlElement& attribute(const std::string& name, const std::string& value);
  // Adds an element inside this one and returns it (it stays valid while this one lives).
  XmlElement& child(const std::string& name);

  // This element and what is inside it, indented by `depth` levels.
  std::string text(int depth = 0) const;
  // The same as a whole file: the XML declaration, then the element.
  std::string document() const;

 private:
  std::string name_;
  std::vector<std::pair<std::string, std::string>> attributes_;
  std::vector<std::unique_ptr<XmlElement>> children_;
};

// A text with the characters XML gives a meaning to (& < > ") written out.
std::string xml_escaped(const std::string& text);

}  // namespace redgem
