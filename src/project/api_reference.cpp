#include "project/api_reference.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iterator>

#include <nlohmann/json.hpp>

#include "core/translations.hpp"
#include "project/api_json_keys.hpp"
#include "project/api_types.hpp"
#include "project/closest_words.hpp"
#include "generated/assistant_files.hpp"

namespace redgem::tools {
namespace {

using namespace api_keys;

constexpr const char* kClassKey = "class";  // what a found method says it is of
// How texts lay a method out.
constexpr std::size_t kSignatureColumn = 34;  // where a method's line starts after its signature
constexpr const char* kIndent = "  ";
constexpr const char* kClassCall = ".";
constexpr const char* kInstanceCall = "#";
constexpr const char* kGuidesFolder = "docs/scripting/";
constexpr const char* kElement = "[]";  // reads an element; "[]=" writes one
constexpr const char* kSeparator = ", ";
constexpr const char* kReturnsMark = " -> ";
constexpr const char* kRestMark = "*";
constexpr const char* kKeywordRestMark = "**";
constexpr const char* kBlockArgumentMark = "&";
constexpr const char* kOptionalBlockMark = "?";
constexpr const char* kBlockOpen = "{ ";
constexpr const char* kBlockClose = " }";
constexpr std::size_t kElementWriteArguments = 2;  // "[]=": the key and the value

bool holds(const std::string& text, const std::string& word) { return lowered(text).find(word) != std::string::npos; }

bool matches(const ApiClass& owner, const ApiMethod& method, const std::string& word) {
  return holds(typed_signature(owner, method), word) || holds(method.about, word);
}

// "  name(args)                      about".
std::string method_line(const std::string& call, const std::string& about) {
  std::string line = kIndent + call;
  line += line.size() < kSignatureColumn ? std::string(kSignatureColumn - line.size(), ' ') : std::string(" ");
  return line + about + "\n";
}

bool is_setter(const std::string& name) {
  return name.size() > 1 && name.back() == '=' && std::isalpha(static_cast<unsigned char>(name.front())) != 0;
}

bool is_operator(const std::string& name) {
  return std::isalpha(static_cast<unsigned char>(name.front())) == 0 && name.front() != '_';
}

std::string rest_mark(const ApiParam& param) { return param.keyword ? kKeywordRestMark : kRestMark; }

std::string with_default(const std::string& text, const char* mark, const ApiParam& param) {
  return param.default_value.empty() ? text : text + mark + param.default_value;
}

// An argument as a call writes it: "name", "loop: true", "*bindings", "&block".
std::string plain_argument(const ApiParam& param) {
  if (param.block) return kBlockArgumentMark + param.name;
  if (param.rest) return rest_mark(param) + param.name;
  if (param.keyword) return with_default(param.name + ":", " ", param);
  return with_default(param.name, " = ", param);
}

// ... with its type: "Symbol name", "loop: bool = true", "*String bindings".
std::string typed_argument(const ApiParam& param) {
  if (param.rest) return rest_mark(param) + param.type + " " + param.name;
  if (param.keyword) return with_default(param.name + ": " + param.type, " = ", param);
  return with_default(param.type + " " + param.name, " = ", param);
}

// How calls are written: with types (and what they return), or not.
struct CallStyle {
  std::string (*argument)(const ApiParam&);
  bool typed;
};
constexpr CallStyle kPlainCall{plain_argument, false};
constexpr CallStyle kTypedCall{typed_argument, true};

// Whether a parameter goes between the call's parentheses in a style (a typed call writes its
// block after them).
bool is_listed(const ApiParam& param, const CallStyle& style) { return !(style.typed && param.block); }

std::string arguments_in(const ApiMethod& method, const CallStyle& style) {
  std::string text;
  for (const ApiParam& param : method.params) {
    if (is_listed(param, style)) text += (text.empty() ? "" : kSeparator) + style.argument(param);
  }
  return text;
}

std::string returned(const ApiMethod& method, const CallStyle& style) {
  return style.typed ? kReturnsMark + method.returns : std::string();
}

bool takes_block(const ApiMethod& method) { return !method.params.empty() && method.params.back().block; }

bool writes_block_after(const ApiMethod& method, const CallStyle& style) { return style.typed && takes_block(method); }

// " { (Float) -> void }" after a typed call that takes a block ("?{" when it may go without).
std::string block_after(const ApiMethod& method, const CallStyle& style) {
  if (!writes_block_after(method, style)) return {};
  const ApiParam& block = method.params.back();
  return std::string(" ") + (block.optional ? kOptionalBlockMark : "") + kBlockOpen + block_signature(block.type) +
         kBlockClose;
}

// Whether an element's method reads one ([] and the like) rather than writes one ([]=).
bool reads_element(const ApiMethod& method) {
  return method.name == kElement || method.params.size() < kElementWriteArguments;
}

// "[key]" or "[key] = value", typed: "[String key] -> untyped", "[String key] = untyped value".
std::string element_call(const ApiMethod& method, const CallStyle& style) {
  if (reads_element(method)) return "[" + arguments_in(method, style) + "]" + returned(method, style);
  return "[" + style.argument(method.params.front()) + "] = " + style.argument(method.params.back());
}

// How a call is written after what it is called on: "rotate(degrees)", "position = vector",
// "+ other", "[x, y, z]"; typed, "rotate(Vector3 degrees) -> void".
std::string call_of(const ApiMethod& method, const CallStyle& style) {
  const std::string arguments = arguments_in(method, style);
  if (method.name.front() == '[') return element_call(method, style);
  if (is_setter(method.name)) return method.name.substr(0, method.name.size() - 1) + " = " + arguments;
  if (is_operator(method.name))
    return (arguments.empty() ? method.name : method.name + " " + arguments) + returned(method, style);
  const std::string call = arguments.empty() ? method.name : method.name + "(" + arguments + ")";
  return call + block_after(method, style) + returned(method, style);
}

// `receiver` and the call, joined by `separator` unless the call is an element's ([...]).
std::string called_on(const std::string& receiver, const char* separator, const ApiMethod& method,
                      const CallStyle& style) {
  const std::string call = call_of(method, style);
  return call.front() == '[' ? receiver + call : receiver + separator + call;
}

// How a method is written inside its class's own text: "rotate(Vector3 degrees) -> void", or
// "Input.axis(Symbol name, pad: Integer = 0) -> Float".
std::string call_in_class(const ApiClass& owner, const ApiMethod& method) {
  return method.on_class ? called_on(owner.name, kClassCall, method, kTypedCall) : call_of(method, kTypedCall);
}

std::string guide_line(const ApiClass& owner) {
  if (owner.guide.empty()) return {};
  return tr("project.api.guide", {{"page", kGuidesFolder + owner.guide}}) + "\n";
}

std::string markdown_method(const ApiClass& owner, const ApiMethod& method) {
  return "- `" + call_in_class(owner, method) + "`: " + method.about + "\n";
}

std::string markdown_class(const ApiClass& owner) {
  std::string text = "## " + owner.name + "\n\n" + owner.about;
  if (!owner.guide.empty()) text += " See [" + owner.guide + "](" + owner.guide + ").";
  text += "\n\n";
  for (const ApiMethod& method : owner.methods) text += markdown_method(owner, method);
  if (!owner.notes.empty()) text += "\n" + owner.notes + "\n";
  return text + "\n";
}

// "behaviour" for the anchor of "Behaviour"; "animationgraph" for "Animation::Graph".
std::string anchor(const std::string& name) {
  std::string kept;
  for (const char letter : lowered(name)) {
    if (std::isalnum(static_cast<unsigned char>(letter)) != 0) kept.push_back(letter);
  }
  return kept;
}

nlohmann::json param_data(const ApiParam& param) {
  nlohmann::json data = {{kName, param.name}, {kType, param.type}};
  for (const auto& [key, on] : {std::pair{kKeyword, param.keyword}, std::pair{kRest, param.rest},
                                std::pair{kBlock, param.block}, std::pair{kOptional, param.optional}}) {
    if (on) data[key] = true;
  }
  if (!param.default_value.empty()) data[kDefault] = param.default_value;
  return data;
}

nlohmann::json method_data(const ApiMethod& method) {
  nlohmann::json data = {{kName, method.name}, {kAbout, method.about}, {kParams, nlohmann::json::array()}};
  for (const ApiParam& param : method.params) data[kParams].push_back(param_data(param));
  data[kReturns] = {{kType, method.returns}};
  if (method.on_class) data[kStatic] = true;
  if (method.top > 0) data[kTop] = method.top;
  if (!method.example.empty()) data[kExample] = method.example;
  return data;
}

nlohmann::json class_data(const ApiClass& owner) {
  nlohmann::json data = {{kName, owner.name},
                         {kKind, owner.is_module ? kModuleKind : kClassKind},
                         {kAbout, owner.about},
                         {kMethods, nlohmann::json::array()}};
  if (!owner.superclass.empty()) data[kSuperclass] = owner.superclass;
  if (!owner.guide.empty()) data[kGuide] = owner.guide;
  if (!owner.notes.empty()) data[kNotes] = owner.notes;
  for (const ApiMethod& method : owner.methods) data[kMethods].push_back(method_data(method));
  return data;
}

// A text as YAML writes it in double quotes (a front matter's value may hold ": ").
std::string quoted(const std::string& text) {
  std::string out = "\"";
  for (const char letter : text) {
    if (letter == '"' || letter == '\\') out += '\\';
    out += letter;
  }
  return out + "\"";
}

}  // namespace

const std::string& api_reference_json() {
  static const std::string text = [] {
    const auto* file =
        std::find_if(std::begin(generated::kAssistantFiles), std::end(generated::kAssistantFiles),
                     [](const generated::EmbeddedFile& each) { return std::strcmp(each.path, kReferenceFile) == 0; });
    return std::string(file->contents);
  }();
  return text;
}

const ApiReference& api_reference() {
  static const ApiReference reference = read_api_reference(api_reference_json());
  return reference;
}

std::string signature(const ApiClass& owner, const ApiMethod& method) {
  return called_on(owner.name, method.on_class ? kClassCall : kInstanceCall, method, kPlainCall);
}

std::string typed_signature(const ApiClass& owner, const ApiMethod& method) {
  return called_on(owner.name, method.on_class ? kClassCall : kInstanceCall, method, kTypedCall);
}

std::string arguments_of(const ApiMethod& method) { return arguments_in(method, kPlainCall); }

const ApiClass* find_api_class(const ApiReference& reference, const std::string& name) {
  const auto found = std::find_if(reference.classes.begin(), reference.classes.end(),
                                  [&name](const ApiClass& owner) { return lowered(owner.name) == lowered(name); });
  return found == reference.classes.end() ? nullptr : &*found;
}

std::string api_index(const ApiReference& reference) {
  std::string text = tr("project.api.index_header", {{"count", std::to_string(reference.classes.size())}}) + "\n";
  for (const ApiClass& owner : reference.classes) text += method_line(owner.name, owner.about);
  return text;
}

std::string api_class_text(const ApiClass& owner) {
  std::string text = owner.name + ": " + owner.about + "\n" + guide_line(owner);
  for (const ApiMethod& method : owner.methods) text += method_line(call_in_class(owner, method), method.about);
  if (!owner.notes.empty()) text += owner.notes + "\n";
  return text;
}

std::string api_search(const ApiReference& reference, const std::string& word, std::size_t most) {
  const std::vector<ApiCall> found = methods_about(reference, word);
  if (found.empty()) return tr("project.api.nothing_found", {{"word", word}}) + "\n";
  std::string text;
  for (std::size_t call = 0; call < found.size() && call < most; ++call) {
    text += method_line(typed_signature(*found[call].owner, *found[call].method), found[call].method->about);
  }
  if (found.size() > most)
    text += tr("project.api.more_found", {{"count", std::to_string(found.size() - most)}}) + "\n";
  return text;
}

std::vector<ApiCall> methods_about(const ApiReference& reference, const std::string& word) {
  std::vector<ApiCall> found;
  for (const ApiClass& owner : reference.classes) {
    for (const ApiMethod& method : owner.methods) {
      if (matches(owner, method, lowered(word))) found.push_back({&owner, &method});
    }
  }
  return found;
}

std::string api_json(const ApiReference& reference) {
  nlohmann::json data = {{kDescription, reference.description},
                         {kAbout, reference.about},
                         {kNotes, reference.notes},
                         {kClasses, nlohmann::json::array()}};
  for (const ApiClass& owner : reference.classes) data[kClasses].push_back(class_data(owner));
  return data.dump();
}

std::string class_json(const ApiClass& owner) { return class_data(owner).dump(); }

std::string search_json(const ApiReference& reference, const std::string& word, std::size_t most) {
  nlohmann::json found = nlohmann::json::array();
  for (const ApiCall& call : methods_about(reference, word)) {
    if (found.size() == most) break;
    nlohmann::json data = method_data(*call.method);
    data[kClassKey] = call.owner->name;
    found.push_back(data);
  }
  return found.dump();
}

std::string api_markdown(const ApiReference& reference) {
  std::string text = "---\ndescription: " + quoted(reference.description) + "\n---\n\n# API reference\n\n" +
                     reference.about + "\n\n| Class | What it is |\n|---|---|\n";
  for (const ApiClass& owner : reference.classes) {
    text += "| [`" + owner.name + "`](#" + anchor(owner.name) + ") | " + owner.about + " |\n";
  }
  text += "\n";
  for (const ApiClass& owner : reference.classes) text += markdown_class(owner);
  return text + "## Notes\n\n" + reference.notes + "\n";
}

std::vector<ApiCall> most_used_calls(const ApiReference& reference) {
  std::vector<ApiCall> calls;
  for (const ApiClass& owner : reference.classes) {
    for (const ApiMethod& method : owner.methods) {
      if (method.top > 0) calls.push_back({&owner, &method});
    }
  }
  std::sort(calls.begin(), calls.end(),
            [](const ApiCall& a, const ApiCall& b) { return a.method->top < b.method->top; });
  return calls;
}

}  // namespace redgem::tools
