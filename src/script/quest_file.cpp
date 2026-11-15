// Reading and looking over a game's quest files (see quest_file.hpp).

#include "script/quest_file.hpp"

#include <algorithm>
#include <array>

#include "core/config.hpp"
#include "core/safe_path.hpp"
#include "script/game_json_files.hpp"

namespace redgem::quest {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;
using Keys = std::vector<const char*>;

constexpr const char* kTitle = "title";
constexpr const char* kDescription = "description";
constexpr const char* kStart = "start";
constexpr const char* kStages = "stages";
constexpr const char* kText = "text";
constexpr const char* kObjectives = "objectives";
constexpr const char* kNext = "next";
constexpr const char* kEnd = "end";
constexpr const char* kCount = "count";
constexpr const char* kRadius = "radius";
constexpr const char* kTo = "to";
constexpr const char* kDialogue = "dialogue";
constexpr const char* kNode = "node";
constexpr const char* kGoTo = "go_to";
constexpr const char* kTalkTo = "talk_to";
constexpr const char* kDeliver = "deliver";
constexpr const char* kCollect = "collect";
constexpr const char* kDefeat = "defeat";
constexpr const char* kCheck = "check";
constexpr std::array<const char*, 6> kKinds = {kGoTo, kTalkTo, kDeliver, kCollect, kDefeat, kCheck};
constexpr std::array<const char*, 2> kItemKinds = {kDeliver, kCollect};
constexpr std::size_t kOneKind = 1;  // an objective is of exactly one kind

bool is(const char* one, const char* other) { return std::string_view(one) == other; }

template <typename List>
const char* first_of(const Json& json, const List& keys) {
  for (const char* key : keys) {
    if (json.contains(key)) return key;
  }
  return nullptr;
}

template <typename List>
std::size_t count_of(const Json& json, const List& keys) {
  return static_cast<std::size_t>(
      std::count_if(keys.begin(), keys.end(), [&](const char* key) { return json.contains(key); }));
}

bool is_name_text(const Json& value, std::size_t most_bytes) {
  return value.is_string() && !value.get_ref<const std::string&>().empty() &&
         value.get_ref<const std::string&>().size() <= most_bytes;
}

// What an objective of a kind may have besides its kind and its text.
Keys extra_keys_of(const char* kind) {
  if (is(kind, kGoTo)) return {kRadius};
  if (is(kind, kTalkTo)) return {kDialogue, kNode};
  if (is(kind, kDeliver)) return {kTo, kCount};
  if (is(kind, kCollect) || is(kind, kDefeat)) return {kCount};
  return {};
}

bool counts(const char* kind) {
  const Keys extra = extra_keys_of(kind);
  return std::any_of(extra.begin(), extra.end(), [](const char* key) { return is(key, kCount); });
}

bool names_item(const char* kind) {
  return std::any_of(kItemKinds.begin(), kItemKinds.end(), [&](const char* item_kind) { return is(kind, item_kind); });
}

bool is_count(const Json& count) {
  return count.is_number_integer() && count.get<long long>() >= 1 && count.get<long long>() <= kMaxCount;
}

bool is_radius(const Json& radius) { return radius.is_number() && radius.get<double>() > 0.0; }

// Looks a quest over, collecting what it finds.
class Reader {
 public:
  explicit Reader(QuestFile& file) : file_(file) {}

  void read_root(const Json& root) {
    if (!root.is_object()) return error("", "a quest file is a JSON object with \"title\", \"start\" and \"stages\"");
    only(root, "", {kTitle, kDescription, kStart, kStages});
    read_text(root, kTitle, "", true);
    read_text(root, kDescription, "", false);
    const Json* stages = root.contains(kStages) ? &root.at(kStages) : nullptr;
    if (stages == nullptr || !stages->is_object() || stages->empty()) {
      return error("", "\"stages\" must be an object of the quest's stages, by id");
    }
    collect_stages(*stages);
    read_start(root);
    for (const auto& [id, stage] : stages->items()) read_stage(id, stage);
    if (file_.ok() && !any_end(*stages)) warning("", "no stage has \"end\": true, so the quest never completes");
  }

 private:
  void error(const std::string& where, const std::string& what) { file_.errors.push_back(said(where, what)); }
  void warning(const std::string& where, const std::string& what) { file_.warnings.push_back(said(where, what)); }
  static std::string said(const std::string& where, const std::string& what) {
    return where.empty() ? what : where + ": " + what;
  }

  // Every key of an object is one of these (a misspelt one would be passed over without a word).
  void only(const Json& object, const std::string& where, const Keys& keys) {
    for (const auto& item : object.items()) {
      const std::string& key = item.key();
      const bool known = std::any_of(keys.begin(), keys.end(), [&](const char* name) { return key == name; });
      if (!known) error(where, "\"" + key + "\" is not something it can have");
    }
  }

  // A text it shows: an I18n key. needed: it must have one.
  void read_text(const Json& object, const char* key, const std::string& where, bool needed) {
    if (!object.contains(key)) {
      if (needed) error(where, std::string("it has no \"") + key + "\" (an I18n key)");
      return;
    }
    if (!is_name_text(object.at(key), std::string::npos))
      return error(where, std::string("\"") + key + "\" is an I18n key");
    file_.texts.push_back({object.at(key).get<std::string>(), where.empty() ? key : where});
  }

  void collect_stages(const Json& stages) {
    for (const auto& [id, stage] : stages.items()) {
      if (id.empty() || id.size() > kMaxStageBytes) {
        error("stage \"" + id + "\"", "an id is 1 to " + std::to_string(kMaxStageBytes) + " bytes");
      }
      file_.stages.insert(id);
    }
  }

  void read_start(const Json& root) {
    if (!root.contains(kStart) || !root.at(kStart).is_string()) {
      return error("", "\"start\" must name the stage the quest starts at");
    }
    read_stage_id(root.at(kStart).get<std::string>(), kStart);
  }

  void read_stage_id(const std::string& id, const std::string& where) {
    if (file_.stages.count(id) == 0) error(where, "there is no stage \"" + id + "\"");
  }

  void read_stage(const std::string& id, const Json& stage) {
    const std::string where = "stage " + id;
    if (!stage.is_object()) return error(where, "a stage is an object with a \"text\"");
    only(stage, where, {kText, kObjectives, kNext, kEnd});
    read_text(stage, kText, where, true);
    const bool has_objectives = stage.contains(kObjectives);
    if (has_objectives) read_objectives(stage.at(kObjectives), where);
    read_ways_on(stage, has_objectives, where);
  }

  // A stage with objectives goes on to "next" once they are done; one with "end": true completes
  // the quest; one with neither waits for a dialogue or a script to move it on.
  void read_ways_on(const Json& stage, bool has_objectives, const std::string& where) {
    if (stage.contains(kEnd)) return read_end(stage, has_objectives, where);
    if (!stage.contains(kNext)) {
      if (has_objectives) error(where, "once its objectives are done it leads nowhere: give it \"next\"");
      return;
    }
    if (!has_objectives) error(where, "\"next\" is where it goes once its objectives are done, and it has none");
    if (!stage.at(kNext).is_string()) return error(where, "\"next\" is a stage's id");
    read_stage_id(stage.at(kNext).get<std::string>(), where);
  }

  void read_end(const Json& stage, bool has_objectives, const std::string& where) {
    if (stage.at(kEnd) != true) error(where, "\"end\" can only be true");
    if (has_objectives || stage.contains(kNext)) {
      error(where, "a stage that ends the quest has no objectives and no \"next\"");
    }
  }

  void read_objectives(const Json& objectives, const std::string& where) {
    if (!objectives.is_array() || objectives.empty() || objectives.size() > kMaxObjectives) {
      return error(where, "\"objectives\" is a list of 1 to " + std::to_string(kMaxObjectives) + " objectives");
    }
    for (std::size_t index = 0; index < objectives.size(); ++index) {
      read_objective(objectives.at(index), where + " objective " + std::to_string(index + 1));
    }
  }

  void read_objective(const Json& objective, const std::string& where) {
    if (!objective.is_object() || count_of(objective, kKinds) != kOneKind) {
      return error(where,
                   "an objective is one of \"go_to\", \"talk_to\", \"deliver\", \"collect\", \"defeat\" and "
                   "\"check\"");
    }
    const char* kind = first_of(objective, kKinds);
    Keys keys = extra_keys_of(kind);
    keys.insert(keys.end(), {kind, kText});
    only(objective, where, keys);
    read_text(objective, kText, where, true);
    read_target(objective, kind, where);
    read_details(objective, kind, where);
  }

  // What it names: an object (go_to, talk_to, defeat), an item (deliver, collect) or a check.
  void read_target(const Json& objective, const char* kind, const std::string& where) {
    if (!is_name_text(objective.at(kind), kMaxTargetBytes)) {
      return error(where,
                   std::string("\"") + kind + "\" is a name of 1 to " + std::to_string(kMaxTargetBytes) + " bytes");
    }
    if (names_item(kind)) file_.items.push_back({objective.at(kind).get<std::string>(), where});
  }

  void read_details(const Json& objective, const char* kind, const std::string& where) {
    if (counts(kind) && objective.contains(kCount) && !is_count(objective.at(kCount))) {
      error(where, "\"count\" is a whole number of 1 to " + std::to_string(kMaxCount) + ": it could never be done");
    }
    if (objective.contains(kRadius) && !is_radius(objective.at(kRadius))) {
      error(where, "\"radius\" is a number of meters above 0: it could never be reached");
    }
    if (is(kind, kDeliver)) read_recipient(objective, where);
    if (is(kind, kTalkTo)) read_talk(objective, where);
  }

  void read_recipient(const Json& objective, const std::string& where) {
    if (!objective.contains(kTo) || !is_name_text(objective.at(kTo), kMaxTargetBytes)) {
      error(where, "a \"deliver\" objective says who it goes \"to\": an object's name");
    }
  }

  // The dialogue it must be, and the node it must reach (which needs the dialogue).
  void read_talk(const Json& objective, const std::string& where) {
    const bool has_dialogue = objective.contains(kDialogue);
    if (has_dialogue && !is_name_text(objective.at(kDialogue), dialogue::kMaxNameBytes)) {
      return error(where, "\"dialogue\" is a dialogue's name");
    }
    if (objective.contains(kNode) && !has_dialogue) return error(where, "\"node\" is a node of its \"dialogue\"");
    if (objective.contains(kNode) && !is_name_text(objective.at(kNode), dialogue::kMaxNodeIdBytes)) {
      return error(where, "\"node\" is a node's id");
    }
    if (!has_dialogue) return;
    file_.dialogues.push_back(
        {objective.at(kDialogue).get<std::string>(), objective.value(kNode, std::string()), where});
  }

  static bool any_end(const Json& stages) {
    return std::any_of(stages.begin(), stages.end(),
                       [](const Json& stage) { return stage.is_object() && stage.value(kEnd, Json()) == true; });
  }

  QuestFile& file_;
};

}  // namespace

QuestFile read_quest(std::string_view text) {
  QuestFile file;
  file.json = game_json::parsed(text);
  if (file.json.is_discarded()) {
    file.errors.push_back("it is not JSON (or it nests deeper than " + std::to_string(game_json::kMaxDepth) +
                          " levels)");
    return file;
  }
  Reader(file).read_root(file.json);
  return file;
}

std::optional<QuestFile> read_quest_file(const fs::path& game_dir, std::string_view name) {
  const auto bytes = read_file_up_to(quest_file(game_dir, name), kMaxFileBytes);
  if (!bytes) return std::nullopt;
  return read_quest({reinterpret_cast<const char*>(bytes->data()), bytes->size()});
}

bool is_quest_name(std::string_view name) { return name.size() <= kMaxNameBytes && safe_asset_name(name); }

fs::path quest_file(const fs::path& game_dir, std::string_view name) {
  return game_dir / config::kQuestsDirName / (std::string(name) + game_json::kFileExtension);
}

std::string shown_file(std::string_view name) {
  return std::string(config::kQuestsDirName) + "/" + std::string(name) + game_json::kFileExtension;
}

std::vector<std::string> quest_names(const fs::path& game_dir) {
  return game_json::names_in(game_dir, config::kQuestsDirName);
}

}  // namespace redgem::quest
