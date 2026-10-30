#pragma once

// The coding agents the Agent panel offers: a name and the command that starts an Agent Client
// Protocol adapter for it. Claude Code and Codex come as presets (their open-source adapters,
// installed with npm); the user edits them or adds others, kept in agents.json in the user's
// RedGem data folder.

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "editor/agent/child_process.hpp"

namespace redgem::agent {

// The adapters' npm packages and the commands they install.
inline constexpr const char* kClaudePackage = "@agentclientprotocol/claude-agent-acp";
inline constexpr const char* kClaudeCommand = "claude-agent-acp";
inline constexpr const char* kCodexPackage = "@agentclientprotocol/codex-acp";
inline constexpr const char* kCodexCommand = "codex-acp";
// What runs a package without installing it.
inline constexpr const char* kPackageRunner = "npx";

struct AgentProfile {
  std::string name;  // what the picker shows
  std::string command;
  std::vector<std::string> arguments;
  std::vector<EnvironmentVariable> environment;
  // The npm package that installs `command`: for the hint shown when it is missing, and to run it
  // through npx meanwhile. Empty for an agent of the user's own.
  std::string package;
};

std::vector<AgentProfile> preset_agents();

// <user data>/agents.json.
std::filesystem::path agents_file();
// The agents in `file`; the presets when it is missing or unreadable.
std::vector<AgentProfile> read_agents(const std::filesystem::path& file);
bool write_agents(const std::filesystem::path& file, const std::vector<AgentProfile>& agents, std::string& error);

// Where a program is on this machine (empty when nowhere): find_program, or a test's own.
using ProgramFinder = std::function<std::filesystem::path(const std::string& program)>;

// How to log in to a preset's agent, for when it asks for that; empty for others.
std::string login_hint(const AgentProfile& agent);

// How to start an agent: its command when it is installed, else its package through npx. nullopt,
// with `error` saying what to install, when neither is there.
std::optional<ProgramCall> launch_call(const AgentProfile& agent, const ProgramFinder& find, std::string& error);

// The settings' text fields: arguments a line each, and environment variables as NAME=value lines.
std::string lines_text(const std::vector<std::string>& lines);
std::vector<std::string> text_lines(const std::string& text);
std::string environment_text(const std::vector<EnvironmentVariable>& environment);
// Lines without a name before their '=' are left out.
std::vector<EnvironmentVariable> environment_of_text(const std::string& text);

}  // namespace redgem::agent
