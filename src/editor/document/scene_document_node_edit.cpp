#include "editor/document/scene_document.hpp"

#include <stdexcept>
#include <utility>

namespace redgem {

class SceneDocument::NodeEdit {
 public:
  explicit NodeEdit(SceneDocument& document)
      : document_(document), before_(document.snapshot()), changed_(document.changed_since_checkpoint_) {
    if (document_.node_edit_active_) throw std::logic_error("nested node edit");
    document_.node_edit_active_ = true;
    document_.changed_since_checkpoint_ = false;
  }
  ~NodeEdit() {
    document_.node_edit_active_ = false;
    if (committed_) return;
    document_.restore(before_);
    document_.changed_since_checkpoint_ = changed_;
  }
  NodeEdit(const NodeEdit&) = delete;
  NodeEdit& operator=(const NodeEdit&) = delete;

  bool finish(const std::string& error) {
    if (!error.empty()) return false;
    if (same_content(before_, document_.snapshot())) return true;
    document_.push_step(document_.undo_, std::move(before_));
    document_.clear_redo();
    document_.trim_history();
    committed_ = true;
    return true;
  }

 private:
  SceneDocument& document_;
  Snapshot before_;
  bool changed_;
  bool committed_ = false;
};

bool SceneDocument::edit_nodes(const std::function<std::string()>& edit, std::string& error) {
  NodeEdit group(*this);
  error = edit();
  return group.finish(error);
}
namespace {
// Keeps a document's own checkpoints from recording while it lives: edits join the step before.
class JoinedEdits {
 public:
  explicit JoinedEdits(bool& active) : active_(active) {
    if (active_) throw std::logic_error("nested node edit");
    active_ = true;
  }
  ~JoinedEdits() { active_ = false; }
  JoinedEdits(const JoinedEdits&) = delete;
  JoinedEdits& operator=(const JoinedEdits&) = delete;

 private:
  bool& active_;
};
}  // namespace

bool SceneDocument::make_then_edit(const std::function<void()>& make, const std::function<std::string()>& edit,
                                   std::string& error) {
  make();
  try {
    JoinedEdits joined(node_edit_active_);
    error = edit();
  } catch (...) {
    take_back_last_step();
    throw;
  }
  if (!error.empty()) take_back_last_step();
  return error.empty();
}

void SceneDocument::take_back_last_step() {
  if (!undo()) return;
  pop_step(redo_);
}

}  // namespace redgem
