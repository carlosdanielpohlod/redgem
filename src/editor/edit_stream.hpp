#pragma once

#include "editor/document/scene_document.hpp"

namespace redgem {

// Undo for edits that stream in while a widget is held (a drag, a slider, a color picker): one
// undo step per interaction with one object, taken just before its first change and dropped if
// the interaction ends where it started.
class EditStream {
 public:
  // Before each change an interaction makes to `object`: the first one records the undo step.
  // A change to another object (the selection moved while the widget was held) is an interaction
  // of its own, with its own step.
  void before_change(SceneDocument& doc, int object);
  // Once a frame, whatever is selected, with whether a widget is still held: the interaction
  // ends when none is.
  void end_frame(bool widget_held);
  // Whether an interaction is in progress.
  bool open() const { return doc_ != nullptr; }

 private:
  void close();
  // The interaction in progress is one with this object.
  bool continues(const SceneDocument& doc, int object) const { return doc_ == &doc && object_ == object; }

  SceneDocument* doc_ = nullptr;  // the document of the interaction in progress
  int object_ = kNoIndex;         // what it edits
};

}  // namespace redgem
