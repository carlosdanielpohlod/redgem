#pragma once

// The scripting API as data (`redgem api`): every class and method games use, with its arguments
// and one line about it, read from docs/scripting/api.json, which is built into the program. That
// file is the one source of truth: docs/scripting/api.md is made from it (`redgem api --markdown`),
// the guide new games get lists its most used calls, and tests keep it in step with the classes
// and methods the engine really defines.

#include <cstddef>
#include <string>
#include <vector>

namespace redgem::tools {

struct ApiMethod {
  std::string name;
  std::string args;       // as a call writes them: "name, loop: true"; empty without any
  std::string about;      // one line
  bool on_class = false;  // called on the class or module (Input.axis), not on an instance
  int top = 0;            // its rank among the calls games make most (1 is first); 0 when not ranked
  std::string example;    // a line of Ruby, for the ranked ones
};

struct ApiClass {
  std::string name;
  std::string about;
  std::string guide;  // the manual's page about it, from docs/scripting/ ("behaviours.md")
  std::string notes;  // what one line per method cannot say
  std::vector<ApiMethod> methods;
  // Methods the class has for the engine's own use (public in Ruby, not for games): listed so
  // the tests know they are not forgotten, and shown nowhere.
  std::vector<std::string> internal;
};

struct ApiReference {
  std::string about;
  std::string description;  // the manual page's, one line
  std::string notes;
  std::vector<ApiClass> classes;
};

// The reference a JSON text holds (docs/scripting/api.json's shape). Throws nlohmann's
// exceptions when it holds something else.
ApiReference read_api_reference(const std::string& json);
// The one built into the program.
const ApiReference& api_reference();
// The JSON text built into the program.
const std::string& api_reference_json();

// How a call is written: "Input.axis(name)", "Transform#rotate(degrees)".
std::string signature(const ApiClass& owner, const ApiMethod& method);

// The class of that name (any case); nullptr when there is none.
const ApiClass* find_api_class(const ApiReference& reference, const std::string& name);

// A line per class: its name, what it is, how many methods it has.
std::string api_index(const ApiReference& reference);
// One class: a line per method, then its notes.
std::string api_class_text(const ApiClass& owner);
// The methods whose name or line holds `word` (any case), at most `most`, a line each.
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
// The methods whose call or line holds `word` (any case), in the reference's order.
std::vector<ApiCall> methods_about(const ApiReference& reference, const std::string& word);

// The same as data, for tools (compact JSON; the methods the engine keeps for itself left out):
// the whole reference, one class, the methods a search finds (at most `most`, each with its class).
std::string api_json(const ApiReference& reference);
std::string class_json(const ApiClass& owner);
std::string search_json(const ApiReference& reference, const std::string& word, std::size_t most);

}  // namespace redgem::tools
