#include "project/mcp_tools.hpp"

#include <algorithm>
#include <stdexcept>

#include "project/mcp_project_tools.hpp"
#include "project/ruby_eval.hpp"

namespace redgem::tools::mcp {
namespace {
// A small fixed vocabulary; schemas are also validated before the native command is called.
constexpr const char* kSchemas = R"JSON([
{"name":"scene_list","description":"List scene objects, one line each: #id name, then only what differs from defaults (pos, rot in degrees, scale, scripts, props). limit defaults to 50 (0 = all); depth 0 = top level only. Replies stop at 8 KiB (about 100 lines): for a bigger scene list a level with depth, then scene_get. Pass #ids back unchanged.","inputSchema":{"type":"object","additionalProperties":false,"properties":{"scene":{"type":"string","maxLength":256},"limit":{"type":"integer","minimum":0,"maximum":200},"depth":{"type":"integer","minimum":0,"maximum":64}}}},
{"name":"scene_get","description":"Read one object in full by #id, unique name or hierarchy path (Level/Wall).","inputSchema":{"type":"object","additionalProperties":false,"required":["object"],"properties":{"object":{"type":"string","minLength":1,"maxLength":256},"scene":{"type":"string","maxLength":256}}}},
{"name":"scene_edit","description":"Change the scene's objects; each call is one undo step in a live editor (unsaved until editor save) and is saved at once on files. action: set (assignments on object, or on each of objects), add (a new object named object: empty, a kind, or a copy of from; under parent), duplicate (copies of object or objects, with children, assignments set on each copy), group (a new empty named object, the objects moved under it, keeping where they are), remove (object or objects with their children; confirm=true). assignments are key=value: position=1,2,3 rotation=0,90,0 (degrees) scale=2 or 1,2,1, name=Gate, parent=Level (parent= for the top), script+=Coin script-=Coin, prop.speed=2.5 (prop.speed= removes it). Components are properties too: prop.visible=false hides, prop.tag=coin, prop.prefab=true makes a template, prop.body=static|dynamic|kinematic|character|vehicle, prop.collider=box|sphere|capsule|mesh, prop.trigger=true, prop.mass=2, prop.layer=enemy, prop.cast_shadow=true. Lights: light=sun|point|spot, light_color=#rrggbb or r,g,b, light_intensity (W, sun W/m2), light_range (m, 0 = default), spot_cone=15,30 (inner,outer degrees). Cameras: camera_fov (degrees), camera_near, camera_far (0 = none).","inputSchema":{"type":"object","additionalProperties":false,"required":["action"],"properties":{"action":{"type":"string","enum":["set","add","duplicate","group","remove"]},"object":{"type":"string","minLength":1,"maxLength":256},"objects":{"type":"array","maxItems":64,"items":{"type":"string","minLength":1,"maxLength":256}},"scene":{"type":"string","maxLength":256},"assignments":{"type":"array","maxItems":64,"items":{"type":"string","maxLength":4096}},"kind":{"type":"string","enum":["empty","cube","sphere","plane","cylinder","capsule","sun","point_light","spot_light","camera","terrain","environment"]},"from":{"type":"string","maxLength":256},"parent":{"type":"string","maxLength":256},"confirm":{"type":"boolean"}}}},
{"name":"scene_diff","description":"Compare the scene with an earlier version, by persistent #id: added, removed and changed fields. Give one of against (an earlier .glb inside the game) or revision (a Git revision such as HEAD, HEAD~1, main or a commit hash: the scene as that commit saved it).","inputSchema":{"type":"object","additionalProperties":false,"properties":{"against":{"type":"string","minLength":1,"maxLength":256},"revision":{"type":"string","minLength":1,"maxLength":256},"scene":{"type":"string","maxLength":256},"limit":{"type":"integer","minimum":1,"maximum":200}}}},
{"name":"asset_search","description":"Find the game's assets by words in their name or folder, one line each: kind and path inside the game, best match first. kind is the folder: model (models/), texture (images/), sound (sounds/), script (scripts/), prefab (prefabs/); tags are the subfolders (models/enemies/orc.glb is tagged enemies). No query lists everything. limit defaults to 20 (0 = all).","inputSchema":{"type":"object","additionalProperties":false,"properties":{"query":{"type":"string","maxLength":256},"kind":{"type":"string","enum":["model","texture","sound","script","prefab"]},"tag":{"type":"string","minLength":1,"maxLength":256},"limit":{"type":"integer","minimum":0,"maximum":200}}}},
{"name":"template_search","description":"Find the templates new games start from (redgem new <game> --template <name>) by words in their name or description, one line each, best match first. No query lists them all.","inputSchema":{"type":"object","additionalProperties":false,"properties":{"query":{"type":"string","maxLength":256},"limit":{"type":"integer","minimum":0,"maximum":200}}}},
{"name":"game_run","description":"Play the saved game headlessly (a fixed step a frame: 1/60 s, or 1/tick_hz of game.json, seed 0, at most 600 frames) and report each script error once with file:line. assert, until and print are Ruby expressions evaluated in the game after each frame, e.g. print=\"Scene.find('Player').transform.position\"; they can use anything scripts can (Scene, Physics, Input) and can change the run. input names a JSON file in the game: a list of events, each {\"frame\": N (0-based)} with any of keys [\"w\"], mouse {x, y, normalized, buttons [\"left\"], click [\"right\"], wheel}, pads [{pad, buttons [\"a\"], axes {\"left_x\": -1, \"right_trigger\": 1}}], touch {x, y} (a finger held) or tap {x, y}; x and y are screen pixels, or 0..1 with normalized true. An event replaces everything held until the next event (a gamepad, once listed, stays plugged in); click, tap and wheel last one frame, and the frame after a click or tap must have no event, and a click or tap cannot press a button the event before still holds. Mouse, touch and tap share one pointer: one per event.","inputSchema":{"type":"object","additionalProperties":false,"properties":{"scene":{"type":"string","maxLength":256},"frames":{"type":"integer","minimum":1,"maximum":600},"seed":{"type":"integer","minimum":0,"maximum":2147483647},"input":{"type":"string","maxLength":256},"assert":{"type":"string","maxLength":4096},"until":{"type":"string","maxLength":4096},"print":{"type":"string","maxLength":4096},"every":{"type":"integer","minimum":1,"maximum":600}}}},
{"name":"game_check","description":"Check scripts, scenes and referenced assets without playing or running the game's tests.","inputSchema":{"type":"object","additionalProperties":false,"properties":{}}},
{"name":"game_shot","description":"A picture of the saved game at a moment, through its camera: a PNG image, 512x288 unless width says otherwise. For whether things work use game_run; this shows how it looks.","inputSchema":{"type":"object","additionalProperties":false,"properties":{"scene":{"type":"string","maxLength":256},"at":{"type":"number","minimum":0,"maximum":10},"width":{"type":"integer","minimum":128,"maximum":1024},"camera":{"type":"string","maxLength":256}}}},
{"name":"game_query","description":"Ask the playing game: inspect an object's live state (world pos, rot, body, velocity, grounded, props), raycast between two points (from, to) or overlap a sphere (at, radius). Points are x,y,z. Connected to an editor, it asks the game the editor is playing (start it with editor action=play); otherwise it plays the saved game for frames (default 60) first.","inputSchema":{"type":"object","additionalProperties":false,"required":["action"],"properties":{"action":{"type":"string","enum":["inspect","raycast","overlap"]},"object":{"type":"string","minLength":1,"maxLength":256},"from":{"type":"string","maxLength":64},"to":{"type":"string","maxLength":64},"at":{"type":"string","maxLength":64},"radius":{"type":"number","minimum":0.01,"maximum":1000},"frames":{"type":"integer","minimum":0,"maximum":600},"scene":{"type":"string","maxLength":256}}}},
{"name":"project","description":"The game beyond its scenes, as files. settings: game.json, or one section of it, and its problems. set_settings: merge changes into game.json (an object merges into its object, null removes a key), e.g. {\"changes\":{\"window\":{\"title\":\"Coast\"},\"input\":{\"actions\":{\"jump\":[\"key:space\",\"pad:a\"]}}}}; the menu section is how the game's menus look: {\"menu\":{\"theme\":{\"colors\":{\"button\":\"#203040\"}}}}. texts: the texts of locales/<language>.json (language default en), keys starting with prefix, limit default 50. set_texts: texts {\"key\":\"text\"} into that language, null removing one. story: list the files of kind (dialogue, quest, item), or read one by name (npcs/baker). set_story: write a whole file of kind and name as content (replacing one needs confirm=true). scenes: the scene names. new_scene: make scenes/<scene>.glb with a ground, a sun and a camera. new_game: make a game in a new folder path next to this one, from template (default empty; see template_search). Every write is checked as game_check checks the game (an item only through the quests that name it): refused, the file kept, when it brings a new error; new warnings are told.","inputSchema":{"type":"object","additionalProperties":false,"required":["action"],"properties":{"action":{"type":"string","enum":["settings","set_settings","texts","set_texts","story","set_story","scenes","new_scene","new_game"]},"section":{"type":"string","minLength":1,"maxLength":64},"changes":{"type":"object"},"language":{"type":"string","minLength":2,"maxLength":16},"prefix":{"type":"string","maxLength":256},"limit":{"type":"integer","minimum":1,"maximum":200},"texts":{"type":"object"},"kind":{"type":"string","enum":["dialogue","quest","item"]},"name":{"type":"string","minLength":1,"maxLength":256},"content":{"type":"object"},"confirm":{"type":"boolean"},"scene":{"type":"string","minLength":1,"maxLength":256},"path":{"type":"string","minLength":1,"maxLength":256},"template":{"type":"string","minLength":1,"maxLength":256}}}},
{"name":"build","description":"Make a standalone game from the saved files (save the editor's edits first), as the editor's Build menu does, on a thread of its own: start returns at once. action start: target linux, windows, android or macos (default: this system; macos only on a Mac), format apk or bundle (android), loose=true keeps the game's files in a folder instead of game.rgpak; it goes next to the game, into <game>-build (or <game>-build-<target>), or the folder named by folder there, replacing only what an earlier build wrote. action status: the build's step and time, or how it ended (the executable, app or APK it made, or why not, with its last log lines); wait waits up to that many seconds for it to end. One build at a time.","inputSchema":{"type":"object","additionalProperties":false,"required":["action"],"properties":{"action":{"type":"string","enum":["start","status"]},"target":{"type":"string","enum":["linux","windows","android","macos"]},"format":{"type":"string","enum":["apk","bundle"]},"loose":{"type":"boolean"},"folder":{"type":"string","minLength":1,"maxLength":256},"wait":{"type":"integer","minimum":0,"maximum":25}}}},
{"name":"terrain","description":"A terrain's ground layers (sand, grass, rock...: up to 8 materials blended by weights) and the rules that paint them, as files. An object uses the layers of terrains/<name>.json when it has prop.terrain_layers=<name> (scene_edit set). action layers: the game's layers files; with name, that file; with object, the file the object names. set_layers writes content as terrains/<name>.json, refused when the game could not read it; replacing a file needs confirm=true. A file is {\"splat_size\": 1024, \"layers\": [{\"name\": \"sand\", \"albedo\": \"textures/sand/Color.jpg\", \"normal\": \"...\", \"roughness\": \"...\", \"height\": \"...\", \"tiling\": 4, \"tint\": \"#ffffff\", \"sharpness\": 0.5, \"variation\": 0.2, \"stochastic\": true, \"triplanar\": false}], \"rules\": [{\"layer\": \"sand\"}, {\"layer\": \"grass\", \"height\": {\"min\": 2, \"soft\": 1.5}}, {\"layer\": \"rock\", \"slope\": {\"min\": 35, \"soft\": 10}}, {\"layer\": \"dirt\", \"noise\": {\"min\": 0.6, \"soft\": 0.1}, \"noise_size\": 12, \"seed\": 3, \"strength\": 0.8}]}: the first layer covers what no weight says, the rules run in order, heights in meters of the mesh, slopes in degrees. paint paints the object's layers by those rules on its saved mesh and writes the weights beside the file (terrains/<name>.splat0.png).","inputSchema":{"type":"object","additionalProperties":false,"required":["action"],"properties":{"action":{"type":"string","enum":["layers","set_layers","paint"]},"name":{"type":"string","minLength":1,"maxLength":64},"object":{"type":"string","minLength":1,"maxLength":256},"scene":{"type":"string","maxLength":256},"content":{"type":"object"},"confirm":{"type":"boolean"}}}},
{"name":"api","description":"Read the scripting API: the class index, one class, or a search. Example: {\"class\":\"Transform\"}.","inputSchema":{"type":"object","additionalProperties":false,"properties":{"class":{"type":"string","maxLength":256},"search":{"type":"string","maxLength":256}}}},
{"name":"events","description":"Read log messages (script errors, Debug.log) since a cursor, one line each. Pass the returned cursor as since next time to get only new ones.","inputSchema":{"type":"object","additionalProperties":false,"properties":{"since":{"type":"integer","minimum":0},"limit":{"type":"integer","minimum":1,"maximum":32},"level":{"type":"string","enum":["debug","info","warning","error"]}}}}
])JSON";
// What scene_edit's assignments take, told to the agent: without it, an agent guesses words (color=)
// that only become custom properties nothing reads.
// What scene_edit's list of words leaves out and agents guess wrong (color=), which only makes a custom
// property nothing reads.
constexpr const char* kColorWords =
    "tint=#rrggbb multiplies the colors of this object's mesh: the way to color one object. Other bare keys "
    "become custom properties.";

constexpr const char* kEditorSchema =
    R"JSON({"name":"editor","description":"The open editor: status, select an object or several (objects), frame an object, capture the viewport (PNG, max_size default 512), play/pause/resume/stop/step its game, undo, redo or save, open_scene (another scene by name; one with unsaved edits needs save first, or discard=true) or save_as (the open scene as scenes/<scene>.glb, then the one edited). Document changes need edit mode and stay unsaved until save.","inputSchema":{"type":"object","additionalProperties":false,"required":["action"],"properties":{"action":{"type":"string","enum":["status","select","frame","undo","redo","save","play","pause","resume","stop","step","capture","open_scene","save_as"]},"scene":{"type":"string","minLength":1,"maxLength":256},"discard":{"type":"boolean"},"object":{"type":"string","minLength":1,"maxLength":256},"objects":{"type":"array","maxItems":64,"items":{"type":"string","minLength":1,"maxLength":256}},"max_size":{"type":"integer","minimum":128,"maximum":1024}}}})JSON";

bool read_only_tool(const std::string& name) {
  return name == "scene_list" || name == "scene_get" || name == "scene_diff" || name == "api" || name == "events" ||
         name == "editor" || name == "asset_search" || name == "template_search";
}

// The tools --read-only keeps with fewer actions: they only read then.
bool narrowed_when_read_only(const std::string& name) {
  return name == "editor" || name == "project" || name == "terrain";
}

bool kept_read_only(const std::string& name) { return read_only_tool(name) || narrowed_when_read_only(name); }

bool enabled(const Options& options, const std::string& name) {
  if (options.read_only && !kept_read_only(name)) return false;
  return options.allow.empty() || options.allow.count(name) != 0;
}

const char* type_name(const Json& schema) {
  const std::string type = schema.at("type");
  if (type == "object") return "an object";
  if (type == "array") return "a list";
  if (type == "string") return "a string";
  if (type == "integer") return "a whole number";
  if (type == "number") return "a number";
  return "true or false";
}

bool correct_type(const Json& schema, const Json& value) {
  const std::string type = schema.at("type");
  if (type == "object") return value.is_object();
  if (type == "array") return value.is_array();
  if (type == "string") return value.is_string();
  if (type == "integer") return value.is_number_integer();
  if (type == "number") return value.is_number();
  return type == "boolean" && value.is_boolean();
}

std::string choices(const Json& schema) {
  std::string text;
  for (const auto& choice : schema["enum"]) text += (text.empty() ? "" : ", ") + choice.get<std::string>();
  return text;
}

bool outside_choices(const Json& schema, const Json& value) {
  return schema.contains("enum") &&
         std::find(schema["enum"].begin(), schema["enum"].end(), value) == schema["enum"].end();
}

void check_scalar(const Json& schema, const Json& value) {
  if (outside_choices(schema, value)) throw std::invalid_argument("must be one of: " + choices(schema));
  if (schema.contains("minimum") && value < schema["minimum"])
    throw std::invalid_argument("must be at least " + schema["minimum"].dump() + ", not " + value.dump());
  if (schema.contains("maximum") && value > schema["maximum"])
    throw std::invalid_argument("must be at most " + schema["maximum"].dump() + ", not " + value.dump());
}

void check_string(const Json& schema, const std::string& value) {
  if (value.find('\0') != std::string::npos) throw std::invalid_argument("text cannot contain NUL");
  if (value.size() < schema.value("minLength", std::size_t(0))) throw std::invalid_argument("cannot be empty");
  const auto longest = schema.value("maxLength", kMaxRequestBytes);
  if (value.size() > longest)
    throw std::invalid_argument("is " + std::to_string(value.size()) + " bytes; the limit is " +
                                std::to_string(longest));
}

// "limit: must be at least 1, not 0": every problem names the argument it is about.
void check_named(const std::string& key, const Json& schema, const Json& value) {
  try {
    validate_arguments(schema, value);
  } catch (const std::invalid_argument& problem) {
    throw std::invalid_argument(key + ": " + problem.what());
  }
}

std::string names(const Json& properties) {
  std::string text;
  for (const auto& [key, ignored] : properties.items()) text += (text.empty() ? "" : ", ") + key;
  return text;
}

void check_object(const Json& schema, const Json& args) {
  for (const auto& field : schema.value("required", Json::array())) {
    const auto& key = field.get_ref<const std::string&>();
    if (!args.contains(key)) throw std::invalid_argument("missing argument: " + key);
  }
  if (!schema.contains("properties")) return;  // a free-form object, such as project's changes
  const Json& properties = schema.at("properties");
  for (const auto& [key, value] : args.items()) {
    if (!properties.contains(key))
      throw std::invalid_argument("unknown argument: " + key + " (accepted: " + names(properties) + ")");
    check_named(key, properties[key], value);
  }
}

void check_array(const Json& schema, const Json& args) {
  if (args.size() < schema.value("minItems", std::size_t(0))) throw std::invalid_argument("too few array items");
  const auto most = schema.at("maxItems").get<std::size_t>();
  if (args.size() > most) throw std::invalid_argument("has more than " + std::to_string(most) + " items");
  for (std::size_t i = 0; i < args.size(); ++i)
    check_named("item " + std::to_string(i + 1), schema.at("items"), args[i]);
}
}  // namespace

namespace {
// The tools only a running editor answers.
void add_editor_tools(Json& tools) {
  tools.push_back(Json::parse(kEditorSchema));
#if defined(REDGEM_RUBY_EVAL)
  tools.push_back(ruby_eval_schema());
#endif
}

Json available_tools(const Options& options) {
  Json tools = Json::parse(kSchemas);
  if (!options.editor_connection.empty()) add_editor_tools(tools);
  return tools;
}

// The actions of project that only read.
Json project_reads() {
  Json reads = Json::array();
  for (const auto& action : Json::parse(kSchemas)) {
    if (action["name"] != "project") continue;
    for (const auto& word : action["inputSchema"]["properties"]["action"]["enum"])
      if (reads_project(word)) reads.push_back(word);
  }
  return reads;
}

Json read_only_actions(const std::string& name) {
  if (name == "editor") return {"status", "capture"};
  if (name == "terrain") return {"layers"};
  return project_reads();
}

// Whether this run keeps only the tool's reading actions.
bool narrowed(const Options& options, const std::string& name) {
  return options.read_only && narrowed_when_read_only(name);
}

void live_schema(Json& tool, const Options& options) {
  if (!narrowed(options, tool["name"])) return;
  tool["inputSchema"]["properties"]["action"]["enum"] = read_only_actions(tool["name"]);
}

// Whether a call of the tool may change or destroy what the game holds.
bool destructive(const Options& options, const std::string& name) {
  if (narrowed_when_read_only(name)) return !options.read_only;
  return name == "scene_edit" || is_ruby_eval_tool(name);
}

// scene_edit says how to color an object, in either mode.
void describe_assignments(Json& tool) {
  if (tool["name"] == "scene_edit") tool["description"] = tool["description"].get<std::string>() + " " + kColorWords;
}
}  // namespace

Json catalog(const Options& options) {
  Json tools = Json::array();
  for (Json tool : available_tools(options)) {
    const std::string name = tool["name"];
    if (!enabled(options, name)) continue;
    live_schema(tool, options);
    describe_assignments(tool);
    tool["annotations"] = {{"readOnlyHint", narrowed_when_read_only(name) ? options.read_only : read_only_tool(name)},
                           {"openWorldHint", !read_only_tool(name)},
                           {"destructiveHint", destructive(options, name)}};
    tools.push_back(std::move(tool));
  }
  return tools;
}

void validate_arguments(const Json& schema, const Json& args) {
  if (!correct_type(schema, args)) throw std::invalid_argument(std::string("must be ") + type_name(schema));
  check_scalar(schema, args);
  if (args.is_string()) return check_string(schema, args.get<std::string>());
  if (args.is_object()) return check_object(schema, args);
  if (args.is_array()) check_array(schema, args);
}
}  // namespace redgem::tools::mcp
