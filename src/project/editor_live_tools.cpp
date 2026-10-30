#include "project/editor_live_tools.hpp"

#include <algorithm>
#include <stdexcept>

#include "core/safe_path.hpp"
#include "core/config.hpp"
#include "project/mcp_tools.hpp"
#include "project/scene_assignments.hpp"
#include "project/scene_diff.hpp"
#include "project/scene_lookup.hpp"
#include "project/runtime_query.hpp"
#include "project/scene_text.hpp"

namespace redgem::tools {
namespace {
using namespace mcp;

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

void validate_removal(const Json& args, const std::vector<std::string>& words) {
  if (!args.value("confirm", false)) throw std::invalid_argument("remove needs confirm=true");
  if (!words.empty()) throw std::invalid_argument("remove does not accept assignments");
}

void validate_edit(const Json& args) {
  const std::string action = args.at("action");
  if (args.contains("from")) throw std::invalid_argument("live add creates an empty object; from is not supported");
  if (action != "add" && args.contains("parent")) throw std::invalid_argument("parent is only accepted by add");
  const auto words = args.value("assignments", std::vector<std::string>{});
  if (action == "set" && words.empty()) throw std::invalid_argument("set needs assignments");
  if (action == "remove") validate_removal(args, words);
}

int add_empty(EditorSession& session, const Json& args) {
  const std::string name = args.at("object");
  const auto problem = name_problem(name);
  if (!problem.empty()) throw std::invalid_argument(problem);
  int parent = kNoIndex;
  if (args.contains("parent")) parent = object(session, {{"object", args["parent"]}});
  return session.document().add_empty(name, parent);
}

std::string apply_edit(EditorSession& session, SceneEdit& edit, int& index, const Json& args) {
  const std::string action = args.at("action");
  if (action == "add") index = add_empty(session, args);
  if (action == "remove") {
    session.document().remove(index);
    index = kNoIndex;
    return {};
  }
  return apply_words(edit, index, args);
}

Json edit(EditorSession& session, const Json& args, bool busy) {
  require_editable(session, busy);
  validate_edit(args);
  int index = args.at("action") == "add" ? kNoIndex : object(session, args);
  const std::string removed = index == kNoIndex ? "" : object_line(session.scene_desc(), index);
  SceneEdit edit{session.document(), session.game_dir() / config::kScriptsDirName, {}};
  std::string error;
  if (!session.document().edit_nodes([&] { return apply_edit(session, edit, index, args); }, error))
    throw std::invalid_argument(error);
  session.refresh_preview();
  session.select(index == kNoIndex ? kNoObject : static_cast<ObjectId>(index));
  return tool_result(index == kNoIndex ? "removed " + removed : object_line(session.scene_desc(), index));
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

bool window_action(const std::string& action) {
  return action == "play" || action == "pause" || action == "resume" || action == "stop" || action == "step" ||
         action == "capture" || action == "frame";
}

// The actions about an object: select needs one, frame takes one (else the selection).
bool takes_object(const std::string& action) { return action == "select" || action == "frame"; }

void validate_control(const std::string& action, const Json& args) {
  if (!takes_object(action) && args.contains("object"))
    throw std::invalid_argument("object is only accepted by select and frame");
  if (action == "select" && !args.contains("object")) throw std::invalid_argument("select needs an object");
  if (action != "capture" && args.contains("max_size"))
    throw std::invalid_argument("max_size is only accepted by capture");
}

Json use_window(EditorSession& session, const std::string& action, const Json& args, bool busy, const Invoke& window) {
  if (!window) throw std::invalid_argument("this action requires an editor window");
  if (action != "capture" && busy) throw std::invalid_argument("finish the current editor gesture or dialog first");
  if (action == "play") require_editable(session, false);
  if (args.contains("object")) session.select(static_cast<ObjectId>(object(session, args)));
  return window(action, args);
}

Json control(EditorSession& session, const Json& args, bool busy, const Invoke& window) {
  const std::string action = args.at("action");
  validate_control(action, args);
  if (action == "status") return tool_result(state(session).dump());
  if (window_action(action)) return use_window(session, action, args, busy, window);
  require_editable(session, busy);
  apply_control(session, action, args);
  return tool_result(state(session).dump());
}

// The objects of the scene the runtime plays, as its ids know them: the edited document's, unless a
// script moved the game on to another scene (whose objects are then named by name alone).
const SceneDesc& runtime_scene(EditorSession& session, Engine& runtime) {
  static const SceneDesc kNoNodes;
  return runtime.scene_name() == session.scene_name() ? session.scene_desc() : kNoNodes;
}

Json query(EditorSession& session, const Json& args) {
  Engine* runtime = session.runtime();
  if (session.editing() || runtime == nullptr)
    throw std::invalid_argument("the editor is not playing; start the game with editor action=play first");
  if (args.contains("frames"))
    throw std::invalid_argument("frames is for headless queries; pause and use editor action=step to advance");
  return tool_result(answer_query(*runtime, runtime_scene(session, *runtime), args));
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

// Events of the editor's own process, with paths shown relative to its game.
Options events_options(const EditorSession& session) {
  Options options;
  options.game = session.game_dir();
  return options;
}

Json handle(EditorSession& session, const std::string& name, const Json& args, bool busy, const Invoke& window) {
  if (name == "scene_get") return tool_result(object_detail(session.scene_desc(), object(session, args)));
  if (name == "scene_edit") return edit(session, args, busy);
  if (name == "scene_diff") return compare(session, args);
  if (name == "game_query") return query(session, args);
  if (name == "editor") return control(session, args, busy, window);
  if (name != "scene_list") throw std::invalid_argument("tool is not available in the editor");
  return tool_result(scene_list(session.scene_desc(), session.scene_name(),
                                {args.value("depth", kEveryLevel), args.value("limit", kDefaultListLimit)}));
}
}  // namespace

Json invoke_editor(EditorSession& session, const std::string& name, const Json& args, bool busy, const Invoke& window) {
  try {
    validate(session, name, args);
    if (name == "events") return mcp::invoke(events_options(session), name, args);
    return handle(session, name, args, busy, window);
  } catch (const std::exception& error) {
    return tool_result(error.what(), true);
  }
}
}  // namespace redgem::tools
