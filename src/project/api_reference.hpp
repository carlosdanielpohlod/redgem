#pragma once

// The scripting API as data (`redgem api`): every class and method games use, with the types of
// its parameters and of what it returns, and one line about it, read from docs/scripting/api.json,
// which is built into the program. That file is the one source of truth: docs/scripting/api.md
// is made from it (`redgem api --markdown`), and so are the type signatures editors read
// (docs/scripting/redgem.rbs, `redgem api --rbs`, see api_rbs.hpp); the guide new games get lists
// its most used calls, and tests keep it in step with the classes and methods the engine really
// defines, their parameters included.
//
// A method in that file:
//   {"name": "tween", "params": [P...], "returns": {"type": "Behaviour::Timer"}, "about": "...",
//    "static": true (called on the class or module), "top": 3, "example": "..."}
// and each of its parameters, in the order a call gives them:
//   {"name": "ease", "type": "Symbol", "keyword": true, "optional": true, "default": ":smooth"}
// with "keyword" (ease: :linear), "rest" (*names, or **settings with "keyword"), "block" (&block,
// last), "optional" and "default" (Ruby, as a call would write it; every optional parameter has
// one, but a block and a rest) all left out when false or empty. Types are in api_types.hpp's
// language. A class: {"name", "kind": "class" or "module", "superclass" (when not Object),
// "about", "guide", "methods", "notes", "internal"}.

#include <cstddef>
#include <string>
#include <vector>

namespace redgem::tools {

struct ApiParam {
  std::string name;
  std::string type;           // in api_types.hpp's language
  bool keyword = false;       // given by its name: loop: true
  bool rest = false;          // takes the rest: *bindings (or **settings, a keyword one)
  bool block = false;         // &block: its type is a block's, ^(Float) -> void
  bool optional = false;      // may be left out
  std::string default_value;  // Ruby, as a call writes it, when left out (":smooth"); empty without one
};

struct ApiMethod {
  std::string name;
  std::vector<ApiParam> params;
  std::string returns;    // its type: what the method gives back
  std::string about;      // one line
  bool on_class = false;  // called on the class or module (Input.axis), not on an instance
  int top = 0;            // its rank among the calls games make most (1 is first); 0 when not ranked
  std::string example;    // a line of Ruby, for the ranked ones
};

struct ApiClass {
  std::string name;
  bool is_module = false;  // a module (Input), not a class (Vector3)
  std::string superclass;  // the class it comes from, when not Object (GameMenu: Behaviour)
  std::string about;
  std::string guide;  // the manual's page about it, from docs/scripting/ ("behaviours.md")
  std::string notes;  // what one line per method cannot say
  std::vector<ApiMethod> methods;
  // Methods the class has for the engine's own use (public in Ruby, not for games): listed so
  // the tests know they are not forgotten, and shown nowhere.
  std::vector<std::string> internal;
};

struct ApiReference {
  // Classes the engine keeps for itself (public in Ruby, not for games), by path: listed so the
  // tests know they are not forgotten, and shown nowhere.
  std::vector<std::string> internal_classes;
  std::string about;
  std::string description;  // the manual page's, one line
  std::string notes;
  std::vector<ApiClass> classes;
};

// The reference a JSON text holds (docs/scripting/api.json's shape, above). Throws nlohmann's
// exceptions when it is not JSON or misses a field, and std::invalid_argument, saying where and
// what, when it holds something else: a field nobody reads, a type not in the language, a
// parameter out of place, an optional one with no default.
ApiReference read_api_reference(const std::string& json);
// The one built into the program.
const ApiReference& api_reference();
// The JSON text built into the program.
const std::string& api_reference_json();

// How a call is written: "Input.axis(name, pad: 0)", "Transform#rotate(degrees)".
std::string signature(const ApiClass& owner, const ApiMethod& method);
// ... with its types: "Input.axis(Symbol name, pad: Integer = 0) -> Float".
std::string typed_signature(const ApiClass& owner, const ApiMethod& method);
// The arguments a call is written with: "name, loop: true, &block".
std::string arguments_of(const ApiMethod& method);

// The class of that name (any case); nullptr when there is none.
const ApiClass* find_api_class(const ApiReference& reference, const std::string& name);

// A line per class: its name, what it is, how many methods it has.
std::string api_index(const ApiReference& reference);
// One class: a line per method, with its types, then its notes.
std::string api_class_text(const ApiClass& owner);
// The methods whose call (with its types) or line holds `word` (any case), at most `most`, a line each.
std::string api_search(const ApiReference& reference, const std::string& word, std::size_t most);
// docs/scripting/api.md.
std::string api_markdown(const ApiReference& reference);

// A method, with the class it is of.
struct ApiCall {
  const ApiClass* owner;
  const ApiMethod* method;
};
// The calls games make most, in their rank's order.
std::vector<ApiCall> most_used_calls(const ApiReference& reference);
// The methods whose call (with its types) or line holds `word` (any case), in the reference's order.
std::vector<ApiCall> methods_about(const ApiReference& reference, const std::string& word);

// The same as data, for tools (compact JSON; the methods the engine keeps for itself left out):
// the whole reference, one class, the methods a search finds (at most `most`, each with its class).
std::string api_json(const ApiReference& reference);
std::string class_json(const ApiClass& owner);
std::string search_json(const ApiReference& reference, const std::string& word, std::size_t most);

}  // namespace redgem::tools
