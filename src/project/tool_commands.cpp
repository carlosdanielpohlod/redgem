#include "project/tool_commands.hpp"

#include <algorithm>
#include <iterator>

#include "project/api_command.hpp"
#include "project/check_command.hpp"
#include "project/run_command.hpp"
#include "project/scene_command.hpp"
#include "project/shot_command.hpp"

namespace redgem::tools {
namespace {

using Tool = ToolResult (*)(const std::vector<std::string>&);

struct NamedTool {
  const char* word;
  Tool run;
};
constexpr NamedTool kTools[] = {
    {kSceneCommand, run_scene_command}, {kRunCommand, run_run_command}, {kShotCommand, run_shot_command},
    {kCheckCommand, run_check_command}, {kApiCommand, run_api_command},
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
