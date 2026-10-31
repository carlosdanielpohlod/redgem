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
#include "editor/agent/text_diff.hpp"

namespace redgem::agent {

enum class ToolStatus { Pending, InProgress, Completed, Failed };
enum class PlanStatus { Pending, InProgress, Completed };

// A change a tool made (or wants to make) to a file, as the lines it shows: worked out once when
// it came, and kept without the file's whole text before and after.
struct FileChange {
  std::string path;
  std::vector<DiffLine> lines;
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

// Other: a kind this editor does not know, shown as neither allowing nor rejecting.
enum class PermissionKind { AllowOnce, AllowAlways, RejectOnce, RejectAlways, Other };
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
  std::size_t number = 0;  // its place in the conversation (see Transcript)
  std::string message_id;  // the agent's id of the message it shows, when the agent gave one
  EntryKind kind = EntryKind::Notice;
  std::string text;              // User, Agent, Thought, Notice, Error
  ToolCall tool;                 // Tool
  std::vector<PlanItem> plan;    // Plan
  PermissionRequest permission;  // Permission
};

// The most entries a conversation keeps: the oldest go first (a question still waiting stays).
inline constexpr std::size_t kMaxEntries = 400;

// Entries are numbered as they come (Entry::number): a number stays its entry's own when older
// entries are let go, and is never given twice.
class Transcript {
 public:
  const std::vector<Entry>& entries() const { return entries_; }
  // The number the next entry gets.
  std::size_t end_number() const { return next_; }
  // Goes up with every change: whoever shows it knows when to look again.
  std::uint64_t revision() const { return revision_; }

  void add(EntryKind kind, const std::string& text);
  // A piece of the agent's answer or thought: it continues the last entry when that is of the same
  // kind, else starts a new one.
  void append(EntryKind kind, const std::string& piece);
  // A block of what the user said, as an agent replays a past conversation: it goes on the user's
  // entry before it when that is the same message (blocks apart by a blank line), else starts one.
  // Without ids, a block right after the user's last one is taken as part of it.
  void add_user_block(const std::string& block, const std::string& message_id);
  void start_tool(const ToolCall& call);
  // Changes the tool call `patch` names; a call never announced starts with what the patch says.
  void update_tool(const ToolCallPatch& patch);
  // The agent's whole plan as it is now: it replaces the one shown last.
  void set_plan(const std::vector<PlanItem>& plan);
  // A question of the agent's; its entry's number.
  std::size_t ask(const PermissionRequest& request);
  // The question numbered `number` (nullptr when that entry is none, or was let go).
  PermissionRequest* permission(std::size_t number);
  // Every question still waiting, to give up when the turn stops.
  std::vector<std::size_t> waiting_permissions() const;
  void clear();

 private:
  Entry* find_tool(const std::string& id);
  Entry* numbered(std::size_t number);
  void push(Entry entry);
  void let_go_of_one();
  void changed() { ++revision_; }

  std::vector<Entry> entries_;
  std::size_t next_ = 0;
  std::uint64_t revision_ = 0;
};

// The patch applied to a tool call.
void apply_patch(ToolCall& call, const ToolCallPatch& patch);

}  // namespace redgem::agent
