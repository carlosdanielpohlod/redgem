#include "project/tool_commands.hpp"

#include <algorithm>
#include <iterator>

#include "project/scene_command.hpp"

namespace redgem::tools {
namespace {

using Tool = ToolResult (*)(const std::vector<std::string>&);

struct NamedTool {
  const char* word;
  Tool run;
};
constexpr NamedTool kTools[] = {
    {kSceneCommand, run_scene_command},
};

}  // namespace

std::optional<ToolResult> run_tool_command(const std::vector<std::string>& args) {
  if (args.empty()) return std::nullopt;
  const auto* tool = std::find_if(std::begin(kTools), std::end(kTools),
                                  [&args](const NamedTool& known) { return args.front() == known.word; });
  if (tool == std::end(kTools)) return std::nullopt;
  return tool->run(std::vector<std::string>(args.begin() + 1, args.end()));
}

}  // namespace redgem::tools
