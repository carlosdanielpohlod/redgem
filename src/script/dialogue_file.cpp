// Reading and looking over a game's dialogue files (see dialogue_file.hpp).

#include "script/dialogue_file.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <set>

#include "core/config.hpp"
#include "core/safe_path.hpp"
#include "script/game_json_files.hpp"

namespace redgem::dialogue {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;
using Keys = std::initializer_list<const char*>;

constexpr const char* kStart = "start";
constexpr const char* kNodes = "nodes";
constexpr const char* kText = "text";
constexpr const char* kSpeaker = "speaker";
constexpr const char* kVoice = "voice";
constexpr const char* kEffects = "effects";
constexpr const char* kNext = "next";
constexpr const char* kChoices = "choices";
constexpr const char* kEnd = "end";
constexpr const char* kIf = "if";
constexpr const char* kVar = "var";
constexpr const char* kQuest = "quest";
constexpr const char* kTo = "to";
constexpr const char* kBy = "by";
constexpr const char* kData = "data";
constexpr std::array<const char*, 2> kSubjects = {kVar, kQuest};
constexpr std::array<const char*, 4> kComparisons = {"is", "not", "at_least", "at_most"};
constexpr std::array<const char*, 2> kStageComparisons = {"is", "not"};
constexpr std::array<const char*, 2> kNumberComparisons = {"at_least", "at_most"};
constexpr std::array<const char*, 4> kEffectKinds = {"set", "add", kQuest, "event"};
// What each kind of effect holds besides its own key, and whether that is a must.
struct EffectShape {
  const char* kind;
  const char* value;  // nullptr: nothing else
  bool needed;
};
constexpr std::array<EffectShape, 4> kEffectShapes = {
    {{"set", kTo, true}, {"add", kBy, true}, {kQuest, kTo, true}, {"event", kData, false}}};
constexpr std::size_t kOneWay = 1;  // a node or a choice goes on in exactly one way

// Whether `json` has a key, from a list.
template <typename List>
std::size_t count_of(const Json& json, const List& keys) {
  return static_cast<std::size_t>(
      std::count_if(keys.begin(), keys.end(), [&](const char* key) { return json.contains(key); }));
}

template <typename List>
const char* first_of(const Json& json, const List& keys) {
  for (const char* key : keys) {
    if (json.contains(key)) return key;
  }
  return nullptr;
}

bool is_scalar(const Json& value) {
  return value.is_null() || value.is_boolean() || value.is_number() || value.is_string();
}

bool is_name_text(const Json& value, std::size_t most_bytes) {
  return value.is_string() && !value.get_ref<const std::string&>().empty() &&
         value.get_ref<const std::string&>().size() <= most_bytes;
}

// What a variable may hold: nil, true, false, a number or a short text.
bool is_variable_value(const Json& value) {
  if (value.is_string()) return value.get_ref<const std::string&>().size() <= kMaxVariableTextBytes;
  return is_scalar(value);
}

// Looks a dialogue over, collecting what it finds.
class Reader {
 public:
  explicit Reader(DialogueFile& file) : file_(file) {}

  void read_root(const Json& root) {
    if (!root.is_object()) return error("", "a dialogue file is a JSON object with \"start\" and \"nodes\"");
    only(root, "", {kStart, kNodes});
    const Json* nodes = root.contains(kNodes) ? &root.at(kNodes) : nullptr;
    if (nodes == nullptr || !nodes->is_object() || nodes->empty()) {
      return error("", "\"nodes\" must be an object of the dialogue's nodes, by id");
    }
    collect_ids(*nodes);
    if (!root.contains(kStart))
      error("", "\"start\" must say where the conversation starts");
    else
      read_next(root.at(kStart), kStart);
    for (const auto& [id, node] : nodes->items()) read_node(id, node);
    if (file_.ok()) warn_of_unreached(root);
  }

 private:
  void error(const std::string& where, const std::string& what) { file_.errors.push_back(said(where, what)); }
  void warning(const std::string& where, const std::string& what) { file_.warnings.push_back(said(where, what)); }
  static std::string said(const std::string& where, const std::string& what) {
    return where.empty() ? what : where + ": " + what;
  }

  // Every key of an object is one of these (a misspelt one would be passed over without a word).
  void only(const Json& object, const std::string& where, Keys keys) {
    for (const auto& item : object.items()) {
      const std::string& key = item.key();
      const bool known = std::any_of(keys.begin(), keys.end(), [&](const char* name) { return key == name; });
      if (!known) error(where, "\"" + key + "\" is not something it can have");
    }
  }

  void collect_ids(const Json& nodes) {
    for (const auto& [id, node] : nodes.items()) {
      if (id.empty() || id.size() > kMaxNodeIdBytes) {
        error("node \"" + id + "\"", "an id is 1 to " + std::to_string(kMaxNodeIdBytes) + " bytes");
      }
      ids_.insert(id);
    }
  }

  void read_node(const std::string& id, const Json& node) {
    const std::string where = "node " + id;
    if (!node.is_object()) return error(where, "a node is an object with a \"text\"");
    only(node, where, {kSpeaker, kText, kVoice, kEffects, kNext, kChoices, kEnd});
    read_text(node, kText, where, true);
    read_text(node, kSpeaker, where, false);
    read_voice(node, where);
    if (node.contains(kEffects)) read_effects(node.at(kEffects), where);
    read_ways_on(node, where, true);
    if (node.contains(kChoices)) read_choices(node.at(kChoices), where);
  }

  // A text it shows: an I18n key. needed: it must have one.
  void read_text(const Json& object, const char* key, const std::string& where, bool needed) {
    if (!object.contains(key)) {
      if (needed) error(where, std::string("it has no \"") + key + "\" (an I18n key)");
      return;
    }
    const Json& text = object.at(key);
    if (!is_name_text(text, std::string::npos)) return error(where, std::string("\"") + key + "\" is an I18n key");
    file_.texts.push_back({text.get<std::string>(), where});
  }

  void read_voice(const Json& node, const std::string& where) {
    if (!node.contains(kVoice)) return;
    const Json& voice = node.at(kVoice);
    if (!voice.is_string() || !safe_asset_name(voice.get_ref<const std::string&>())) {
      error(where, "\"voice\" is a sound's name inside the game's sounds folder");
    }
  }

  // How it goes on: "next", "end": true, or (a node) "choices"; one of them.
  void read_ways_on(const Json& object, const std::string& where, bool may_choose) {
    if (object.contains(kEnd) && object.at(kEnd) != true) error(where, "\"end\" can only be true");
    const std::size_t ways = count_of(object, std::array{kNext, kEnd}) + (may_choose && object.contains(kChoices));
    const char* options = may_choose ? "\"next\", \"choices\" or \"end\": true" : "\"next\" or \"end\": true";
    if (ways < kOneWay) error(where, std::string("it leads nowhere: give it ") + options);
    if (ways > kOneWay) error(where, std::string("it goes on in more than one way: give it one of ") + options);
    if (object.contains(kNext)) read_next(object.at(kNext), where);
  }

  void read_choices(const Json& choices, const std::string& where) {
    if (!choices.is_array() || choices.empty()) return error(where, "\"choices\" is a list of at least one choice");
    for (std::size_t index = 0; index < choices.size(); ++index) {
      read_choice(choices.at(index), where + " choice " + std::to_string(index + 1));
    }
    const bool all_conditional = std::all_of(
        choices.begin(), choices.end(), [](const Json& choice) { return choice.is_object() && choice.contains(kIf); });
    if (all_conditional) warning(where, "every choice has a condition: when none holds, the conversation ends there");
  }

  void read_choice(const Json& choice, const std::string& where) {
    if (!choice.is_object()) return error(where, "a choice is an object with a \"text\"");
    only(choice, where, {kText, kIf, kEffects, kNext, kEnd});
    read_text(choice, kText, where, true);
    if (choice.contains(kIf)) read_conditions(choice.at(kIf), where);
    if (choice.contains(kEffects)) read_effects(choice.at(kEffects), where);
    read_ways_on(choice, where, false);
  }

  // A node's id, or branches: [{ "if": [...], "next": "id" }, ...].
  void read_next(const Json& next, const std::string& where) {
    if (next.is_string()) return read_node_id(next.get<std::string>(), where);
    if (!next.is_array() || next.empty()) return error(where, "\"next\" is a node's id, or a list of branches");
    for (std::size_t index = 0; index < next.size(); ++index) {
      read_branch(next.at(index), where + " branch " + std::to_string(index + 1));
    }
    if (next.back().is_object() && next.back().contains(kIf)) {
      warning(where, "every branch has a condition: when none holds, the conversation ends there");
    }
  }

  void read_branch(const Json& branch, const std::string& where) {
    if (!branch.is_object() || !branch.contains(kNext) || !branch.at(kNext).is_string()) {
      return error(where, "a branch is an object with a \"next\" node (and maybe an \"if\")");
    }
    only(branch, where, {kIf, kNext});
    if (branch.contains(kIf)) read_conditions(branch.at(kIf), where);
    read_node_id(branch.at(kNext).get<std::string>(), where);
  }

  void read_node_id(const std::string& id, const std::string& where) {
    if (ids_.count(id) == 0) error(where, "there is no node \"" + id + "\"");
  }

  void read_conditions(const Json& conditions, const std::string& where) {
    if (!conditions.is_array()) return error(where, "\"if\" is a list of conditions");
    for (const Json& condition : conditions) read_condition(condition, where);
  }

  void read_condition(const Json& condition, const std::string& where) {
    if (!condition.is_object()) return error(where, "a condition is an object such as {\"var\": \"gold\", \"is\": 3}");
    only(condition, where, {kVar, kQuest, "is", "not", "at_least", "at_most"});
    if (count_of(condition, kSubjects) != kOneWay)
      return error(where, "a condition reads one \"var\" or one \"quest\"");
    if (count_of(condition, kComparisons) != kOneWay) {
      return error(where, "a condition has one of \"is\", \"not\", \"at_least\" and \"at_most\"");
    }
    read_subject(condition, first_of(condition, kSubjects), where);
    read_comparison(condition, first_of(condition, kComparisons), where);
    if (condition.contains(kQuest)) read_stage_compared(condition, where);
  }

  // A quest's stage compared by "is" or "not" (a text: null is a quest not started).
  void read_stage_compared(const Json& condition, const std::string& where) {
    const char* comparison = first_of(condition, kStageComparisons);
    if (comparison == nullptr || !condition.at(comparison).is_string()) return;
    add_stage(condition, condition.at(comparison), where);
  }

  void add_stage(const Json& object, const Json& stage, const std::string& where) {
    if (!object.at(kQuest).is_string()) return;
    file_.stages.push_back({object.at(kQuest).get<std::string>(), stage.get<std::string>(), where});
  }

  // The variable or the quest a condition or an effect names.
  void read_subject(const Json& object, const char* key, const std::string& where) {
    const bool quest = std::string(key) == kQuest;
    if (!is_name_text(object.at(key), kMaxVariableNameBytes)) {
      return error(
          where, std::string("\"") + key + "\" is a name of 1 to " + std::to_string(kMaxVariableNameBytes) + " bytes");
    }
    if (quest) file_.quests.push_back({object.at(key).get<std::string>(), where});
  }

  void read_comparison(const Json& condition, const char* comparison, const std::string& where) {
    const Json& wanted = condition.at(comparison);
    const bool by_number = std::any_of(kNumberComparisons.begin(), kNumberComparisons.end(),
                                       [&](const char* name) { return std::string(name) == comparison; });
    if (by_number && !wanted.is_number()) return error(where, std::string("\"") + comparison + "\" is a number");
    if (!is_variable_value(wanted))
      error(where, std::string("\"") + comparison + "\" is null, true, false, a number or a text");
  }

  void read_effects(const Json& effects, const std::string& where) {
    if (!effects.is_array()) return error(where, "\"effects\" is a list of effects");
    for (const Json& effect : effects) read_effect(effect, where);
  }

  void read_effect(const Json& effect, const std::string& where) {
    if (!effect.is_object() || count_of(effect, kEffectKinds) != kOneWay) {
      return error(where, "an effect is one of \"set\", \"add\", \"quest\" and \"event\"");
    }
    const char* kind = first_of(effect, kEffectKinds);
    const EffectShape& shape = *std::find_if(kEffectShapes.begin(), kEffectShapes.end(),
                                             [&](const EffectShape& one) { return std::string(one.kind) == kind; });
    only(effect, where, {shape.kind, shape.value});
    read_subject(effect, kind, where);
    if (shape.needed && !effect.contains(shape.value)) {
      return error(where, std::string("a \"") + kind + "\" effect needs \"" + shape.value + "\"");
    }
    read_effect_value(effect, kind, where);
  }

  void read_effect_value(const Json& effect, const std::string& kind, const std::string& where) {
    if (kind == "add" && !effect.at(kBy).is_number()) return error(where, "\"by\" is a number");
    if (kind == kQuest && !is_name_text(effect.at(kTo), kMaxVariableNameBytes)) {
      return error(where, "a quest goes \"to\" a stage: a name");
    }
    if (kind == kQuest) add_stage(effect, effect.at(kTo), where);
    if (kind == "set" && !is_variable_value(effect.at(kTo))) {
      error(where, "\"to\" is null, true, false, a number or a text");
    }
  }

  // The nodes no way from the start reaches (read once the file is well made).
  void warn_of_unreached(const Json& root) {
    std::set<std::string> reached;
    std::vector<std::string> waiting = targets_of(root.at(kStart));
    while (!waiting.empty()) {
      const std::string id = waiting.back();
      waiting.pop_back();
      if (!reached.insert(id).second) continue;
      for (const std::string& next : ways_from(root.at(kNodes).at(id))) waiting.push_back(next);
    }
    for (const std::string& id : ids_) {
      if (reached.count(id) == 0) warning("node " + id, "nothing leads to it");
    }
  }

  static std::vector<std::string> targets_of(const Json& next) {
    if (next.is_string()) return {next.get<std::string>()};
    std::vector<std::string> targets;
    for (const Json& branch : next) targets.push_back(branch.at(kNext).get<std::string>());
    return targets;
  }

  static std::vector<std::string> ways_from(const Json& node) {
    std::vector<std::string> ways = node.contains(kNext) ? targets_of(node.at(kNext)) : std::vector<std::string>{};
    if (!node.contains(kChoices)) return ways;
    for (const Json& choice : node.at(kChoices)) {
      if (!choice.contains(kNext)) continue;
      for (const std::string& target : targets_of(choice.at(kNext))) ways.push_back(target);
    }
    return ways;
  }

  DialogueFile& file_;
  std::set<std::string> ids_;
};

}  // namespace

DialogueFile read_dialogue(std::string_view text) {
  DialogueFile file;
  file.json = game_json::parsed(text);
  if (file.json.is_discarded()) {
    file.errors.push_back("it is not JSON (or it nests deeper than " + std::to_string(game_json::kMaxDepth) +
                          " levels)");
    return file;
  }
  Reader(file).read_root(file.json);
  return file;
}

bool is_dialogue_name(std::string_view name) { return name.size() <= kMaxNameBytes && safe_asset_name(name); }

fs::path dialogue_file(const fs::path& game_dir, std::string_view name) {
  return game_dir / config::kDialoguesDirName / (std::string(name) + kFileExtension);
}

std::string shown_file(std::string_view name) {
  return std::string(config::kDialoguesDirName) + "/" + std::string(name) + kFileExtension;
}

std::vector<std::string> dialogue_names(const fs::path& game_dir) {
  return game_json::names_in(game_dir, config::kDialoguesDirName);
}

}  // namespace redgem::dialogue
