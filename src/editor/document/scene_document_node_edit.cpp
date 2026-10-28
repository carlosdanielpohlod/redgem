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

bool SceneDocument::edit_existing_nodes(const std::function<std::string()>& edit, std::string& error) {
  NodeEdit group(*this);
  error = edit();
  return group.finish(error);
}
}  // namespace redgem
