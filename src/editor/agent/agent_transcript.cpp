#include "editor/agent/agent_transcript.hpp"

#include <algorithm>

namespace redgem::agent {
namespace {

// Pieces of an answer go on the entry before them only while nothing else came between.
bool continues(const std::vector<Entry>& entries, EntryKind kind) {
  return !entries.empty() && entries.back().kind == kind;
}

bool waiting_question(const Entry& entry) { return entry.kind == EntryKind::Permission && entry.permission.waiting(); }

bool is_tool_call(const Entry& entry, const std::string& id) {
  return entry.kind == EntryKind::Tool && entry.tool.id == id;
}

// Whether a search for entry `number` found it.
bool is_entry(std::vector<Entry>::iterator found, const std::vector<Entry>& entries, std::size_t number) {
  return found != entries.end() && found->number == number;
}

template <typename Value>
void take(std::optional<Value> const& given, Value& field) {
  if (given) field = *given;
}

}  // namespace

bool PermissionRequest::choose(const std::string& option_id) {
  if (!waiting()) return false;
  const auto found = std::find_if(options_.begin(), options_.end(),
                                  [&](const PermissionOption& option) { return option.id == option_id; });
  if (found == options_.end()) return false;
  chosen_ = static_cast<std::size_t>(found - options_.begin());
  state_ = State::Answered;
  return true;
}

bool PermissionRequest::cancel() {
  if (!waiting()) return false;
  state_ = State::Cancelled;
  return true;
}

const PermissionOption* PermissionRequest::chosen() const {
  return state_ == State::Answered ? &options_[chosen_] : nullptr;
}

void apply_patch(ToolCall& call, const ToolCallPatch& patch) {
  take(patch.title, call.title);
  take(patch.kind, call.kind);
  take(patch.status, call.status);
  take(patch.content, call.content);
  take(patch.input, call.input);
  take(patch.locations, call.locations);
}

void Transcript::add(EntryKind kind, const std::string& text) {
  Entry entry;
  entry.kind = kind;
  entry.text = text;
  push(std::move(entry));
  changed();
}

void Transcript::append(EntryKind kind, const std::string& piece) {
  if (!continues(entries_, kind)) return add(kind, piece);
  entries_.back().text += piece;
  changed();
}

void Transcript::start_tool(const ToolCall& call) {
  if (Entry* known = find_tool(call.id)) {
    known->tool = call;
  } else {
    Entry entry;
    entry.kind = EntryKind::Tool;
    entry.tool = call;
    push(std::move(entry));
  }
  changed();
}

void Transcript::update_tool(const ToolCallPatch& patch) {
  Entry* known = find_tool(patch.id);
  if (known == nullptr) {
    ToolCall call;
    call.id = patch.id;
    apply_patch(call, patch);
    return start_tool(call);
  }
  apply_patch(known->tool, patch);
  changed();
}

void Transcript::set_plan(const std::vector<PlanItem>& plan) {
  const auto last = std::find_if(entries_.rbegin(), entries_.rend(),
                                 [](const Entry& entry) { return entry.kind == EntryKind::Plan; });
  if (last != entries_.rend()) {
    last->plan = plan;
  } else {
    Entry entry;
    entry.kind = EntryKind::Plan;
    entry.plan = plan;
    push(std::move(entry));
  }
  changed();
}

std::size_t Transcript::ask(const PermissionRequest& request) {
  Entry entry;
  entry.kind = EntryKind::Permission;
  entry.permission = request;
  push(std::move(entry));
  changed();
  return next_ - 1;
}

PermissionRequest* Transcript::permission(std::size_t number) {
  Entry* entry = numbered(number);
  if (entry == nullptr || entry->kind != EntryKind::Permission) return nullptr;
  changed();  // (asked for to be answered)
  return &entry->permission;
}

Entry* Transcript::numbered(std::size_t number) {
  const auto found = std::lower_bound(entries_.begin(), entries_.end(), number,
                                      [](const Entry& entry, std::size_t wanted) { return entry.number < wanted; });
  return is_entry(found, entries_, number) ? &*found : nullptr;
}

void Transcript::push(Entry entry) {
  entry.number = next_++;
  entries_.push_back(std::move(entry));
  if (entries_.size() > kMaxEntries) let_go_of_one();
}

// The oldest entry that is not a question still waiting.
void Transcript::let_go_of_one() {
  const auto oldest =
      std::find_if(entries_.begin(), entries_.end(), [](const Entry& entry) { return !waiting_question(entry); });
  if (oldest != entries_.end()) entries_.erase(oldest);
}

std::vector<std::size_t> Transcript::waiting_permissions() const {
  std::vector<std::size_t> waiting;
  for (const Entry& entry : entries_) {
    if (waiting_question(entry)) waiting.push_back(entry.number);
  }
  return waiting;
}

void Transcript::clear() {
  entries_.clear();
  changed();
}

Entry* Transcript::find_tool(const std::string& id) {
  const auto found =
      std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) { return is_tool_call(entry, id); });
  return found == entries_.end() ? nullptr : &*found;
}

}  // namespace redgem::agent
