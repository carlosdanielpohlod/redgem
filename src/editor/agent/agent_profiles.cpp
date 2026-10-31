#include "editor/agent/agent_profiles.hpp"

#include <system_error>

#include "app/json_file.hpp"
#include "core/files.hpp"
#include "core/platform.hpp"
#include "core/translations.hpp"

namespace redgem::agent {
namespace {

constexpr const char* kAgentsFileName = "agents.json";
constexpr const char* kAgentsKey = "agents";
constexpr const char* kRunWithoutAsking = "--yes";  // npx: fetch the package without a question
constexpr int kFileIndent = 2;
constexpr const char* kClaudeName = "Claude Code";  // the presets, as the picker names them
constexpr const char* kCodexName = "Codex";

using Json = nlohmann::json;

AgentProfile preset(const char* name, const char* command, const char* package) {
  AgentProfile agent;
  agent.name = name;
  agent.command = command;
  agent.package = package;
  return agent;
}

Json environment_json(const std::vector<EnvironmentVariable>& environment) {
  Json listed = Json::object();
  for (const EnvironmentVariable& variable : environment) listed[variable.name] = variable.value;
  return listed;
}

std::vector<EnvironmentVariable> environment_of(const Json& listed) {
  std::vector<EnvironmentVariable> environment;
  if (!listed.is_object()) return environment;
  for (const auto& [name, value] : listed.items()) {
    if (value.is_string()) environment.push_back({name, value.get<std::string>()});
  }
  return environment;
}

std::vector<std::string> texts_of(const Json& listed) {
  std::vector<std::string> texts;
  if (!listed.is_array()) return texts;
  for (const Json& text : listed) {
    if (text.is_string()) texts.push_back(text.get<std::string>());
  }
  return texts;
}

std::string text_of(const Json& value, const char* key) {
  return holds_text(value, key) ? value[key].get<std::string>() : "";
}

// An agent as the file keeps it says at least its name and its command.
bool describes_an_agent(const Json& value) { return holds_text(value, "name") && holds_text(value, "command"); }

bool lists_agents(const Json& read) {
  return read.is_object() && read.contains(kAgentsKey) && read[kAgentsKey].is_array();
}

std::optional<AgentProfile> profile_of(const Json& value) {
  if (!describes_an_agent(value)) return std::nullopt;
  AgentProfile agent;
  agent.name = text_of(value, "name");
  agent.command = text_of(value, "command");
  agent.arguments = texts_of(value.value("args", Json()));
  agent.environment = environment_of(value.value("env", Json()));
  agent.package = text_of(value, "package");
  return agent;
}

Json profile_json(const AgentProfile& agent) {
  return {{"name", agent.name},
          {"command", agent.command},
          {"args", agent.arguments},
          {"env", environment_json(agent.environment)},
          {"package", agent.package}};
}

bool runs_through_npx(const AgentProfile& agent, const ProgramFinder& find) {
  return !agent.package.empty() && !find(kPackageRunner).empty();
}

ProgramCall through_npx(const AgentProfile& agent) {
  ProgramCall call{kPackageRunner, {kRunWithoutAsking, agent.package}, agent.environment};
  call.arguments.insert(call.arguments.end(), agent.arguments.begin(), agent.arguments.end());
  return call;
}

std::string install_hint(const AgentProfile& agent) {
  if (agent.package.empty()) return tr("editor.agent.command_missing", {{"command", agent.command}});
  return tr("editor.agent.adapter_missing", {{"command", agent.command}, {"package", agent.package}});
}

}  // namespace

std::vector<AgentProfile> preset_agents() {
  return {preset(kClaudeName, kClaudeCommand, kClaudePackage), preset(kCodexName, kCodexCommand, kCodexPackage)};
}

std::filesystem::path agents_file() { return platform::user_data_dir() / kAgentsFileName; }

std::vector<AgentProfile> read_agents(const std::filesystem::path& file) {
  const Json read = read_json_file(file);
  if (!lists_agents(read)) return preset_agents();
  std::vector<AgentProfile> agents;
  for (const Json& value : read[kAgentsKey]) {
    if (auto agent = profile_of(value)) agents.push_back(std::move(*agent));
  }
  return agents.empty() ? preset_agents() : agents;
}

bool write_agents(const std::filesystem::path& file, const std::vector<AgentProfile>& agents, std::string& error) {
  Json listed = Json::array();
  for (const AgentProfile& agent : agents) listed.push_back(profile_json(agent));
  std::error_code fs_error;
  std::filesystem::create_directories(file.parent_path(), fs_error);
  return write_text_file(file, Json{{kAgentsKey, listed}}.dump(kFileIndent) + "\n", error);
}

std::string login_hint(const AgentProfile& agent) {
  if (agent.package == kClaudePackage) return tr("editor.agent.login_claude");
  if (agent.package == kCodexPackage) return tr("editor.agent.login_codex");
  return "";
}

std::optional<ProgramCall> launch_call(const AgentProfile& agent, const ProgramFinder& find, std::string& error) {
  if (!find(agent.command).empty()) return ProgramCall{agent.command, agent.arguments, agent.environment};
  if (runs_through_npx(agent, find)) return through_npx(agent);
  error = install_hint(agent);
  return std::nullopt;
}

}  // namespace redgem::agent
