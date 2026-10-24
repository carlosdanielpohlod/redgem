#include "project/api_reference.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iterator>

#include <nlohmann/json.hpp>

#include "core/translations.hpp"
#include "project/closest_words.hpp"
#include "generated/assistant_files.hpp"

namespace redgem::tools {
namespace {

// Where the reference is in the repository, as it was built in.
constexpr const char* kReferenceFile = "docs/scripting/api.json";
// The keys of that file.
constexpr const char* kAbout = "about";
constexpr const char* kDescription = "description";  // of the manual's page, for its index
constexpr const char* kNotes = "notes";
constexpr const char* kClasses = "classes";
constexpr const char* kName = "name";
constexpr const char* kGuide = "guide";
constexpr const char* kMethods = "methods";
constexpr const char* kInternal = "internal";
constexpr const char* kArgs = "args";
constexpr const char* kStatic = "static";
constexpr const char* kTop = "top";
constexpr const char* kExample = "example";
constexpr const char* kClassKey = "class";  // what a found method says it is of
// How texts lay a method out.
constexpr std::size_t kSignatureColumn = 34;  // where a method's line starts after its signature
constexpr const char* kIndent = "  ";
constexpr const char* kClassCall = ".";
constexpr const char* kInstanceCall = "#";
constexpr const char* kGuidesFolder = "docs/scripting/";
constexpr const char* kElement = "[]";  // reads an element; "[]=" writes one

std::string text_of(const nlohmann::json& json, const char* key) { return json.value(key, std::string()); }

ApiMethod method_from(const nlohmann::json& json) {
  ApiMethod method;
  method.name = text_of(json, kName);
  method.args = text_of(json, kArgs);
  method.about = text_of(json, kAbout);
  method.on_class = json.value(kStatic, false);
  method.top = json.value(kTop, 0);
  method.example = text_of(json, kExample);
  return method;
}

ApiClass class_from(const nlohmann::json& json) {
  ApiClass owner;
  owner.name = text_of(json, kName);
  owner.about = text_of(json, kAbout);
  owner.guide = text_of(json, kGuide);
  owner.notes = text_of(json, kNotes);
  for (const nlohmann::json& method : json.at(kMethods)) owner.methods.push_back(method_from(method));
  owner.internal = json.value(kInternal, std::vector<std::string>());
  return owner;
}

bool holds(const std::string& text, const std::string& word) { return lowered(text).find(word) != std::string::npos; }

bool matches(const ApiClass& owner, const ApiMethod& method, const std::string& word) {
  return holds(signature(owner, method), word) || holds(method.about, word);
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

// "[]=" with "key, value" -> "[key] = value".
std::string element_call(const ApiMethod& method) {
  const std::size_t comma = method.args.find(", ");
  if (method.name == kElement || comma == std::string::npos) return "[" + method.args + "]";
  return "[" + method.args.substr(0, comma) + "] = " + method.args.substr(comma + std::strlen(", "));
}

// How a call is written after what it is called on: "rotate(degrees)", "position = vector",
// "+ other", "[x, y, z]".
std::string call_of(const ApiMethod& method) {
  if (method.name.front() == '[') return element_call(method);
  if (is_setter(method.name)) return method.name.substr(0, method.name.size() - 1) + " = " + method.args;
  if (is_operator(method.name)) return method.args.empty() ? method.name : method.name + " " + method.args;
  return method.args.empty() ? method.name : method.name + "(" + method.args + ")";
}

// `receiver` and the call, joined by `separator` unless the call is an element's ([...]).
std::string called_on(const std::string& receiver, const char* separator, const ApiMethod& method) {
  const std::string call = call_of(method);
  return call.front() == '[' ? receiver + call : receiver + separator + call;
}

// How a method is written inside its class's own text: "rotate(degrees)", or "Input.axis(name)".
std::string call_in_class(const ApiClass& owner, const ApiMethod& method) {
  return method.on_class ? called_on(owner.name, kClassCall, method) : call_of(method);
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

nlohmann::json method_data(const ApiMethod& method) {
  nlohmann::json data = {{kName, method.name}, {kAbout, method.about}};
  if (!method.args.empty()) data[kArgs] = method.args;
  if (method.on_class) data[kStatic] = true;
  if (method.top > 0) data[kTop] = method.top;
  if (!method.example.empty()) data[kExample] = method.example;
  return data;
}

nlohmann::json class_data(const ApiClass& owner) {
  nlohmann::json data = {{kName, owner.name}, {kAbout, owner.about}, {kMethods, nlohmann::json::array()}};
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

ApiReference read_api_reference(const std::string& json_text) {
  const nlohmann::json json = nlohmann::json::parse(json_text);
  ApiReference reference;
  reference.about = text_of(json, kAbout);
  reference.description = text_of(json, kDescription);
  reference.notes = text_of(json, kNotes);
  for (const nlohmann::json& owner : json.at(kClasses)) reference.classes.push_back(class_from(owner));
  return reference;
}

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
  return called_on(owner.name, method.on_class ? kClassCall : kInstanceCall, method);
}

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
    text += method_line(signature(*found[call].owner, *found[call].method), found[call].method->about);
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
