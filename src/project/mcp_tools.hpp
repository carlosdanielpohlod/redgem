#pragma once

#include <filesystem>
#include <set>
#include <string>

#include "project/mcp_protocol.hpp"

namespace redgem::tools::mcp {
struct Options {
  std::filesystem::path game;
  bool read_only = false;
  std::set<std::string> allow;
  std::filesystem::path editor_connection{};
};
Json catalog(const Options& options);
Json invoke(const Options& options, const std::string& name, const Json& args);
void validate_arguments(const Json& schema, const Json& args);
}  // namespace redgem::tools::mcp
