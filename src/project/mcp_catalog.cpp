#include "project/mcp_tools.hpp"

#include <algorithm>
#include <stdexcept>

namespace redgem::tools::mcp {
namespace {
// A small fixed vocabulary; schemas are also validated before the native command is called.
constexpr const char* kSchemas = R"JSON([
{"name":"scene_list","description":"List objects in the configured game. Use returned #ids for subsequent calls.","inputSchema":{"type":"object","additionalProperties":false,"properties":{"scene":{"type":"string","maxLength":256},"limit":{"type":"integer","minimum":1,"maximum":200},"depth":{"type":"integer","minimum":0,"maximum":64}}}},
{"name":"scene_get","description":"Read one object by #id, unique name or hierarchy path.","inputSchema":{"type":"object","additionalProperties":false,"required":["object"],"properties":{"object":{"type":"string","minLength":1,"maxLength":256},"scene":{"type":"string","maxLength":256}}}},
{"name":"scene_edit","description":"Set, add or remove a scene object. Assignments use CLI key=value syntax. Remove requires confirm=true. Edits save the file.","inputSchema":{"type":"object","additionalProperties":false,"required":["action","object"],"properties":{"action":{"type":"string","enum":["set","add","remove"]},"object":{"type":"string","minLength":1,"maxLength":256},"scene":{"type":"string","maxLength":256},"assignments":{"type":"array","maxItems":64,"items":{"type":"string","maxLength":4096}},"from":{"type":"string","maxLength":256},"parent":{"type":"string","maxLength":256},"confirm":{"type":"boolean"}}}},
{"name":"scene_diff","description":"Compare the scene with an earlier .glb inside the configured game, by persistent identity.","inputSchema":{"type":"object","additionalProperties":false,"required":["against"],"properties":{"against":{"type":"string","minLength":1,"maxLength":256},"scene":{"type":"string","maxLength":256},"limit":{"type":"integer","minimum":1,"maximum":200}}}},
{"name":"game_run","description":"Run headlessly with a fixed step and seed. Assertions, stop conditions and prints are Ruby expressions and can mutate the game. At most 600 frames.","inputSchema":{"type":"object","additionalProperties":false,"properties":{"scene":{"type":"string","maxLength":256},"frames":{"type":"integer","minimum":1,"maximum":600},"seed":{"type":"integer","minimum":0,"maximum":2147483647},"input":{"type":"string","maxLength":256},"assert":{"type":"string","maxLength":4096},"until":{"type":"string","maxLength":4096},"print":{"type":"string","maxLength":4096},"every":{"type":"integer","minimum":1,"maximum":600}}}},
{"name":"game_check","description":"Check scripts, scenes and referenced assets without running the game's test scripts.","inputSchema":{"type":"object","additionalProperties":false,"properties":{}}},
{"name":"api","description":"Read the scripting API index, one class or a search. Example: {\"class\":\"Transform\"}.","inputSchema":{"type":"object","additionalProperties":false,"properties":{"class":{"type":"string","maxLength":256},"search":{"type":"string","maxLength":256}}}},
{"name":"events","description":"Read this server process's log history since a cursor. Pass the returned cursor next time. Event text is bounded; dropped and cursor_ahead report lost/foreign history.","inputSchema":{"type":"object","additionalProperties":false,"properties":{"since":{"type":"integer","minimum":0},"limit":{"type":"integer","minimum":1,"maximum":32},"level":{"type":"string","enum":["debug","info","warning","error"]}}}}
])JSON";
constexpr const char* kEditorSchema =
    R"JSON({"name":"editor","description":"Read live editor status, select an object, undo, redo or save. Mutations are refused during gestures or play. Live set edits stay unsaved until save.","inputSchema":{"type":"object","additionalProperties":false,"required":["action"],"properties":{"action":{"type":"string","enum":["status","select","undo","redo","save"]},"object":{"type":"string","minLength":1,"maxLength":256}}}})JSON";

bool read_only_tool(const std::string& name) {
  return name == "scene_list" || name == "scene_get" || name == "scene_diff" || name == "api" || name == "events" ||
         name == "editor";
}

bool enabled(const Options& options, const std::string& name) {
  if (options.read_only && !read_only_tool(name)) return false;
  return options.allow.empty() || options.allow.count(name) != 0;
}

bool correct_type(const Json& schema, const Json& value) {
  const std::string type = schema.at("type");
  if (type == "object") return value.is_object();
  if (type == "array") return value.is_array();
  if (type == "string") return value.is_string();
  if (type == "integer") return value.is_number_integer();
  return type == "boolean" && value.is_boolean();
}

void check_scalar(const Json& schema, const Json& value) {
  if (schema.contains("enum") && std::find(schema["enum"].begin(), schema["enum"].end(), value) == schema["enum"].end())
    throw std::invalid_argument("value is outside the permitted choices");
  if (schema.contains("minimum") && value < schema["minimum"]) throw std::invalid_argument("value is too small");
  if (schema.contains("maximum") && value > schema["maximum"]) throw std::invalid_argument("value is too large");
}

void check_string(const Json& schema, const std::string& value) {
  if (value.find('\0') != std::string::npos) throw std::invalid_argument("text cannot contain NUL");
  if (value.size() < schema.value("minLength", std::size_t(0))) throw std::invalid_argument("text is too short");
  if (value.size() > schema.value("maxLength", kMaxRequestBytes)) throw std::invalid_argument("text is too long");
}

void check_object(const Json& schema, const Json& args) {
  for (const auto& field : schema.value("required", Json::array())) {
    const auto& key = field.get_ref<const std::string&>();
    if (!args.contains(key)) throw std::invalid_argument("missing argument: " + key);
  }
  const Json& properties = schema.at("properties");
  for (const auto& [key, value] : args.items()) {
    if (!properties.contains(key)) throw std::invalid_argument("unknown argument: " + key);
    validate_arguments(properties[key], value);
  }
}

void check_array(const Json& schema, const Json& args) {
  if (args.size() < schema.value("minItems", std::size_t(0))) throw std::invalid_argument("too few array items");
  if (args.size() > schema.at("maxItems").get<std::size_t>()) throw std::invalid_argument("too many array items");
  for (const auto& item : args) validate_arguments(schema.at("items"), item);
}
}  // namespace

namespace {
Json available_tools(const Options& options) {
  Json tools = Json::parse(kSchemas);
  if (!options.editor_connection.empty()) tools.push_back(Json::parse(kEditorSchema));
  return tools;
}

void live_schema(Json& tool, const Options& options) {
  if (options.editor_connection.empty()) return;
  if (tool["name"] == "editor" && options.read_only) tool["inputSchema"]["properties"]["action"]["enum"] = {"status"};
  if (tool["name"] != "scene_edit") return;
  auto& schema = tool["inputSchema"];
  schema["properties"]["action"]["enum"] = {"set"};
  schema["properties"]["assignments"]["minItems"] = 1;
  schema["required"].push_back("assignments");
  for (const char* field : {"from", "parent", "confirm"}) schema["properties"].erase(field);
  tool["description"] =
      "Set assignments on an existing object in the live editor, atomically as one undo step. Does not save.";
}
}  // namespace

Json catalog(const Options& options) {
  Json tools = Json::array();
  for (Json tool : available_tools(options)) {
    const std::string name = tool["name"];
    if (!enabled(options, name)) continue;
    live_schema(tool, options);
    tool["annotations"] = {{"readOnlyHint", name == "editor" ? options.read_only : read_only_tool(name)},
                           {"openWorldHint", !read_only_tool(name)},
                           {"destructiveHint", name == "scene_edit" || (name == "editor" && !options.read_only)}};
    tools.push_back(std::move(tool));
  }
  return tools;
}

void validate_arguments(const Json& schema, const Json& args) {
  if (!correct_type(schema, args)) throw std::invalid_argument("argument has the wrong type");
  check_scalar(schema, args);
  if (args.is_string()) return check_string(schema, args.get<std::string>());
  if (args.is_object()) return check_object(schema, args);
  if (args.is_array()) check_array(schema, args);
}
}  // namespace redgem::tools::mcp
