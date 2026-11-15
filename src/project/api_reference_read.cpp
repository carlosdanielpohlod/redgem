// Reading docs/scripting/api.json, and refusing what it may not hold (see api_reference.hpp).

#include <algorithm>
#include <cctype>
#include <iterator>
#include <set>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "project/api_json_keys.hpp"
#include "project/api_reference.hpp"
#include "project/api_types.hpp"

namespace redgem::tools {
namespace {

using nlohmann::json;
using namespace api_keys;

// The fields each part may have: any other is a mistake (or a field from before the types).
constexpr const char* kReferenceFields[] = {kDescription, kAbout, kNotes, kInternalClasses, kClasses};
constexpr const char* kClassFields[] = {kName,  kStability, kKind,  kSuperclass, kAbout,
                                        kGuide, kMethods,   kNotes, kInternal};
constexpr const char* kMethodFields[] = {kName, kStability, kStatic, kParams, kReturns, kAbout, kTop, kExample};
constexpr const char* kReturnsFields[] = {kType};
constexpr const char* kParamFields[] = {kName, kType, kKeyword, kRest, kBlock, kOptional, kDefault};
constexpr const char* kTopPlace = "the reference";
constexpr const char* kClassCall = ".";
constexpr const char* kInstanceCall = "#";
constexpr const char* kNewMethod = "new";
constexpr const char* kElementWriter = "[]=";
constexpr std::size_t kElementWriterParams = 2;  // at least a key and the value
constexpr const char* kPathSeparator = "::";     // Dialogue::Script is in Dialogue

[[noreturn]] void refuse(const std::string& where, const std::string& why) {
  throw std::invalid_argument(std::string(kReferenceFile) + ": " + where + ": " + why);
}

template <std::size_t Count>
void check_fields(const json& data, const char* const (&known)[Count], const std::string& where) {
  for (const auto& field : data.items()) {
    const bool is_known = std::find(std::begin(known), std::end(known), field.key()) != std::end(known);
    if (!is_known) refuse(where, "no field is called " + field.key());
  }
}

std::string text_of(const json& data, const char* key) { return data.value(key, std::string()); }
bool flag_of(const json& data, const char* key) { return data.value(key, false); }

// A class's or a method's stability; Unmarked when it has none (which the tests refuse).
ApiStability stability_of(const json& data, const std::string& where) {
  if (!data.contains(kStability)) return ApiStability::Unmarked;
  const json& mark = data.at(kStability);
  if (mark == kStable) return ApiStability::Stable;
  if (mark == kExperimental) return ApiStability::Experimental;
  refuse(where, "its stability is stable or experimental, not " + mark.dump());
}

ApiParam param_from(const json& data, const std::string& where) {
  check_fields(data, kParamFields, where);
  ApiParam param;
  param.name = data.at(kName).get<std::string>();
  param.type = data.at(kType).get<std::string>();
  param.keyword = flag_of(data, kKeyword);
  param.rest = flag_of(data, kRest);
  param.block = flag_of(data, kBlock);
  param.optional = flag_of(data, kOptional);
  param.default_value = text_of(data, kDefault);
  return param;
}

// "Input.axis", "Transform#rotate": where a method's mistakes are.
std::string method_place(const std::string& owner, const json& data) {
  return owner + (flag_of(data, kStatic) ? kClassCall : kInstanceCall) + text_of(data, kName);
}

ApiMethod method_from(const json& data, const std::string& owner) {
  const std::string where = method_place(owner, data);
  check_fields(data, kMethodFields, where);
  check_fields(data.at(kReturns), kReturnsFields, where);
  ApiMethod method;
  method.name = data.at(kName).get<std::string>();
  for (const json& param : data.at(kParams)) method.params.push_back(param_from(param, where));
  method.returns = data.at(kReturns).at(kType).get<std::string>();
  method.about = text_of(data, kAbout);
  method.on_class = flag_of(data, kStatic);
  method.top = data.value(kTop, 0);
  method.example = text_of(data, kExample);
  method.stability = stability_of(data, where);
  return method;
}

bool is_kind(const std::string& kind) { return kind == kClassKind || kind == kModuleKind; }

bool is_module_kind(const json& data, const std::string& where) {
  const std::string kind = data.at(kKind).get<std::string>();
  if (!is_kind(kind)) refuse(where, "its kind is class or module, not " + kind);
  return kind == kModuleKind;
}

ApiClass class_from(const json& data) {
  const std::string name = data.at(kName).get<std::string>();
  check_fields(data, kClassFields, name);
  ApiClass owner;
  owner.name = name;
  owner.is_module = is_module_kind(data, name);
  owner.superclass = text_of(data, kSuperclass);
  owner.about = text_of(data, kAbout);
  owner.guide = text_of(data, kGuide);
  owner.notes = text_of(data, kNotes);
  owner.stability = stability_of(data, name);
  for (const json& method : data.at(kMethods)) owner.methods.push_back(method_from(method, name));
  owner.internal = data.value(kInternal, std::vector<std::string>());
  return owner;
}

// Where a parameter stands in a call: the positional ones first, then *rest, the keywords,
// **rest and the block, in that order.
enum class Slot { Positional, Rest, Keyword, KeywordRest, Block };

Slot slot_of(const ApiParam& param) {
  if (param.block) return Slot::Block;
  if (param.rest) return param.keyword ? Slot::KeywordRest : Slot::Rest;
  return param.keyword ? Slot::Keyword : Slot::Positional;
}

// Whether a call may have more than one parameter of a slot.
bool is_repeatable(Slot slot) { return slot == Slot::Positional || slot == Slot::Keyword; }

bool is_positional(const ApiParam& param) { return slot_of(param) == Slot::Positional; }

// Whether a positional parameter a call needs comes after one it may leave out (Ruby allows it;
// RBS cannot say it).
bool is_needed_after_optional(const ApiParam& before, const ApiParam& param) {
  return is_positional(before) && is_positional(param) && before.optional && !param.optional;
}

bool is_out_of_place(const ApiParam& before, const ApiParam& param) {
  const Slot first = slot_of(before);
  const Slot slot = slot_of(param);
  return slot < first || (slot == first && !is_repeatable(slot)) || is_needed_after_optional(before, param);
}

void check_order(const std::vector<ApiParam>& params, const std::string& where) {
  for (std::size_t at = 1; at < params.size(); ++at) {
    if (is_out_of_place(params[at - 1], params[at])) refuse(where, params[at].name + " is out of place");
  }
}

bool is_name_start(char letter) { return std::islower(static_cast<unsigned char>(letter)) != 0 || letter == '_'; }
bool is_name_letter(char letter) { return std::isalnum(static_cast<unsigned char>(letter)) != 0 || letter == '_'; }

// Whether a parameter's name is one Ruby (and RBS) can write: speed, max_distance, _unused.
bool is_parameter_name(const std::string& name) {
  return !name.empty() && is_name_start(name.front()) && std::all_of(name.begin(), name.end(), is_name_letter);
}

bool has_line_break(const std::string& text) { return text.find('\n') != std::string::npos; }

bool mixes_kinds(const ApiParam& param) { return param.block && (param.keyword || param.rest); }
bool is_optional_rest(const ApiParam& param) { return param.rest && param.optional; }
bool misses_default(const ApiParam& param) { return param.optional && !param.block && param.default_value.empty(); }
bool has_stray_default(const ApiParam& param) {
  return !param.default_value.empty() && (!param.optional || param.block);
}

void check_flags(const ApiParam& param, const std::string& where) {
  if (mixes_kinds(param)) refuse(where, "a block is neither a keyword nor a rest");
  if (is_optional_rest(param)) refuse(where, "a rest is never optional");
  if (misses_default(param)) refuse(where, "optional, with no default");
  if (has_stray_default(param)) refuse(where, "a default, but it is not optional");
}

void check_type(const std::string& type, const std::vector<std::string>& classes, TypePlace place,
                const std::string& where) {
  const std::string problem = api_type_problem(type, classes, place);
  if (!problem.empty()) refuse(where, "type " + type + ": " + problem);
}

void check_param(const ApiParam& param, const std::vector<std::string>& classes, const std::string& where) {
  if (!is_parameter_name(param.name)) refuse(where, "a parameter cannot be called \"" + param.name + "\"");
  const std::string place = where + " " + param.name;
  check_flags(param, place);
  check_type(param.type, classes, param.block ? TypePlace::Block : TypePlace::Parameter, place);
}

// Whether a module says it has new (only a class makes objects; RBS writes a class's new as its
// initialize).
bool is_module_new(const ApiClass& owner, const ApiMethod& method) {
  return owner.is_module && method.on_class && method.name == kNewMethod;
}

// Whether an element's writer ([]=) misses its keys or its value.
bool is_short_element_writer(const ApiMethod& method) {
  return method.name == kElementWriter && method.params.size() < kElementWriterParams;
}

// Whether a method promises more than its class: a stable one in an experimental class.
bool is_stable_in_experimental(const ApiClass& owner, const ApiMethod& method) {
  return owner.stability == ApiStability::Experimental && method.stability == ApiStability::Stable;
}

// "Input.axis", "Transform#rotate".
std::string method_where(const ApiClass& owner, const ApiMethod& method) {
  return owner.name + (method.on_class ? kClassCall : kInstanceCall) + method.name;
}

void check_method(const ApiClass& owner, const ApiMethod& method, const std::vector<std::string>& classes) {
  const std::string where = method_where(owner, method);
  if (has_line_break(method.about)) refuse(where, "its about is one line");
  if (is_module_new(owner, method)) refuse(where, "a module has no new: call its maker otherwise");
  if (is_short_element_writer(method)) refuse(where, "[]= takes its keys, then the value");
  if (is_stable_in_experimental(owner, method)) refuse(where, "stable, in an experimental class");
  for (const ApiParam& param : method.params) check_param(param, classes, where);
  check_order(method.params, where);
  check_type(method.returns, classes, TypePlace::Returned, where);
}

// Whether a module says it comes from a class (only a class does; which one, the tests check
// against the engine).
bool is_module_with_superclass(const ApiClass& owner) { return owner.is_module && !owner.superclass.empty(); }

void check_class(const ApiClass& owner, const std::vector<std::string>& classes) {
  if (is_module_with_superclass(owner)) refuse(owner.name, "a module comes from no class");
  if (has_line_break(owner.about)) refuse(owner.name, "its about is one line");
  for (const ApiMethod& method : owner.methods) check_method(owner, method, classes);
}

bool is_stable(ApiStability stability) { return stability == ApiStability::Stable; }

// The classes games may not rely on yet.
std::set<std::string> experimental_classes(const ApiReference& reference) {
  std::set<std::string> experimental;
  for (const ApiClass& owner : reference.classes) {
    if (owner.stability == ApiStability::Experimental) experimental.insert(owner.name);
  }
  return experimental;
}

bool is_path_letter(char letter) { return is_name_letter(letter) || letter == ':'; }

// The words of a type, class paths kept whole: "Array[Behaviour::Timer] | nil" has Array,
// Behaviour::Timer and nil.
std::vector<std::string> words_in(const std::string& type) {
  std::vector<std::string> words;
  for (std::size_t at = 0; at < type.size();) {
    std::size_t end = at;
    while (end < type.size() && is_path_letter(type[end])) ++end;
    if (end > at) words.push_back(type.substr(at, end - at));
    at = end + 1;
  }
  return words;
}

bool names_experimental(const std::string& type, const std::set<std::string>& experimental) {
  const std::vector<std::string> words = words_in(type);
  return std::any_of(words.begin(), words.end(), [&](const std::string& word) { return experimental.count(word) > 0; });
}

// Whether a method takes or returns a class that may change.
bool leans_on_experimental(const ApiMethod& method, const std::set<std::string>& experimental) {
  if (names_experimental(method.returns, experimental)) return true;
  return std::any_of(method.params.begin(), method.params.end(),
                     [&](const ApiParam& param) { return names_experimental(param.type, experimental); });
}

bool is_stable_on_experimental(const ApiMethod& method, const std::set<std::string>& experimental) {
  return is_stable(method.stability) && leans_on_experimental(method, experimental);
}

// The class a nested one is in: "Dialogue" for "Dialogue::Script"; nothing for one at the top.
std::string outer_of(const std::string& name) {
  const std::size_t at = name.rfind(kPathSeparator);
  return at == std::string::npos ? std::string() : name.substr(0, at);
}

// Whether a stable class is in, or comes from, a class that may change.
bool is_stable_on_experimental(const ApiClass& owner, const std::set<std::string>& experimental) {
  const bool in_or_from = experimental.count(outer_of(owner.name)) > 0 || experimental.count(owner.superclass) > 0;
  return is_stable(owner.stability) && in_or_from;
}

// Nothing stable rests on what may change: a stable class is in and comes from stable ones, and
// a stable method takes and returns stable classes.
void check_stability(const ApiClass& owner, const std::set<std::string>& experimental) {
  if (is_stable_on_experimental(owner, experimental)) refuse(owner.name, "stable, in or from an experimental class");
  for (const ApiMethod& method : owner.methods) {
    if (is_stable_on_experimental(method, experimental))
      refuse(method_where(owner, method), "stable, but takes or returns an experimental class");
  }
}

void check_reference(const ApiReference& reference) {
  std::vector<std::string> classes;
  classes.reserve(reference.classes.size());
  for (const ApiClass& owner : reference.classes) classes.push_back(owner.name);
  const std::set<std::string> experimental = experimental_classes(reference);
  for (const ApiClass& owner : reference.classes) {
    check_class(owner, classes);
    check_stability(owner, experimental);
  }
}

}  // namespace

ApiReference read_api_reference(const std::string& json_text) {
  const json data = json::parse(json_text);
  check_fields(data, kReferenceFields, kTopPlace);
  ApiReference reference;
  reference.about = text_of(data, kAbout);
  reference.description = text_of(data, kDescription);
  reference.internal_classes = data.value(kInternalClasses, std::vector<std::string>());
  reference.notes = text_of(data, kNotes);
  for (const json& owner : data.at(kClasses)) reference.classes.push_back(class_from(owner));
  check_reference(reference);
  return reference;
}

}  // namespace redgem::tools
