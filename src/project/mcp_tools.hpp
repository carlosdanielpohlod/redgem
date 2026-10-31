#pragma once

#include <filesystem>
#include <set>
#include <string>

#include "project/mcp_protocol.hpp"

namespace redgem::tools::mcp {
// Lines a scene list shows when the request names no limit, in file and live mode alike.
inline constexpr std::size_t kDefaultListLimit = 50;

// What --editor takes instead of a connection file: find this game's running editor on every call.
inline constexpr const char* kDiscoverEditor = "auto";

struct Options {
  std::filesystem::path game;
  bool read_only = false;
  std::set<std::string> allow;
  std::filesystem::path editor_connection{};  // a connection file, kDiscoverEditor, or empty for files only
  std::filesystem::path bridge_root{};        // where discovery looks (the user's data folder when empty)
};
// Whether calls look for the running editor themselves, falling back to files when there is none.
inline bool discovers_editor(const Options& options) { return options.editor_connection == kDiscoverEditor; }
Json catalog(const Options& options);
Json invoke(const Options& options, const std::string& name, const Json& args);
void validate_arguments(const Json& schema, const Json& args);
}  // namespace redgem::tools::mcp
