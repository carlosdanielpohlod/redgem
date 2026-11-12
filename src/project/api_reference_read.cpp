// Reading docs/scripting/api.json, and refusing what it may not hold (see api_reference.hpp).

#include <algorithm>
#include <cctype>
#include <iterator>
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
constexpr const char* kClassFields[] = {kName, kKind, kSuperclass, kAbout, kGuide, kMethods, kNotes, kInternal};
constexpr const char* kMethodFields[] = {kName, kStatic, kParams, kReturns, kAbout, kTop, kExample};
constexpr const char* kReturnsFields[] = {kType};
constexpr const char* kParamFields[] = {kName, kType, kKeyword, kRest, kBlock, kOptional, kDefault};
constexpr const char* kTopPlace = "the reference";
constexpr const char* kClassCall = ".";
constexpr const char* kInstanceCall = "#";

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

void check_method(const ApiClass& owner, const ApiMethod& method, const std::vector<std::string>& classes) {
  const std::string where = owner.name + (method.on_class ? kClassCall : kInstanceCall) + method.name;
  if (has_line_break(method.about)) refuse(where, "its about is one line");
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

void check_reference(const ApiReference& reference) {
  std::vector<std::string> classes;
  for (const ApiClass& owner : reference.classes) classes.push_back(owner.name);
  for (const ApiClass& owner : reference.classes) check_class(owner, classes);
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
