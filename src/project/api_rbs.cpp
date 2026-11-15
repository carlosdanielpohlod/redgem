#include "project/api_rbs.hpp"

#include <algorithm>
#include <iterator>
#include <system_error>

#include "core/files.hpp"
#include "project/api_types.hpp"

namespace redgem::tools {
namespace {

// What the signatures start with: what they are, and the type Color stands for.
constexpr const char* kHeader =
    "# The scripting API of RedGem games as Ruby type signatures (RBS), for the language servers of\n"
    "# editors and type checkers (Steep, Solargraph and the like). Made from docs/scripting/api.json\n"
    "# by `redgem api --rbs`: change that file, not this one. Each class and method is annotated\n"
    "# %a{redgem:stable} (no breaking change within a 1.x minor release) or %a{redgem:experimental}\n"
    "# (it may change in any release): see docs/scripting/stability.md.\n"
    "\n"
    "module RedGem\n"
    "  # A color: its name (:orange), or [r, g, b] from 0 to 255.\n"
    "  type color = Symbol | Array[Numeric]\n"
    "end\n";
constexpr const char* kIndent = "  ";
constexpr const char* kComment = "# ";
constexpr const char* kEnd = "end\n";
constexpr const char* kClassStatic = "self.";
constexpr const char* kSeparator = ", ";
constexpr const char* kOptionalMark = "?";
constexpr const char* kRestMark = "*";
constexpr const char* kKeywordRestMark = "**";
// A class's `new` is its `initialize` in RBS, which returns nothing to use.
constexpr const char* kNew = "new";
constexpr const char* kInitialize = "initialize";
constexpr const char* kVoid = "void";
// The annotation that says whether games may rely on a class or a method: %a{redgem:stable}.
constexpr const char* kAnnotationOpen = "%a{redgem:";
constexpr const char* kAnnotationClose = "}\n";
// The words RBS keeps for itself, which cannot name a parameter (it is then left unnamed).
constexpr const char* kRbsKeywords[] = {"alias",    "attr_accessor", "attr_reader", "attr_writer", "bool",  "bot",
                                        "class",    "def",           "end",         "extend",      "false", "include",
                                        "instance", "interface",     "module",      "nil",         "out",   "prepend",
                                        "private",  "public",        "self",        "singleton",   "super", "top",
                                        "true",     "type",          "unchecked",   "untyped",     "void",  "use"};

bool is_constructor(const ApiMethod& method) { return method.on_class && method.name == kNew; }

bool is_rbs_keyword(const std::string& name) {
  return std::find(std::begin(kRbsKeywords), std::end(kRbsKeywords), name) != std::end(kRbsKeywords);
}

// " name", or nothing for a name RBS cannot take.
std::string named(const std::string& name) { return is_rbs_keyword(name) ? std::string() : " " + name; }

std::string optional_mark(const ApiParam& param) { return param.optional ? kOptionalMark : ""; }

// "Symbol name", "?Integer pad", "ease: Symbol", "?loop: bool", "*String bindings".
std::string parameter(const ApiParam& param) {
  const std::string type = rbs_type(param.type);
  if (param.rest) return (param.keyword ? kKeywordRestMark : kRestMark) + type + named(param.name);
  if (param.keyword) return optional_mark(param) + param.name + ": " + type;
  return optional_mark(param) + type + named(param.name);
}

std::string parameters(const ApiMethod& method) {
  std::string text;
  for (const ApiParam& param : method.params) {
    if (!param.block) text += (text.empty() ? "" : kSeparator) + parameter(param);
  }
  return text;
}

// " { (Float) -> void }", or " ?{ ... }" for a block it may go without; nothing without one.
std::string block(const ApiMethod& method) {
  const auto found =
      std::find_if(method.params.begin(), method.params.end(), [](const ApiParam& param) { return param.block; });
  if (found == method.params.end()) return {};
  return " " + optional_mark(*found) + rbs_block(found->type);
}

// "(Symbol name, ?pad: Integer) -> Float".
std::string method_type(const ApiMethod& method) {
  const std::string returned = is_constructor(method) ? std::string(kVoid) : rbs_returned(method.returns);
  return "(" + parameters(method) + ")" + block(method) + " -> " + returned;
}

std::string method_name(const ApiMethod& method) {
  if (is_constructor(method)) return kInitialize;
  return (method.on_class ? kClassStatic : "") + method.name;
}

// "%a{redgem:stable}\n", after a declaration's comment (which RBS still gives the declaration);
// nothing for one with no mark.
std::string annotation(ApiStability stability) {
  if (stability == ApiStability::Unmarked) return {};
  return std::string(kAnnotationOpen) + stability_name(stability) + kAnnotationClose;
}

std::string method_rbs(const ApiMethod& method) {
  const std::string marked = annotation(method.stability);
  return std::string(kIndent) + kComment + method.about + "\n" + (marked.empty() ? "" : kIndent + marked) + kIndent +
         "def " + method_name(method) + ": " + method_type(method) + "\n";
}

// "class GameMenu < Behaviour", "module Input".
std::string declaration(const ApiClass& owner) {
  if (owner.is_module) return "module " + owner.name + "\n";
  return "class " + owner.name + (owner.superclass.empty() ? "" : " < " + owner.superclass) + "\n";
}

std::string class_rbs(const ApiClass& owner) {
  std::string text =
      "\n" + std::string(kComment) + owner.about + "\n" + annotation(owner.stability) + declaration(owner);
  for (const ApiMethod& method : owner.methods) text += method_rbs(method);
  return text + kEnd;
}

}  // namespace

std::string api_rbs(const ApiReference& reference) {
  std::string text = kHeader;
  for (const ApiClass& owner : reference.classes) text += class_rbs(owner);
  return text;
}

bool write_type_signatures(const std::filesystem::path& game_dir, std::string& error) {
  std::error_code fs_error;
  std::filesystem::create_directories(game_dir / kSignaturesFolder, fs_error);
  if (fs_error) {
    error = "cannot make " + (game_dir / kSignaturesFolder).string() + ": " + fs_error.message();
    return false;
  }
  return write_text_file(game_dir / kSignaturesFile, api_rbs(api_reference()), error);
}

}  // namespace redgem::tools
