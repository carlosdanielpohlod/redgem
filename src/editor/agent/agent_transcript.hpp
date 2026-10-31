#pragma once

// A conversation with a coding agent as the Agent panel shows it: what the user said, what the agent
// answered (streamed in pieces), what it thought, the tools it called and how they went, its plan,
// and the questions it asked before acting (each answered once).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "editor/agent/json_rpc.hpp"

namespace redgem::agent {

enum class ToolStatus { Pending, InProgress, Completed, Failed };
enum class PlanStatus { Pending, InProgress, Completed };

// A change a tool made (or wants to make) to a file: its text before (none for a new file) and after.
struct FileChange {
  std::string path;
  std::optional<std::string> old_text;
  std::string new_text;
};

// One piece of what a tool call shows: text it produced, or a change to a file.
struct ToolContent {
  std::string text;
  std::optional<FileChange> change;
};

struct ToolCall {
  std::string id;
  std::string title;
  std::string kind;  // read, edit, delete, move, search, execute, think, fetch, switch_mode, other
  ToolStatus status = ToolStatus::Pending;
  std::vector<ToolContent> content;
  std::string input;                   // what it was called with, as JSON text (empty when the agent did not say)
  std::vector<std::string> locations;  // the files it works on
};

// What an update of a tool call changes: only the fields the agent sent.
struct ToolCallPatch {
  std::string id;
  std::optional<std::string> title;
  std::optional<std::string> kind;
  std::optional<ToolStatus> status;
  std::optional<std::vector<ToolContent>> content;  // replaces the whole content
  std::optional<std::string> input;
  std::optional<std::vector<std::string>> locations;
};

struct PlanItem {
  std::string content;
  PlanStatus status = PlanStatus::Pending;
};

enum class PermissionKind { AllowOnce, AllowAlways, RejectOnce, RejectAlways };
struct PermissionOption {
  std::string id;
  std::string name;
  PermissionKind kind = PermissionKind::AllowOnce;
};

// A question the agent asked before acting: it waits for one answer, an option or a cancellation.
class PermissionRequest {
 public:
  enum class State { Waiting, Answered, Cancelled };
  PermissionRequest() = default;
  PermissionRequest(Json request_id, ToolCallPatch tool, std::vector<PermissionOption> options)
      : request_id_(std::move(request_id)), tool_(std::move(tool)), options_(std::move(options)) {}

  // Chooses the option with `option_id`; false (nothing changes) when it is answered already or
  // has no such option.
  bool choose(const std::string& option_id);
  // Gives the question up (the turn was stopped); false when it was answered already.
  bool cancel();

  State state() const { return state_; }
  bool waiting() const { return state_ == State::Waiting; }
  const Json& request_id() const { return request_id_; }
  const ToolCallPatch& tool() const { return tool_; }
  const std::vector<PermissionOption>& options() const { return options_; }
  // The option chosen (nullptr unless answered).
  const PermissionOption* chosen() const;

 private:
  Json request_id_;
  ToolCallPatch tool_;
  std::vector<PermissionOption> options_;
  State state_ = State::Waiting;
  std::size_t chosen_ = 0;
};

enum class EntryKind { User, Agent, Thought, Tool, Plan, Permission, Notice, Error };

// One entry of the conversation: the fields its kind uses are set.
struct Entry {
  EntryKind kind = EntryKind::Notice;
  std::string text;              // User, Agent, Thought, Notice, Error
  ToolCall tool;                 // Tool
  std::vector<PlanItem> plan;    // Plan
  PermissionRequest permission;  // Permission
};

class Transcript {
 public:
  const std::vector<Entry>& entries() const { return entries_; }
  // Goes up with every change: whoever shows it knows when to look again.
  std::uint64_t revision() const { return revision_; }

  void add(EntryKind kind, const std::string& text);
  // A piece of the agent's answer or thought: it continues the last entry when that is of the same
  // kind, else starts a new one.
  void append(EntryKind kind, const std::string& piece);
  void start_tool(const ToolCall& call);
  // Changes the tool call `patch` names; a call never announced starts with what the patch says.
  void update_tool(const ToolCallPatch& patch);
  // The agent's whole plan as it is now: it replaces the one shown last.
  void set_plan(const std::vector<PlanItem>& plan);
  // A question of the agent's; its index in entries().
  std::size_t ask(const PermissionRequest& request);
  // The question at `index` (nullptr when that entry is none).
  PermissionRequest* permission(std::size_t index);
  // Every question still waiting, to give up when the turn stops.
  std::vector<std::size_t> waiting_permissions() const;
  void clear();

 private:
  Entry* find_tool(const std::string& id);
  void changed() { ++revision_; }

  std::vector<Entry> entries_;
  std::uint64_t revision_ = 0;
};

// The patch applied to a tool call.
void apply_patch(ToolCall& call, const ToolCallPatch& patch);

}  // namespace redgem::agent
