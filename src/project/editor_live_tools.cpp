#include "project/editor_live_tools.hpp"

#include <algorithm>
#include <stdexcept>

#include "core/safe_path.hpp"
#include "core/config.hpp"
#include "project/mcp_tools.hpp"
#include "project/scene_assignments.hpp"
#include "project/scene_diff.hpp"
#include "project/scene_lookup.hpp"
#include "project/scene_text.hpp"

namespace redgem::tools {
namespace {
using namespace mcp;
constexpr std::size_t kLiveListLimit = 50;

Json state(EditorSession& session) {
  Json selected = Json::array();
  for (const auto id : session.selected().ids())
    selected.push_back(object_id(session.scene_desc(), static_cast<int>(id)));
  const char* mode = session.editing() ? "editing" : session.play_state() == PlayState::Paused ? "paused" : "playing";
  return {{"scene", session.scene_name()},
          {"mode", mode},
          {"dirty", session.document().dirty()},
          {"selection", selected},
          {"can_undo", session.document().can_undo()},
          {"can_redo", session.document().can_redo()},
          {"external_change_pending", session.external_change_pending()}};
}

int object(EditorSession& session, const Json& args) {
  const FoundObject found = find_object(session.scene_desc(), args.at("object").get<std::string>());
  if (found.index == kNoIndex) throw std::invalid_argument(found.error);
  return found.index;
}

void require_current_scene(const EditorSession& session, const Json& args) {
  if (args.contains("scene") && args["scene"] != session.scene_name())
    throw std::invalid_argument("the requested scene is not the editor's open scene");
}

std::string apply_words(SceneEdit& edit, int& index, const Json& args) {
  for (const auto& word : args.value("assignments", std::vector<std::string>{})) {
    const auto error = apply_assignment(edit, index, word);
    if (!error.empty()) return error;
  }
  return {};
}

void require_editable(const EditorSession& session, bool busy) {
  if (!session.editing() || busy)
    throw std::invalid_argument("editor is busy; stop playing or finish the current gesture");
  if (session.external_change_pending())
    throw std::invalid_argument("resolve the editor's external file conflict first");
}

Json edit(EditorSession& session, const Json& args, bool busy) {
  require_editable(session, busy);
  if (args.at("action") != "set") throw std::invalid_argument("live edits currently support existing-object set only");
  if (args.contains("from") || args.contains("parent"))
    throw std::invalid_argument("from and parent are not set options");
  int index = object(session, args);
  SceneEdit edit{session.document(), session.game_dir() / config::kScriptsDirName, {}};
  std::string error;
  if (!session.document().edit_existing_nodes([&] { return apply_words(edit, index, args); }, error))
    throw std::invalid_argument(error);
  session.refresh_preview();
  session.select(static_cast<ObjectId>(index));
  return tool_result(object_line(session.scene_desc(), index) + "\n");
}

void apply_control(EditorSession& session, const std::string& action, const Json& args) {
  if (action == "select") session.select(static_cast<ObjectId>(object(session, args)));
  if (action == "undo") session.undo();
  if (action == "redo") session.redo();
  if (action == "save") {
    std::string error;
    if (!session.save(error)) throw std::runtime_error(error);
  }
}

Json control(EditorSession& session, const Json& args, bool busy) {
  const std::string action = args.at("action");
  if (action != "select" && args.contains("object")) throw std::invalid_argument("object is only accepted by select");
  if (action == "select" && !args.contains("object")) throw std::invalid_argument("select needs an object");
  if (action == "status") return tool_result(state(session).dump());
  require_editable(session, busy);
  apply_control(session, action, args);
  return tool_result(state(session).dump());
}

Json compare(EditorSession& session, const Json& args) {
  const auto before = file_inside(session.game_dir(), args.at("against").get<std::string>());
  if (!before) throw std::invalid_argument("against must name a regular file inside the game");
  const auto result =
      compare_scene_file(session.scene_desc(), session.scene_name(), *before, args.value("limit", kDefaultDiffLines));
  return tool_result(result.out + result.err, result.exit_code != kExitSuccess);
}

void validate(const EditorSession& session, const std::string& name, const Json& args) {
  Options options;
  options.game = session.game_dir();
  options.editor_connection = "live";  // exposes the editor schema without opening a connection
  const Json tools = catalog(options);
  const auto tool = std::find_if(tools.begin(), tools.end(), [&](const Json& value) { return value["name"] == name; });
  if (tool == tools.end()) throw std::invalid_argument("unknown editor tool");
  validate_arguments(tool->at("inputSchema"), args);
  require_current_scene(session, args);
}

Json handle(EditorSession& session, const std::string& name, const Json& args, bool busy) {
  if (name == "scene_get") return tool_result(object_detail(session.scene_desc(), object(session, args)));
  if (name == "scene_edit") return edit(session, args, busy);
  if (name == "scene_diff") return compare(session, args);
  if (name == "editor") return control(session, args, busy);
  if (name != "scene_list") throw std::invalid_argument("tool is not available in the editor");
  return tool_result(scene_list(session.scene_desc(), session.scene_name(),
                                {args.value("depth", kEveryLevel), args.value("limit", kLiveListLimit)}));
}
}  // namespace

Json invoke_editor(EditorSession& session, const std::string& name, const Json& args, bool busy) {
  try {
    validate(session, name, args);
    if (name == "events") return mcp::invoke({}, name, args);
    return handle(session, name, args, busy);
  } catch (const std::exception& error) {
    return tool_result(error.what(), true);
  }
}
}  // namespace redgem::tools
