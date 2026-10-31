#pragma once

// How the Agent panel draws each entry of a conversation (agent_transcript_view.cpp) and the
// markdown of the agent's answers (agent_markdown_view.cpp). Each takes the entry's index, which
// tells its widgets apart from those of the other entries.

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "editor/agent/agent_transcript.hpp"
#include "editor/agent/markdown_blocks.hpp"

namespace redgem::ui::agent_view {

// Blocks of markdown, wrapped to the width of the window.
void draw_markdown(const std::vector<agent::Block>& blocks);

void draw_user(const std::string& text);
void draw_thought(const std::string& text);
void draw_tool(const agent::ToolCall& call);
void draw_plan(const std::vector<agent::PlanItem>& plan);
// The question with a button per option; the id of the option clicked, if one was.
std::optional<std::string> draw_permission(const agent::PermissionRequest& request);
void draw_notice(const std::string& text);
void draw_error(const std::string& text);
// "Working..." with a turning mark, while a turn runs.
void draw_working();

}  // namespace redgem::ui::agent_view
