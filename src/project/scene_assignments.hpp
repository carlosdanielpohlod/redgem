#pragma once

// What `redgem scene set` (and `add`) change on an object, a word each: `position=1,2,3`,
// `rotation=0,90,0` (degrees), `scale=2` or `scale=1,2,1`, `name=Gate`, `parent=Level` (or
// `parent=` for the top of the scene), `prop.speed=2.5` (`prop.speed=` removes it), and the
// scripts: `script+=Door`, `script-=Door`, `script=Door,Sound`. A line of `redgem scene list` is
// taken back word for word: `pos`, `rot`, `scripts`, a property as `speed=2.5` (a key that is a
// typo of one of these is refused, not made a property), and `mesh=`, `light=` and `camera` as
// they already are (they say what an object is, and are not changed here).

#include <filesystem>
#include <string>
#include <vector>

#include "editor/document/scene_document.hpp"

namespace redgem::tools {

// The scene being changed, and what the changes found worth saying.
struct SceneEdit {
  SceneDocument& document;
  std::filesystem::path scripts_dir;  // where the scripts attached are looked for
  std::vector<std::string> notes;     // a script attached that is not there yet, say
};

// Applies one word to the object `index`, which moving it under another parent renumbers.
// Empty when it was applied; what is wrong (and what would be right) when it was not.
std::string apply_assignment(SceneEdit& edit, int& index, const std::string& word);

// A property's value as typed: true or false, a number, or else a text (quotes around it are
// left out).
PropertyValue typed_value(const std::string& text);

// What is wrong with a name an object is given: nothing, a name that reads as an id ("#3"), or one
// with a '/' (a path): no lookup would find the object by it. Empty when nothing is.
std::string name_problem(const std::string& name);

// A script as the `script` property names it: "Door" -> "door.rb"; "door.rb" stays.
std::string script_file_for(const std::string& name);

}  // namespace redgem::tools
