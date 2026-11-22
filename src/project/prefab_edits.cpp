#include "project/prefab_edits.hpp"

#include <stdexcept>
#include <vector>

#include "core/config.hpp"
#include "editor/prefab_instances.hpp"
#include "project/scene_lookup.hpp"
#include "project/scene_prefab_words.hpp"
#include "project/scene_text.hpp"

namespace redgem::tools {
namespace {

namespace fs = std::filesystem;
using mcp::Json;
using Words = std::vector<std::string>;

std::invalid_argument wrong(const std::string& what) { return std::invalid_argument(what); }

struct Placement {
  SceneDocument& document;
  const fs::path& game_dir;
  int index;
};

int placement_named(const SceneDocument& document, const Json& args) {
  if (!args.contains("object")) throw wrong("scene_prefab needs object: the placement");
  const FoundObject found = find_object(document.desc(), args["object"].get<std::string>());
  if (found.index == kNoIndex) throw wrong(found.error);
  if (!prefab_instances::is_placement(document, found.index))
    throw wrong(args["object"].get<std::string>() + " is not a prefab placement (it has no prop.prefab_file)");
  return found.index;
}

// The part's path: "" (or ".") for the placement itself.
std::string part_of(const Json& args) {
  const std::string part = args.value("part", std::string());
  return part == kPlacementRootWord ? std::string() : part;
}

std::string listing(const Placement& placement) {
  const auto fields = prefab_instances::fields(placement.document, placement.index);
  const std::string line = object_line(placement.document.desc(), placement.index) + "\n";
  return line + (fields.empty() ? std::string("no overrides\n") : overrides_listing(fields));
}

PrefabOutcome changed(const Placement& placement, const Words& notes) {
  std::string text = listing(placement);
  for (const auto& note : notes) text += "note: " + note + "\n";
  return {text, placement.index};
}

void check(bool done, const std::string& error) {
  if (!done) throw wrong(error);
}

// The inherited part at `path`, with the placement's override of it (`own`) or as the file gives it.
NodeDesc part_as(const Placement& placement, const std::string& path, bool own) {
  std::string error;
  auto part = inherited_part(placement.document, placement.index, placement.game_dir, path, own, error);
  if (!part) throw wrong(error);
  return *part;
}

// The whole override the assignments make of the part, against the file's part.
prefabs::PartOverride part_override(const Placement& placement, const std::string& path, const Words& words,
                                    Words& notes) {
  const NodeDesc current = part_as(placement, path, true);
  const NodeDesc base = part_as(placement, path, false);
  std::string error;
  auto change = part_change(current, base, words, placement.game_dir / config::kScriptsDirName, error, notes);
  if (!change) throw wrong(error);
  const auto overrides = prefab_instances::overrides_of(placement.document, placement.index);
  const auto old = overrides.find(path);
  if (old != overrides.end()) change->removed = change->removed || old->second.removed;
  return *change;
}

PrefabOutcome override_part(const Placement& placement, const Json& args) {
  if (!args.contains("part")) throw wrong("override needs part: the inherited part's path (\".\" for the placement)");
  const Words words = args.value("assignments", Words{});
  if (words.empty()) throw wrong("override needs assignments, such as position=0,1,0 or removed=true");
  Words notes;
  const std::string path = part_of(args);
  const auto change = part_override(placement, path, words, notes);
  std::string error;
  check(prefab_instances::replace_override(placement.document, placement.index, path, change, error), error);
  return changed(placement, notes);
}

PrefabOutcome revert(const Placement& placement, const Json& args) {
  const std::string field = args.value("field", std::string());
  if (!field.empty() && !args.contains("part")) throw wrong("revert's field needs the part it is of");
  std::string error;
  check(prefab_instances::revert(placement.document, placement.index, part_of(args), field, error), error);
  return changed(placement, {});
}

void require_confirm(const std::string& action, const Json& args) {
  if (!args.value("confirm", false)) throw wrong(action + " needs confirm=true");
}

PrefabOutcome apply(const Placement& placement, const Json& args) {
  require_confirm("apply (it writes the prefab file, which every placement of it reads)", args);
  std::string error;
  check(prefab_instances::apply(placement.document, placement.index, placement.game_dir, error), error);
  return changed(placement, {});
}

PrefabOutcome unpack(const Placement& placement, const Json& args) {
  require_confirm("unpack (the placement becomes plain objects)", args);
  std::string error;
  const int root = prefab_instances::unpack(placement.document, placement.index, placement.game_dir, error);
  check(root != kNoIndex, error);
  return {object_line(placement.document.desc(), root) + "\n", root};
}

using Action = PrefabOutcome (*)(const Placement&, const Json&);
struct Known {
  const char* name;
  Action run;
};
constexpr Known kActions[] = {
    {"overrides", [](const Placement& p, const Json&) { return PrefabOutcome{listing(p), p.index}; }},
    {"override", override_part},
    {"revert", revert},
    {"apply", apply},
    {"unpack", unpack},
};

}  // namespace

PrefabOutcome edit_prefab(SceneDocument& document, const fs::path& game_dir, const Json& args) {
  const std::string action = args.at("action");
  const Placement placement{document, game_dir, placement_named(document, args)};
  for (const auto& known : kActions) {
    if (action == known.name) return known.run(placement, args);
  }
  throw wrong("unknown scene_prefab action: " + action);
}

bool reads_prefab(const std::string& action) { return action == "overrides"; }

}  // namespace redgem::tools
