#include "project/mcp_tools.hpp"

#include <algorithm>
#include <stdexcept>

namespace redgem::tools::mcp {
namespace {
// A small fixed vocabulary; schemas are also validated before the native command is called.
constexpr const char* kSchemas = R"JSON([
{"name":"scene_list","description":"List scene objects, one line each: #id name, then only what differs from defaults (pos, rot in degrees, scale, scripts, props). limit defaults to 50 (0 = all); depth 0 = top level only. Replies stop at 8 KiB (about 100 lines): for a bigger scene list a level with depth, then scene_get. Pass #ids back unchanged.","inputSchema":{"type":"object","additionalProperties":false,"properties":{"scene":{"type":"string","maxLength":256},"limit":{"type":"integer","minimum":0,"maximum":200},"depth":{"type":"integer","minimum":0,"maximum":64}}}},
{"name":"scene_get","description":"Read one object in full by #id, unique name or hierarchy path (Level/Wall).","inputSchema":{"type":"object","additionalProperties":false,"required":["object"],"properties":{"object":{"type":"string","minLength":1,"maxLength":256},"scene":{"type":"string","maxLength":256}}}},
{"name":"scene_edit","description":"Set, add or remove one object. assignments are key=value as scene_list prints them: position=1,2,3 rotation=0,90,0 scale=2 name=Gate parent=Level prop.speed=2.5 script+=Coin script-=Coin. add with from copies that object (mesh, scripts, props; files only), without it adds an empty object. remove needs confirm=true. On files the scene is saved; in a live editor each call is one undo step and stays unsaved.","inputSchema":{"type":"object","additionalProperties":false,"required":["action","object"],"properties":{"action":{"type":"string","enum":["set","add","remove"]},"object":{"type":"string","minLength":1,"maxLength":256},"scene":{"type":"string","maxLength":256},"assignments":{"type":"array","maxItems":64,"items":{"type":"string","maxLength":4096}},"from":{"type":"string","maxLength":256},"parent":{"type":"string","maxLength":256},"confirm":{"type":"boolean"}}}},
{"name":"scene_diff","description":"Compare the scene with an earlier .glb inside the game, by persistent #id: added, removed and changed fields.","inputSchema":{"type":"object","additionalProperties":false,"required":["against"],"properties":{"against":{"type":"string","minLength":1,"maxLength":256},"scene":{"type":"string","maxLength":256},"limit":{"type":"integer","minimum":1,"maximum":200}}}},
{"name":"game_run","description":"Play the saved game headlessly (1/60 s steps, seed 0, at most 600 frames) and report each script error once with file:line. assert, until and print are Ruby expressions evaluated in the game after each frame, e.g. print=\"Scene.find('Player').transform.position\"; they can use anything scripts can (Scene, Physics, Input) and can change the run.","inputSchema":{"type":"object","additionalProperties":false,"properties":{"scene":{"type":"string","maxLength":256},"frames":{"type":"integer","minimum":1,"maximum":600},"seed":{"type":"integer","minimum":0,"maximum":2147483647},"input":{"type":"string","maxLength":256},"assert":{"type":"string","maxLength":4096},"until":{"type":"string","maxLength":4096},"print":{"type":"string","maxLength":4096},"every":{"type":"integer","minimum":1,"maximum":600}}}},
{"name":"game_check","description":"Check scripts, scenes and referenced assets without playing or running the game's tests.","inputSchema":{"type":"object","additionalProperties":false,"properties":{}}},
{"name":"game_shot","description":"A picture of the saved game at a moment, through its camera: a PNG image, 512x288 unless width says otherwise. For whether things work use game_run; this shows how it looks.","inputSchema":{"type":"object","additionalProperties":false,"properties":{"scene":{"type":"string","maxLength":256},"at":{"type":"number","minimum":0,"maximum":10},"width":{"type":"integer","minimum":128,"maximum":1024},"camera":{"type":"string","maxLength":256}}}},
{"name":"game_query","description":"Ask the playing game: inspect an object's live state (world pos, rot, body, velocity, grounded, props), raycast between two points (from, to) or overlap a sphere (at, radius). Points are x,y,z. Connected to an editor, it asks the game the editor is playing (start it with editor action=play); otherwise it plays the saved game for frames (default 60) first.","inputSchema":{"type":"object","additionalProperties":false,"required":["action"],"properties":{"action":{"type":"string","enum":["inspect","raycast","overlap"]},"object":{"type":"string","minLength":1,"maxLength":256},"from":{"type":"string","maxLength":64},"to":{"type":"string","maxLength":64},"at":{"type":"string","maxLength":64},"radius":{"type":"number","minimum":0.01,"maximum":1000},"frames":{"type":"integer","minimum":0,"maximum":600},"scene":{"type":"string","maxLength":256}}}},
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
    R"JSON({"name":"editor","description":"The open editor: status, select or frame an object, capture the viewport (PNG, max_size default 512), play/pause/resume/stop/step its game, undo, redo or save. Document changes need edit mode and stay unsaved until save.","inputSchema":{"type":"object","additionalProperties":false,"required":["action"],"properties":{"action":{"type":"string","enum":["status","select","frame","undo","redo","save","play","pause","resume","stop","step","capture"]},"object":{"type":"string","minLength":1,"maxLength":256},"max_size":{"type":"integer","minimum":128,"maximum":1024}}}})JSON";

bool read_only_tool(const std::string& name) {
  return name == "scene_list" || name == "scene_get" || name == "scene_diff" || name == "api" || name == "events" ||
         name == "editor";
}

bool enabled(const Options& options, const std::string& name) {
  if (options.read_only && !read_only_tool(name)) return false;
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
Json available_tools(const Options& options) {
  Json tools = Json::parse(kSchemas);
  if (!options.editor_connection.empty()) tools.push_back(Json::parse(kEditorSchema));
  return tools;
}

// A pinned editor connection always answers scene edits live, where from is refused: the schema
// leaves it out. With discovery the files may answer instead, so it stays.
bool always_live(const Options& options) { return !options.editor_connection.empty() && !discovers_editor(options); }

void live_schema(Json& tool, const Options& options) {
  if (tool["name"] == "editor" && options.read_only)
    tool["inputSchema"]["properties"]["action"]["enum"] = {"status", "capture"};
  if (tool["name"] == "scene_edit" && always_live(options)) tool["inputSchema"]["properties"].erase("from");
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
    tool["annotations"] = {{"readOnlyHint", name == "editor" ? options.read_only : read_only_tool(name)},
                           {"openWorldHint", !read_only_tool(name)},
                           {"destructiveHint", name == "scene_edit" || (name == "editor" && !options.read_only)}};
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
