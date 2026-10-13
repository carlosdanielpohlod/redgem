#include "editor/edit_stream.hpp"

namespace redgem {

void EditStream::before_change(SceneDocument& doc, int object) {
  if (continues(doc, object)) return;
  close();
  doc.checkpoint();
  doc_ = &doc;
  object_ = object;
}

void EditStream::end_frame(bool widget_held) {
  if (!widget_held) close();
}

void EditStream::close() {
  if (doc_ == nullptr) return;
  doc_->drop_checkpoint_if_unchanged();
  doc_ = nullptr;
  object_ = kNoIndex;
}

}  // namespace redgem
