#include "project/editor_live_tools.hpp"

#include <algorithm>
#include <stdexcept>

#include "core/safe_path.hpp"
#include "core/config.hpp"
#include "project/mcp_import_settings_tool.hpp"
#include "project/mcp_tools.hpp"
#include "project/scene_assignments.hpp"
#include "project/scene_command.hpp"
#include "project/scene_diff.hpp"
#include "project/prefab_edits.hpp"
#include "project/scene_edits.hpp"
#include "project/scene_revision.hpp"
#include "project/scene_lookup.hpp"
#include "project/runtime_query.hpp"
#include "project/scene_text.hpp"
#include "scene/property.hpp"
#include "scene/props.hpp"

namespace redgem::tools {
namespace {
using namespace mcp;

constexpr const char* kRecoveryPending =
    "the editor is busy, and offers to recover unsaved edits of an earlier session: answer it in "
    "the editor (recover them, or discard them), then try again";
constexpr const char* kExternalChange =
    "the scene changed on disk while the editor has unsaved edits; resolve it in the editor "
    "(reload the file, or keep the edits) first";

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
          {"external_change_pending", session.external_change_pending()},
          {"recovery_pending", session.recovery_pending()}};
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

// The scene changed on disk while the editor had unsaved edits: the editor asks its user which to
// keep, and that question (a dialog, which makes the editor busy too) is what holds every change.
void require_no_external_change(const EditorSession& session) {
  if (session.external_change_pending()) throw std::invalid_argument(kExternalChange);
}

// The editor is busy: when it found the copy of an earlier session's unsaved edits (it closed
// without saving them), its offer to recover them is the likely reason, and is named.
void require_no_recovery_offer(const EditorSession& session, bool busy) {
  if (busy && session.recovery_pending()) throw std::invalid_argument(kRecoveryPending);
}

void require_editable(const EditorSession& session, bool busy) {
  require_no_external_change(session);
  require_no_recovery_offer(session, busy);
  if (!session.editing() || busy)
    throw std::invalid_argument("editor is busy; stop playing or finish the current gesture");
}

// Selects what an edit leaves: the objects it made or changed, none after a removal.
void select_outcome(EditorSession& session, const EditOutcome& outcome) {
  std::vector<ObjectId> ids;
  ids.reserve(outcome.objects.size());
  for (const int index : outcome.objects) ids.push_back(static_cast<ObjectId>(index));
  if (ids.empty()) {
    session.select(kNoObject);
    return;
  }
  session.select_all(ids);
}

Json edit(EditorSession& session, const Json& args, bool busy) {
  require_editable(session, busy);
  SceneEdit edit{session.document(), session.game_dir() / config::kScriptsDirName, {}};
  const EditOutcome outcome = edit_scene(edit, args);  // one undo step, or none when it throws
  session.refresh_preview();
  select_outcome(session, outcome);
  return tool_result(outcome.text);
}

// scene_prefab in the open scene: each change one undo step, its placement (or what unpack made)
// selected.
Json prefab(EditorSession& session, const Json& args, bool busy) {
  if (reads_prefab(args.at("action")))
    return tool_result(edit_prefab(session.document(), session.game_dir(), args).text);
  require_editable(session, busy);
  const PrefabOutcome outcome = edit_prefab(session.document(), session.game_dir(), args);
  session.refresh_preview();
  session.select(static_cast<ObjectId>(outcome.object));
  return tool_result(outcome.text);
}

// What apply says: how many objects took the new settings, or why none did.
std::string reimported_text(const std::string& model, int done) {
  if (done == 0)
    return "no object of the open scene came from models/" + model +
           " (only objects the editor imported carry its model_file property)";
  return "imported models/" + model + " again into " + std::to_string(done) +
         " object(s), each one undo step; unsaved until editor save";
}

// Whether an object of the open scene was imported from models/<model> (its model_file).
bool holds_model(EditorSession& session, const std::string& model) {
  const SceneDocument& document = session.document();
  for (int index = 0; index < document.node_count(); ++index)
    if (get_string(document.node(index).props, props::kModelFile) == model) return true;
  return false;
}

// import_settings in the editor: settings and set on the files, as without it; apply imports the
// model again wherever the open scene holds it (the objects keep their names, places and scripts).
Json reimport(EditorSession& session, const Json& args, bool busy) {
  if (args.at("action") != "apply") return import_settings_files(session.game_dir(), args);
  require_editable(session, busy);
  const std::string model = model_to_reimport(session.game_dir(), args);
  // (Nothing to change: the re-import would still extract pictures and drop the selection.)
  if (!holds_model(session, model)) return tool_result(reimported_text(model, 0));
  std::string error;
  const int done = session.reimport_model(model, error);
  if (error.empty()) return tool_result(reimported_text(model, done));
  return tool_result(done == 0 ? error : reimported_text(model, done) + ", then it stopped: " + error, true);
}

// The objects select names: `object`, and those of `objects`.
std::vector<ObjectId> selected_by(EditorSession& session, const Json& args) {
  std::vector<ObjectId> ids;
  if (args.contains("object")) ids.push_back(static_cast<ObjectId>(object(session, args)));
  for (const auto& reference : args.value("objects", std::vector<std::string>{}))
    ids.push_back(static_cast<ObjectId>(object(session, {{"object", reference}})));
  return ids;
}

void apply_control(EditorSession& session, const std::string& action, const Json& args) {
  if (action == "select") session.select_all(selected_by(session, args));
  if (action == "undo") session.undo();
  if (action == "redo") session.redo();
  if (action == "save") {
    std::string error;
    if (!session.save(error)) throw std::runtime_error(error);
  }
}

// The actions that put another scene in the editor: open one, or save this one as one.
bool scene_action(const std::string& action) { return action == "open_scene" || action == "save_as"; }

bool window_action(const std::string& action) {
  return action == "play" || action == "pause" || action == "resume" || action == "stop" || action == "step" ||
         action == "capture" || action == "frame" || scene_action(action);
}

// The actions about an object: select needs one, frame takes one (else the selection).
bool takes_object(const std::string& action) { return action == "select" || action == "frame"; }

bool selects_nothing(const std::string& action, const Json& args) {
  return action == "select" && !args.contains("object") && !args.contains("objects");
}

bool misplaced_objects(const std::string& action, const Json& args) {
  return action != "select" && args.contains("objects");
}

bool misplaced_scene(const std::string& action, const Json& args) {
  return scene_action(action) != args.contains("scene");
}

bool misplaced_discard(const std::string& action, const Json& args) {
  return action != "open_scene" && args.contains("discard");
}

void validate_scene_words(const std::string& action, const Json& args) {
  if (misplaced_scene(action, args)) throw std::invalid_argument("open_scene and save_as take scene, and only they do");
  if (misplaced_discard(action, args)) throw std::invalid_argument("discard is only accepted by open_scene");
}

void validate_control(const std::string& action, const Json& args) {
  validate_scene_words(action, args);
  if (!takes_object(action) && args.contains("object"))
    throw std::invalid_argument("object is only accepted by select and frame");
  if (selects_nothing(action, args)) throw std::invalid_argument("select needs object or objects");
  if (misplaced_objects(action, args)) throw std::invalid_argument("objects is only accepted by select");
  if (action != "capture" && args.contains("max_size"))
    throw std::invalid_argument("max_size is only accepted by capture");
}

// Whether opening another scene would drop edits nobody said to drop.
bool drops_unsaved(EditorSession& session, const std::string& action, const Json& args) {
  return action == "open_scene" && session.document().dirty() && !args.value("discard", false);
}

// Whether open_scene names a scene the game has: only those are opened (no path out of the game).
bool names_no_scene(const EditorSession& session, const std::string& action, const Json& args) {
  if (action != "open_scene") return false;
  const auto names = scene_names(session.game_dir());
  return std::find(names.begin(), names.end(), args["scene"].get<std::string>()) == names.end();
}

void require_scene_change(EditorSession& session, const std::string& action, const Json& args) {
  require_editable(session, false);
  if (names_no_scene(session, action, args))
    throw std::invalid_argument("the game has no scene called " + args["scene"].get<std::string>() +
                                "; project action=scenes lists them");
  if (drops_unsaved(session, action, args))
    throw std::invalid_argument("the open scene has unsaved edits: save first, or pass discard=true to drop them");
}

// A capture is taken whatever the editor is doing; the other window actions wait until it is free.
bool refuses_when_busy(const std::string& action, bool busy) { return busy && action != "capture"; }

Json use_window(EditorSession& session, const std::string& action, const Json& args, bool busy, const Invoke& window) {
  if (!window) throw std::invalid_argument("this action requires an editor window");
  if (refuses_when_busy(action, busy)) {
    require_no_external_change(session);  // (the reason the editor is busy, when it is one of these)
    require_no_recovery_offer(session, busy);
    throw std::invalid_argument("finish the current editor gesture or dialog first");
  }
  if (action == "play") require_editable(session, false);
  if (scene_action(action)) require_scene_change(session, action, args);
  if (args.contains("object")) {
    require_editable(session, busy);  // selecting it is a change, refused as select's is
    session.select(static_cast<ObjectId>(object(session, args)));
  }
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
  Engine* runtime = playing_runtime(session);
  if (runtime == nullptr) throw std::invalid_argument(kEditorNotPlaying);
  if (args.contains("frames"))
    throw std::invalid_argument("frames is for headless queries; pause and use editor action=step to advance");
  return tool_result(answer_query(*runtime, runtime_scene(session, *runtime), args));
}

// The open document compared with an earlier save of it inside the game.
ToolResult compared_with_file(EditorSession& session, const Json& args, std::size_t limit) {
  const auto before = file_inside(session.game_dir(), args.at("against").get<std::string>());
  if (!before) throw std::invalid_argument("against must name a regular file inside the game");
  return compare_scene_file(session.scene_desc(), session.scene_name(), *before, limit);
}

// The open document compared with its file as a Git revision holds it.
ToolResult compared_with_revision(EditorSession& session, const Json& args, std::size_t limit) {
  return compare_scene_revision(session.scene_desc(), session.scene_name(), session.document().path(),
                                args.at("revision").get<std::string>(), limit);
}

Json compare(EditorSession& session, const Json& args) {
  check_diff_baseline(args);
  const std::size_t limit = args.value("limit", kDefaultDiffLines);
  const auto result = args.contains("revision") ? compared_with_revision(session, args, limit)
                                                : compared_with_file(session, args, limit);
  return tool_result(result.out + result.err, result.exit_code != kExitSuccess);
}

void validate(const EditorSession& session, const std::string& name, const Json& args) {
  Options options;
  options.game = session.game_dir();
  // Exposes the editor schema without opening a connection, keeping from so that a live add with
  // it gets the reason it is refused (see validate_edit).
  options.editor_connection = kDiscoverEditor;
  const Json tools = catalog(options);
  const auto tool = std::find_if(tools.begin(), tools.end(), [&](const Json& value) { return value["name"] == name; });
  if (tool == tools.end()) throw std::invalid_argument("unknown editor tool");
  validate_arguments(tool->at("inputSchema"), args);
  if (name != "editor") require_current_scene(session, args);  // (the editor's scene is one to open or save as)
}

// Events of the editor's own process, with paths shown relative to its game.
Options events_options(const EditorSession& session) {
  Options options;
  options.game = session.game_dir();
  return options;
}

Json list(EditorSession& session, const Json& args) {
  return tool_result(scene_list(session.scene_desc(), session.scene_name(),
                                {args.value("depth", kEveryLevel), args.value("limit", kDefaultListLimit)}));
}

// The tools that only read the document or the running game.
Json read(EditorSession& session, const std::string& name, const Json& args) {
  if (name == "scene_get") return tool_result(object_detail(session.scene_desc(), object(session, args)));
  if (name == "scene_diff") return compare(session, args);
  if (name == "game_query") return query(session, args);
  if (name != "scene_list") throw std::invalid_argument("tool is not available in the editor");
  return list(session, args);
}

Json handle(EditorSession& session, const std::string& name, const Json& args, bool busy, const Invoke& window) {
  if (name == "scene_edit") return edit(session, args, busy);
  if (name == "scene_prefab") return prefab(session, args, busy);
  if (name == "editor") return control(session, args, busy, window);
  if (name == "import_settings") return reimport(session, args, busy);
  return read(session, name, args);
}
}  // namespace

Engine* playing_runtime(EditorSession& session) { return session.editing() ? nullptr : session.runtime(); }

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
