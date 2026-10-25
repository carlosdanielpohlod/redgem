#include "project/assistant_guide.hpp"

#include <algorithm>
#include <cstring>
#include <iterator>

#include "core/files.hpp"
#include "generated/assistant_files.hpp"
#include "project/api_reference.hpp"

namespace redgem::tools {
namespace {

// Where the guide is in the repository, as it was built in, and what in it the calls replace.
constexpr const char* kGuideFile = "assets/assistant/AGENTS.md";
constexpr const char* kCallsMark = "{{most_used_calls}}";
// What the file one assistant reads instead says: it points at the other (its @ brings it in).
constexpr const char* kPointer = "See @AGENTS.md: the guide to this game, for every coding assistant.\n";

std::string built_in_guide() {
  const auto* file =
      std::find_if(std::begin(generated::kAssistantFiles), std::end(generated::kAssistantFiles),
                   [](const generated::EmbeddedFile& each) { return std::strcmp(each.path, kGuideFile) == 0; });
  return file->contents;
}

// "- `Input.axis(name)`: what it does. `x = Input.axis(:horizontal)`".
std::string call_line(const ApiCall& call) {
  return "- `" + signature(*call.owner, *call.method) + "`: " + call.method->about + ". `" + call.method->example +
         "`\n";
}

std::string most_used_lines() {
  std::string lines;
  for (const ApiCall& call : most_used_calls(api_reference())) lines += call_line(call);
  if (!lines.empty()) lines.pop_back();  // (the guide has the line's end)
  return lines;
}

}  // namespace

std::string assistant_guide() {
  std::string guide = built_in_guide();
  const std::size_t mark = guide.find(kCallsMark);
  if (mark != std::string::npos) guide.replace(mark, std::strlen(kCallsMark), most_used_lines());
  return guide;
}

bool write_assistant_guide(const std::filesystem::path& game_dir, std::string& error) {
  return write_text_file(game_dir / kGuideFileName, assistant_guide(), error) &&
         write_text_file(game_dir / kPointerFileName, kPointer, error);
}

}  // namespace redgem::tools
